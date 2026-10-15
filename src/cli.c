#include "cli.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifndef _WIN32
  #include <strings.h>
#endif

/* Thread-safe Command Queue Implementation */

void CommandQueue_Init(CommandQueue *q)
{
    if (!q || q->initialized)
        return;

    memset(q->items, 0, sizeof(q->items));
    q->head = 0;
    q->tail = 0;
    q->count = 0;
#ifdef _WIN32
    InitializeCriticalSection(&q->lock);
#else
    pthread_mutex_init(&q->lock, NULL);
#endif
    q->initialized = true;
}

void CommandQueue_Destroy(CommandQueue *q)
{
    if (!q || !q->initialized)
        return;

#ifdef _WIN32
    DeleteCriticalSection(&q->lock);
#else
    pthread_mutex_destroy(&q->lock);
#endif
    q->initialized = false;
    q->count = 0;
    q->head = 0;
    q->tail = 0;
}

bool CommandQueue_Push(CommandQueue *q, const char *cmd)
{
    if (!q || !cmd || !q->initialized)
        return false;

#ifdef _WIN32
    EnterCriticalSection(&q->lock);
#else
    pthread_mutex_lock(&q->lock);
#endif

    if (q->count >= CLI_QUEUE_CAPACITY) {
#ifdef _WIN32
        LeaveCriticalSection(&q->lock);
#else
        pthread_mutex_unlock(&q->lock);
#endif
        return false;
    }

    strncpy(q->items[q->tail], cmd, CLI_MAX_CMD_LEN - 1);
    q->items[q->tail][CLI_MAX_CMD_LEN - 1] = '\0';
    q->tail = (q->tail + 1) % CLI_QUEUE_CAPACITY;
    q->count++;

#ifdef _WIN32
    LeaveCriticalSection(&q->lock);
#else
    pthread_mutex_unlock(&q->lock);
#endif

    return true;
}

bool CommandQueue_Pop(CommandQueue *q, char *out_cmd, size_t max_len)
{
    if (!q || !out_cmd || max_len == 0 || !q->initialized)
        return false;

#ifdef _WIN32
    EnterCriticalSection(&q->lock);
#else
    pthread_mutex_lock(&q->lock);
#endif

    if (q->count == 0) {
#ifdef _WIN32
        LeaveCriticalSection(&q->lock);
#else
        pthread_mutex_unlock(&q->lock);
#endif
        return false;
    }

    strncpy(out_cmd, q->items[q->head], max_len - 1);
    out_cmd[max_len - 1] = '\0';
    q->head = (q->head + 1) % CLI_QUEUE_CAPACITY;
    q->count--;

#ifdef _WIN32
    LeaveCriticalSection(&q->lock);
#else
    pthread_mutex_unlock(&q->lock);
#endif

    return true;
}

int CommandQueue_Count(CommandQueue *q)
{
    if (!q || !q->initialized)
        return 0;

    int res = 0;
#ifdef _WIN32
    EnterCriticalSection(&q->lock);
    res = q->count;
    LeaveCriticalSection(&q->lock);
#else
    pthread_mutex_lock(&q->lock);
    res = q->count;
    pthread_mutex_unlock(&q->lock);
#endif
    return res;
}

/* Background CLI Input Thread */

#ifdef _WIN32
static DWORD WINAPI CLI_InputThread(LPVOID lpParam)
#else
static void *CLI_InputThread(void *lpParam)
#endif
{
    Connection *c = (Connection *)lpParam;
    char buffer[CLI_MAX_CMD_LEN];

    while (c && c->running) {
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            break;
        }

        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
            buffer[--len] = '\0';
        }

        if (len > 0) {
            CommandQueue_Push(&c->cmd_queue, buffer);
        }
    }

    return 0;
}

/* CLI Management API */

bool CLI_Init(Connection *c)
{
    if (!c)
        return false;

    CommandQueue_Init(&c->cmd_queue);
    c->running = true;

#ifdef _WIN32
    c->cli_thread = CreateThread(NULL, 0, CLI_InputThread, c, 0, NULL);
    if (!c->cli_thread) {
        LOGE("Failed to create CLI input thread\n");
        return false;
    }
#else
    if (pthread_create(&c->cli_thread, NULL, CLI_InputThread, c) != 0) {
        LOGE("Failed to create CLI input thread\n");
        return false;
    }
#endif

    return true;
}

void CLI_Cleanup(Connection *c)
{
    if (!c)
        return;

    c->running = false;

#ifdef _WIN32
    if (c->cli_thread) {
        CancelSynchronousIo(c->cli_thread);
        if (WaitForSingleObject(c->cli_thread, 50) == WAIT_TIMEOUT) {
            TerminateThread(c->cli_thread, 0);
        }
        CloseHandle(c->cli_thread);
        c->cli_thread = NULL;
    }
#else
    if (c->cli_thread) {
        pthread_cancel(c->cli_thread);
        pthread_join(c->cli_thread, NULL);
        c->cli_thread = 0;
    }
#endif

    CommandQueue_Destroy(&c->cmd_queue);
}

void CLI_Tick(Connection *c)
{
    if (!c || !c->running)
        return;

    char cmd[CLI_MAX_CMD_LEN];
    int max_cmds = 16;
    while (c->running && max_cmds-- > 0 && CommandQueue_Pop(&c->cmd_queue, cmd, sizeof(cmd))) {
        CLI_ProcessInput(c, cmd);
    }
}

void CLI_ShowStatus(Connection *c)
{
    if (!c)
        return;

    char line_buf[128];
    const char *conn_status = (c->sock >= 0) ? "CONNECTED" : "DISCONNECTED";
    const char *mode_str = (c->mode == BOT_MODE_HARVEST)
                               ? "HARVEST [Resource Collection / Sheltered]"
                               : "NORMAL [Daily Protection / Farm]";

    char shield_status[64];
    if (c->shield_info.active) {
        uint64_t end_time = c->shield_info.begin_time + c->shield_info.duration;
        uint32_t rem = (end_time > c->server_time) ? (uint32_t)(end_time - c->server_time) : 0;
        uint32_t h = rem / 3600;
        uint32_t m = (rem % 3600) / 60;
        uint32_t s = rem % 60;
        snprintf(shield_status, sizeof(shield_status), "ACTIVE (%02uh %02um %02us remaining)", h, m, s);
    } else {
        snprintf(shield_status, sizeof(shield_status), "INACTIVE");
    }

    const char *auto_shield = c->protection.shield_always_on
                                  ? "24/7 Forced Active"
                                  : (c->protection.enabled ? "Threat Triggered" : "Disabled");

    printf("+------------------------------------------------------------------------------+\n");
    printf("|                             LORDS MOBILE BOT                                 |\n");
    printf("|                             STATUS DASHBOARD                                 |\n");
    printf("+------------------------------------------------------------------------------+\n");
    snprintf(line_buf, sizeof(line_buf), "Connection   : %s", conn_status);
    printf("| %-76.76s |\n", line_buf);

    if (c->player.name[0] != '\0') {
        snprintf(line_buf, sizeof(line_buf), "Player       : %s", c->player.name);
        printf("| %-76.76s |\n", line_buf);
        snprintf(line_buf, sizeof(line_buf), "Might / Gems : Might: %llu, Gems: %u",
                 (unsigned long long)c->player.power, c->player.gems);
        printf("| %-76.76s |\n", line_buf);
    }

    snprintf(line_buf, sizeof(line_buf), "Bot Mode     : %s", mode_str);
    printf("| %-76.76s |\n", line_buf);
    printf("+------------------------------------------------------------------------------+\n");
    snprintf(line_buf, sizeof(line_buf), "Shield       : %s", shield_status);
    printf("| %-76.76s |\n", line_buf);
    snprintf(line_buf, sizeof(line_buf), "Auto-Shield  : %s", auto_shield);
    printf("| %-76.76s |\n", line_buf);
    printf("+------------------------------------------------------------------------------+\n");
    snprintf(line_buf, sizeof(line_buf), "Whitelist    : %u member(s)", c->whitelist_count);
    printf("| %-76.76s |\n", line_buf);

    if (c->whitelist_count == 0) {
        printf("| %-76.76s |\n", "  (None)");
    } else {
        for (uint8_t i = 0; i < c->whitelist_count; i++) {
            snprintf(line_buf, sizeof(line_buf), "  - %s", c->whitelist[i]);
            printf("| %-76.76s |\n", line_buf);
        }
    }
    printf("+------------------------------------------------------------------------------+\n");
    printf("| %-76.76s |\n", "Resources:");
    snprintf(line_buf, sizeof(line_buf), "  Food: %-12u Rock: %-12u Wood: %-12u",
             c->resources.food, c->resources.rock, c->resources.wood);
    printf("| %-76.76s |\n", line_buf);
    snprintf(line_buf, sizeof(line_buf), "  Ore : %-12u Gold: %-12u",
             c->resources.ore, c->resources.gold);
    printf("| %-76.76s |\n", line_buf);
    printf("+------------------------------------------------------------------------------+\n");
}

void CLI_ProcessInput(Connection *c, const char *line)
{
    if (!c || !line)
        return;

    char buf[CLI_MAX_CMD_LEN];
    char line_buf[128];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *trimmed = TrimWhitespace(buf);
    if (!trimmed || trimmed[0] == '\0')
        return;

    char *cmd = trimmed;
    char *arg = NULL;
    char *space = strchr(trimmed, ' ');
    if (!space)
        space = strchr(trimmed, '\t');

    if (space) {
        *space = '\0';
        arg = TrimWhitespace(space + 1);
    }

#ifdef _WIN32
    #define STR_CMP _stricmp
#else
    #define STR_CMP strcasecmp
#endif

    if (STR_CMP(cmd, "help") == 0) {
        printf("+------------------------------------------------------------------------------+\n");
        printf("|                            AVAILABLE CLI COMMANDS                            |\n");
        printf("+------------------------------------------------------------------------------+\n");
        printf("| help             : Display command help list                                 |\n");
        printf("| status           : Display structured ASCII status dashboard                 |\n");
        printf("| harvest <on|off> : Toggle harvest mode (shelter army / resource collection)  |\n");
        printf("| shield <on|off>  : Toggle 24/7 shield protection                             |\n");
        printf("| reload           : Hot reload config file without reconnecting               |\n");
        printf("| quit / exit      : Cleanly disconnect and exit application                   |\n");
        printf("+------------------------------------------------------------------------------+\n");
    }
    else if (STR_CMP(cmd, "status") == 0) {
        CLI_ShowStatus(c);
    }
    else if (STR_CMP(cmd, "harvest") == 0) {
        if (!arg || arg[0] == '\0') {
            printf("+------------------------------------------------------------------------------+\n");
            printf("| %-76.76s |\n", "Usage: harvest <on|off>");
            snprintf(line_buf, sizeof(line_buf), "Current mode: %s", (c->mode == BOT_MODE_HARVEST) ? "HARVEST" : "NORMAL");
            printf("| %-76.76s |\n", line_buf);
            printf("+------------------------------------------------------------------------------+\n");
        } else if (STR_CMP(arg, "on") == 0) {
            c->mode = BOT_MODE_HARVEST;
            printf("+------------------------------------------------------------------------------+\n");
            printf("| %-76.76s |\n", "[MODE] Harvest mode ENABLED (Army shelter / Ready for harvesting)");
            printf("+------------------------------------------------------------------------------+\n");
        } else if (STR_CMP(arg, "off") == 0) {
            c->mode = BOT_MODE_NORMAL;
            printf("+------------------------------------------------------------------------------+\n");
            printf("| %-76.76s |\n", "[MODE] Normal mode ENABLED (Daily farm / Shield protection active)");
            printf("+------------------------------------------------------------------------------+\n");
        } else {
            printf("+------------------------------------------------------------------------------+\n");
            snprintf(line_buf, sizeof(line_buf), "[ERROR] Invalid harvest param '%s'. Usage: harvest <on|off>", arg);
            printf("| %-76.76s |\n", line_buf);
            printf("+------------------------------------------------------------------------------+\n");
        }
    }
    else if (STR_CMP(cmd, "shield") == 0) {
        if (!arg || arg[0] == '\0') {
            printf("+------------------------------------------------------------------------------+\n");
            printf("| %-76.76s |\n", "Usage: shield <on|off>");
            snprintf(line_buf, sizeof(line_buf), "Current 24/7 Shield: %s", c->protection.shield_always_on ? "ENABLED" : "DISABLED");
            printf("| %-76.76s |\n", line_buf);
            printf("+------------------------------------------------------------------------------+\n");
        } else if (STR_CMP(arg, "on") == 0) {
            c->protection.enabled = true;
            c->protection.shield_always_on = true;
            printf("+------------------------------------------------------------------------------+\n");
            printf("| %-76.76s |\n", "[SHIELD] 24/7 Shield protection ENABLED");
            printf("+------------------------------------------------------------------------------+\n");
        } else if (STR_CMP(arg, "off") == 0) {
            c->protection.shield_always_on = false;
            printf("+------------------------------------------------------------------------------+\n");
            printf("| %-76.76s |\n", "[SHIELD] 24/7 Shield protection DISABLED");
            printf("+------------------------------------------------------------------------------+\n");
        } else {
            printf("+------------------------------------------------------------------------------+\n");
            snprintf(line_buf, sizeof(line_buf), "[ERROR] Invalid shield param '%s'. Usage: shield <on|off>", arg);
            printf("| %-76.76s |\n", line_buf);
            printf("+------------------------------------------------------------------------------+\n");
        }
    }
    else if (STR_CMP(cmd, "reload") == 0) {
        const char *path = c->config_path[0] ? c->config_path : "config.cfg";
        printf("[CONFIG] Reloading configuration from '%s'...\n", path);
        if (ReloadConfig(c, path)) {
            printf("+------------------------------------------------------------------------------+\n");
            printf("| %-76.76s |\n", "[CONFIG] Configuration reloaded successfully.");
            snprintf(line_buf, sizeof(line_buf), "Whitelist count: %u", c->whitelist_count);
            printf("| %-76.76s |\n", line_buf);
            printf("+------------------------------------------------------------------------------+\n");
        } else {
            printf("+------------------------------------------------------------------------------+\n");
            printf("| %-76.76s |\n", "[ERROR] Failed to reload configuration. Previous settings retained.");
            printf("+------------------------------------------------------------------------------+\n");
        }
    }
    else if (STR_CMP(cmd, "quit") == 0 || STR_CMP(cmd, "exit") == 0) {
        c->running = false;
        printf("+------------------------------------------------------------------------------+\n");
        printf("| %-76.76s |\n", "[EXIT] Clean shutdown requested. Terminating bot execution...");
        printf("+------------------------------------------------------------------------------+\n");
    }
    else {
        printf("[CLI] Unknown command: '%s'. Type 'help' for available commands.\n", cmd);
    }

#undef STR_CMP
}
