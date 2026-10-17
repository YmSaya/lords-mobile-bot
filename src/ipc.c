#include "ipc.h"
#include "log.h"
#include "config.h"
#include "protocol.h"
#include "updater.h"
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
  #ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0601
  #endif
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #define STR_CASE_CMP _stricmp
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <strings.h>
  #define STR_CASE_CMP strcasecmp
#endif

static bool Json_ExtractString(const char *json, const char *key, char *out, size_t max_out)
{
    if (!json || !key || !out || max_out == 0)
        return false;

    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\"", key);

    const char *pos = strstr(json, search_pattern);
    if (!pos)
        return false;

    pos += strlen(search_pattern);
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == ':'))
        pos++;

    if (*pos == '\"') {
        pos++;
        size_t idx = 0;
        while (*pos && *pos != '\"' && idx + 1 < max_out) {
            out[idx++] = *pos++;
        }
        out[idx] = '\0';
        return true;
    }

    size_t idx = 0;
    while (*pos && *pos != ',' && *pos != '}' && !isspace((unsigned char)*pos) && idx + 1 < max_out) {
        out[idx++] = *pos++;
    }
    out[idx] = '\0';
    return (idx > 0);
}

bool IPC_ProcessCommand(Connection *c, const char *json_in, char *json_out, size_t max_out_len)
{
    if (!c || !json_in || !json_out || max_out_len == 0)
        return false;

    char cmd[64] = {0};
    char action[32] = {0};

    /* Try extracting from JSON: {"cmd": "status"} */
    if (!Json_ExtractString(json_in, "cmd", cmd, sizeof(cmd))) {
        /* Fallback: parse plain text line (e.g. "status", "harvest on") */
        char temp_line[256];
        strncpy(temp_line, json_in, sizeof(temp_line) - 1);
        temp_line[sizeof(temp_line) - 1] = '\0';
        char *trimmed = TrimWhitespace(temp_line);
        if (trimmed && trimmed[0] != '\0') {
            char *space = strchr(trimmed, ' ');
            if (space) {
                *space = '\0';
                strncpy(cmd, trimmed, sizeof(cmd) - 1);
                char *arg = TrimWhitespace(space + 1);
                if (arg) {
                    strncpy(action, arg, sizeof(action) - 1);
                }
            } else {
                strncpy(cmd, trimmed, sizeof(cmd) - 1);
            }
        }
    } else {
        /* If JSON cmd was found, also extract optional action parameter */
        Json_ExtractString(json_in, "action", action, sizeof(action));
    }

    /* Support compound command in cmd field, e.g. {"cmd":"harvest on"} */
    char *internal_space = strchr(cmd, ' ');
    if (internal_space) {
        *internal_space = '\0';
        if (action[0] == '\0') {
            char *arg = TrimWhitespace(internal_space + 1);
            if (arg) {
                strncpy(action, arg, sizeof(action) - 1);
                action[sizeof(action) - 1] = '\0';
            }
        }
    }

    if (cmd[0] == '\0') {
        snprintf(json_out, max_out_len, "{\"status\":\"error\",\"message\":\"Empty command received.\"}");
        return true;
    }

    if (STR_CASE_CMP(cmd, "status") == 0) {
        uint32_t shield_remaining = 0;
        if (c->shield_info.active) {
            uint64_t now = c->server_time ? c->server_time : (uint64_t)time(NULL);
            uint64_t end_time = c->shield_info.begin_time + c->shield_info.duration;
            if (end_time > now)
                shield_remaining = (uint32_t)(end_time - now);
        }

        const char *conn_status = (c->sock >= 0) ? "CONNECTED" : "DISCONNECTED";
        const char *mode_str = (c->mode == BOT_MODE_HARVEST) ? "HARVEST" : "NORMAL";
        const char *auto_shield = c->protection.shield_always_on
                                      ? "24/7 Forced Active"
                                      : (c->protection.enabled ? "Threat Triggered" : "Disabled");

        snprintf(json_out, max_out_len,
                 "{\"status\":\"ok\",\"type\":\"status\",\"data\":{"
                 "\"version\":\"%s\","
                 "\"connection\":\"%s\","
                 "\"player\":\"%s\","
                 "\"power\":%llu,"
                 "\"gems\":%u,"
                 "\"mode\":\"%s\","
                 "\"sheltered\":%s,"
                 "\"gather_enabled\":%s,"
                 "\"gather_paused\":%s,"
                 "\"shield_active\":%s,"
                 "\"shield_remaining\":%u,"
                 "\"shield_alert\":%s,"
                 "\"auto_shield\":\"%s\","
                 "\"resources\":{\"food\":%u,\"rock\":%u,\"wood\":%u,\"ore\":%u,\"gold\":%u},"
                 "\"whitelist_count\":%u,"
                 "\"current_marches\":%u,"
                 "\"max_marches\":%u"
                 "}}",
                 VERSION,
                 conn_status,
                 c->player.name[0] ? c->player.name : "Unknown",
                 (unsigned long long)c->player.power,
                 c->player.gems,
                 mode_str,
                 c->troops_in_shelter ? "true" : "false",
                 c->gather_enabled ? "true" : "false",
                 (c->gather_paused || c->mode == BOT_MODE_HARVEST) ? "true" : "false",
                 c->shield_info.active ? "true" : "false",
                 shield_remaining,
                 c->shield_alert_24h_active ? "true" : "false",
                 auto_shield,
                 c->resources.food, c->resources.rock, c->resources.wood, c->resources.ore, c->resources.gold,
                 c->whitelist_count,
                 c->player.current_marches,
                 c->player.max_marches);
        return true;
    }
    else if (STR_CASE_CMP(cmd, "harvest") == 0) {
        if (STR_CASE_CMP(action, "on") == 0) {
            c->mode = BOT_MODE_HARVEST;
            c->gather_paused = true;
            RequestHideTroopInShelter(c, 12, true, NULL);
            snprintf(json_out, max_out_len,
                     "{\"status\":\"ok\",\"message\":\"Harvest mode enabled. Army sheltered for 12 hours (0 garrison).\"}");
            return true;
        } else if (STR_CASE_CMP(action, "off") == 0) {
            c->mode = BOT_MODE_NORMAL;
            c->gather_paused = false;
            RequestReleaseShelterTroop(c);
            SelectAndUseShield(c);
            snprintf(json_out, max_out_len,
                     "{\"status\":\"ok\",\"message\":\"Harvest mode disabled. Army released and shield protection restored.\"}");
            return true;
        } else {
            snprintf(json_out, max_out_len,
                     "{\"status\":\"error\",\"message\":\"Invalid harvest action '%s'. Usage: harvest on|off\"}",
                     action);
            return true;
        }
    }
    else if (STR_CASE_CMP(cmd, "shield") == 0) {
        if (STR_CASE_CMP(action, "on") == 0) {
            c->protection.enabled = true;
            c->protection.shield_always_on = true;
            if (c->mode != BOT_MODE_HARVEST && !c->shield_info.active) {
                SelectAndUseShield(c);
            }
            snprintf(json_out, max_out_len,
                     "{\"status\":\"ok\",\"message\":\"24/7 Shield protection enabled.\"}");
            return true;
        } else if (STR_CASE_CMP(action, "off") == 0) {
            c->protection.shield_always_on = false;
            snprintf(json_out, max_out_len,
                     "{\"status\":\"ok\",\"message\":\"24/7 Shield protection disabled.\"}");
            return true;
        } else {
            snprintf(json_out, max_out_len,
                     "{\"status\":\"error\",\"message\":\"Invalid shield action '%s'. Usage: shield on|off\"}",
                     action);
            return true;
        }
    }
    else if (STR_CASE_CMP(cmd, "gather") == 0) {
        if (STR_CASE_CMP(action, "on") == 0) {
            c->gather_enabled = true;
            if (c->mode != BOT_MODE_HARVEST) {
                c->gather_paused = false;
            }
            snprintf(json_out, max_out_len,
                     "{\"status\":\"ok\",\"message\":\"Auto gathering enabled.\"}");
            return true;
        } else if (STR_CASE_CMP(action, "off") == 0) {
            c->gather_enabled = false;
            snprintf(json_out, max_out_len,
                     "{\"status\":\"ok\",\"message\":\"Auto gathering disabled.\"}");
            return true;
        } else {
            snprintf(json_out, max_out_len,
                     "{\"status\":\"error\",\"message\":\"Invalid gather action '%s'. Usage: gather on|off\"}",
                     action);
            return true;
        }
    }
    else if (STR_CASE_CMP(cmd, "reload") == 0) {
        const char *path = c->config_path[0] ? c->config_path : "config.cfg";
        if (ReloadConfig(c, path)) {
            snprintf(json_out, max_out_len,
                     "{\"status\":\"ok\",\"message\":\"Configuration reloaded successfully.\"}");
        } else {
            snprintf(json_out, max_out_len,
                     "{\"status\":\"error\",\"message\":\"Failed to reload configuration from file.\"}");
        }
        return true;
    }
    else if (STR_CASE_CMP(cmd, "update") == 0) {
        char msg[256] = {0};
        bool ok = Updater_ExecuteUpdate(c, msg, sizeof(msg));
        snprintf(json_out, max_out_len,
                 "{\"status\":\"%s\",\"message\":\"%s\"}",
                 ok ? "ok" : "error", msg);
        return true;
    }
    else if (STR_CASE_CMP(cmd, "help") == 0) {
        snprintf(json_out, max_out_len,
                 "{\"status\":\"ok\",\"message\":\"Available commands: status, harvest <on|off>, shield <on|off>, gather <on|off>, reload, update, help\"}");
        return true;
    }
    else {
        snprintf(json_out, max_out_len,
                 "{\"status\":\"error\",\"message\":\"Unknown command '%s'.\"}", cmd);
        return true;
    }
}

#ifdef _WIN32
static DWORD WINAPI IPC_ServerThread(LPVOID lpParam)
#else
static void *IPC_ServerThread(void *lpParam)
#endif
{
    Connection *c = (Connection *)lpParam;
    if (!c)
        return 0;

#ifdef _WIN32
    while (c->running && c->ipc_listen_sock != INVALID_SOCKET) {
#else
    while (c->running && c->ipc_listen_sock >= 0) {
#endif
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(c->ipc_listen_sock, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; /* 100ms timeout for non-blocking loop check */

#ifdef _WIN32
        int sel = select(0, &readfds, NULL, NULL, &tv);
#else
        int sel = select(c->ipc_listen_sock + 1, &readfds, NULL, NULL, &tv);
#endif
        if (sel <= 0) {
            continue;
        }

        struct sockaddr_in client_addr;
#ifdef _WIN32
        int addrlen = sizeof(client_addr);
        SOCKET client_sock = accept(c->ipc_listen_sock, (struct sockaddr *)&client_addr, &addrlen);
        if (client_sock == INVALID_SOCKET) {
            continue;
        }
#else
        socklen_t addrlen = sizeof(client_addr);
        int client_sock = accept(c->ipc_listen_sock, (struct sockaddr *)&client_addr, &addrlen);
        if (client_sock < 0) {
            continue;
        }
#endif

        char buffer[IPC_MAX_MSG_LEN];
        int total_read = 0;

        while (c->running && total_read + 1 < (int)sizeof(buffer)) {
            fd_set c_rfds;
            FD_ZERO(&c_rfds);
            FD_SET(client_sock, &c_rfds);
            struct timeval c_tv = { .tv_sec = 1, .tv_usec = 0 };

#ifdef _WIN32
            int c_sel = select(0, &c_rfds, NULL, NULL, &c_tv);
#else
            int c_sel = select(client_sock + 1, &c_rfds, NULL, NULL, &c_tv);
#endif
            if (c_sel <= 0) {
                break;
            }

            int n = recv(client_sock, buffer + total_read, (int)(sizeof(buffer) - 1 - total_read), 0);
            if (n <= 0) break;
            total_read += n;
            buffer[total_read] = '\0';
            if (strchr(buffer, '\n')) break;
        }

        if (total_read > 0) {
            char *newline = strchr(buffer, '\n');
            if (newline) *newline = '\0';
            char *carriage = strchr(buffer, '\r');
            if (carriage) *carriage = '\0';

            char resp[IPC_MAX_MSG_LEN];
            IPC_ProcessCommand(c, buffer, resp, sizeof(resp) - 2);

            size_t rlen = strlen(resp);
            resp[rlen++] = '\n';
            resp[rlen] = '\0';

            send(client_sock, resp, (int)rlen, 0);
        }

#ifdef _WIN32
        closesocket(client_sock);
#else
        close(client_sock);
#endif
    }

    return 0;
}

bool IPC_Init(Connection *c, uint16_t port)
{
    if (!c)
        return false;

    if (port == 0)
        port = IPC_DEFAULT_PORT;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
    if (sock == INVALID_SOCKET) {
        LOGE("Failed to create IPC socket\n");
        return false;
    }
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    if (sock < 0) {
        LOGE("Failed to create IPC socket\n");
        return false;
    }
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* Loopback strictly: 127.0.0.1 */

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        LOGE("Failed to bind IPC socket to 127.0.0.1:%u\n", port);
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return false;
    }

    if (listen(sock, 5) != 0) {
        LOGE("Failed to listen on IPC socket\n");
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return false;
    }

    c->ipc_listen_sock = sock;

#ifdef _WIN32
    c->ipc_thread = CreateThread(NULL, 0, IPC_ServerThread, c, 0, NULL);
    if (!c->ipc_thread) {
        LOGE("Failed to create IPC thread\n");
        closesocket(sock);
        c->ipc_listen_sock = INVALID_SOCKET;
        return false;
    }
#else
    if (pthread_create(&c->ipc_thread, NULL, IPC_ServerThread, c) != 0) {
        LOGE("Failed to create IPC thread\n");
        close(sock);
        c->ipc_listen_sock = -1;
        return false;
    }
#endif

    LOGI("IPC Server listening on 127.0.0.1:%u\n", port);
    return true;
}

void IPC_Cleanup(Connection *c)
{
    if (!c)
        return;

#ifdef _WIN32
    if (c->ipc_listen_sock != INVALID_SOCKET) {
        closesocket(c->ipc_listen_sock);
        c->ipc_listen_sock = INVALID_SOCKET;
    }
    if (c->ipc_thread) {
        WaitForSingleObject(c->ipc_thread, 500);
        CloseHandle(c->ipc_thread);
        c->ipc_thread = NULL;
    }
#else
    if (c->ipc_listen_sock >= 0) {
        close(c->ipc_listen_sock);
        c->ipc_listen_sock = -1;
    }
    if (c->ipc_thread) {
        pthread_join(c->ipc_thread, NULL);
        c->ipc_thread = 0;
    }
#endif
}
