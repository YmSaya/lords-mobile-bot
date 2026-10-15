#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NDEBUG
  #undef NDEBUG
#endif
#include <assert.h>

#include "connection.h"
#include "config.h"
#include "cli.h"

#define TEST_CFG_INIT "test_phase2_init.cfg"
#define TEST_CFG_RELOAD "test_phase2_reload.cfg"
#define TEST_CFG_CORRUPT "test_phase2_corrupt.cfg"

/* 1. Whitelist Parsing and Space Trimming Tests */
static void test_whitelist_parsing(void)
{
    printf("[TEST] Testing whitelist parsing and space handling...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);

    /* Test TrimWhitespace basic and edge cases */
    char t1[] = "   King Arthur   ";
    char *res1 = TrimWhitespace(t1);
    assert(strcmp(res1, "King Arthur") == 0);

    char t2[] = "My Farm 01";
    char *res2 = TrimWhitespace(t2);
    assert(strcmp(res2, "My Farm 01") == 0);

    char t3[] = "   Multiple   Internal   Spaces   ";
    char *res3 = TrimWhitespace(t3);
    assert(strcmp(res3, "Multiple   Internal   Spaces") == 0);

    char t4[] = "   ";
    char *res4 = TrimWhitespace(t4);
    assert(strcmp(res4, "") == 0);

    char t5[] = "";
    char *res5 = TrimWhitespace(t5);
    assert(strcmp(res5, "") == 0);

    assert(TrimWhitespace(NULL) == NULL);

    /* Test ParseWhitelist with standard input */
    bool ok = ParseWhitelist(c, "King Arthur, My Farm 01, Player Three,  Castle Guard  ");
    assert(ok == true);
    assert(c->whitelist_count == 4);
    assert(strcmp(c->whitelist[0], "King Arthur") == 0);
    assert(strcmp(c->whitelist[1], "My Farm 01") == 0);
    assert(strcmp(c->whitelist[2], "Player Three") == 0);
    assert(strcmp(c->whitelist[3], "Castle Guard") == 0);

    /* Test ParseWhitelist with multiple internal spaces and commas */
    ok = ParseWhitelist(c, "   ,  , Alpha   Beta ,   Gamma  ");
    assert(ok == true);
    assert(c->whitelist_count == 2);
    assert(strcmp(c->whitelist[0], "Alpha   Beta") == 0);
    assert(strcmp(c->whitelist[1], "Gamma") == 0);

    /* Test ParseWhitelist with empty string */
    ok = ParseWhitelist(c, "");
    assert(ok == true);
    assert(c->whitelist_count == 0);

    /* Test ParseWhitelist with NULL value */
    ok = ParseWhitelist(c, NULL);
    assert(ok == true);
    assert(c->whitelist_count == 0);

    /* Test ParseWhitelist capacity capping at MAX_WHITELIST_PLAYERS (16) */
    const char *twenty_players = "P1,P2,P3,P4,P5,P6,P7,P8,P9,P10,P11,P12,P13,P14,P15,P16,P17,P18,P19,P20";
    ok = ParseWhitelist(c, twenty_players);
    assert(ok == true);
    assert(c->whitelist_count == MAX_WHITELIST_PLAYERS);
    assert(strcmp(c->whitelist[0], "P1") == 0);
    assert(strcmp(c->whitelist[15], "P16") == 0);

    /* Test ParseWhitelist with long leading whitespace after commas (exceeding token capacity) */
    ok = ParseWhitelist(c, "Player One,                                  Player Two");
    assert(ok == true);
    assert(c->whitelist_count == 2);
    assert(strcmp(c->whitelist[0], "Player One") == 0);
    assert(strcmp(c->whitelist[1], "Player Two") == 0);

    /* Test ParseWhitelist with long trailing whitespace before commas */
    ok = ParseWhitelist(c, "Leader                                  , Guard");
    assert(ok == true);
    assert(c->whitelist_count == 2);
    assert(strcmp(c->whitelist[0], "Leader") == 0);
    assert(strcmp(c->whitelist[1], "Guard") == 0);

    /* Test ParseWhitelist with tabs as whitespace */
    ok = ParseWhitelist(c, "\tKing Arthur\t,\tMy Farm 01\t");
    assert(ok == true);
    assert(c->whitelist_count == 2);
    assert(strcmp(c->whitelist[0], "King Arthur") == 0);
    assert(strcmp(c->whitelist[1], "My Farm 01") == 0);

    /* Test ParseWhitelist with consecutive commas and only whitespace */
    ok = ParseWhitelist(c, "P1,,,   ,P2");
    assert(ok == true);
    assert(c->whitelist_count == 2);
    assert(strcmp(c->whitelist[0], "P1") == 0);
    assert(strcmp(c->whitelist[1], "P2") == 0);

    /* Test ParseWhitelist with name exceeding MAX_PLAYER_NAME_LEN (32 bytes) */
    const char *long_name_input = "ThisIsAVeryLongPlayerNameThatExceedsThirtyOneCharacters, NormalPlayer";
    ok = ParseWhitelist(c, long_name_input);
    assert(ok == true);
    assert(c->whitelist_count == 2);
    assert(strlen(c->whitelist[0]) == MAX_PLAYER_NAME_LEN - 1);
    assert(strncmp(c->whitelist[0], "ThisIsAVeryLongPlayerNameThatEx", MAX_PLAYER_NAME_LEN - 1) == 0);
    assert(strcmp(c->whitelist[1], "NormalPlayer") == 0);

    free(c);
    printf("[PASS] Whitelist parsing and space handling tests passed.\n");
}

/* 2. Whitelist Case-Insensitive Matching Tests */
static void test_whitelist_case_matching(void)
{
    printf("[TEST] Testing case-insensitive whitelist matching...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);

    ParseWhitelist(c, "King Arthur, My Farm 01, Lord Stark");
    assert(c->whitelist_count == 3);

    /* Case-insensitive exact matches */
    assert(IsPlayerWhitelisted(c, "King Arthur") == true);
    assert(IsPlayerWhitelisted(c, "king arthur") == true);
    assert(IsPlayerWhitelisted(c, "KING ARTHUR") == true);
    assert(IsPlayerWhitelisted(c, "kInG aRtHuR") == true);
    assert(IsPlayerWhitelisted(c, "  King Arthur  ") == true);
    assert(IsPlayerWhitelisted(c, "                                  King Arthur                                  ") == true);
    assert(IsPlayerWhitelisted(c, "\t\t\tKing Arthur\t\t\t") == true);

    assert(IsPlayerWhitelisted(c, "My Farm 01") == true);
    assert(IsPlayerWhitelisted(c, "my farm 01") == true);
    assert(IsPlayerWhitelisted(c, "MY FARM 01") == true);

    assert(IsPlayerWhitelisted(c, "Lord Stark") == true);
    assert(IsPlayerWhitelisted(c, "lord stark") == true);

    /* Non-matches */
    assert(IsPlayerWhitelisted(c, "KingArthur") == false);   /* Internal space missing */
    assert(IsPlayerWhitelisted(c, "King  Arthur") == false);  /* Extra internal space */
    assert(IsPlayerWhitelisted(c, "My Farm 02") == false);
    assert(IsPlayerWhitelisted(c, "Unknown Player") == false);
    assert(IsPlayerWhitelisted(c, "King") == false);
    assert(IsPlayerWhitelisted(c, "Arthur") == false);

    /* Edge cases */
    assert(IsPlayerWhitelisted(c, "") == false);
    assert(IsPlayerWhitelisted(c, "   ") == false);
    assert(IsPlayerWhitelisted(c, NULL) == false);
    assert(IsPlayerWhitelisted(NULL, "King Arthur") == false);

    /* Empty whitelist */
    c->whitelist_count = 0;
    assert(IsPlayerWhitelisted(c, "King Arthur") == false);

    free(c);
    printf("[PASS] Case-insensitive whitelist matching tests passed.\n");
}

/* 3. CLI Command Processing and Parameter Validation Tests */
static void test_cli_command_processing(void)
{
    printf("[TEST] Testing CLI command parsing and parameter validation...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);

    /* Null safety */
    CLI_ProcessInput(NULL, "help");
    CLI_ProcessInput(c, NULL);
    CLI_ProcessInput(c, "");
    CLI_ProcessInput(c, "    ");

    /* Status command */
    CLI_ProcessInput(c, "status");
    CLI_ProcessInput(c, "STATUS");
    CLI_ProcessInput(c, "  status  ");

    /* Help command */
    CLI_ProcessInput(c, "help");
    CLI_ProcessInput(c, "HELP");

    /* Harvest mode command */
    assert(c->mode == BOT_MODE_NORMAL);
    CLI_ProcessInput(c, "harvest on");
    assert(c->mode == BOT_MODE_HARVEST);

    CLI_ProcessInput(c, "harvest off");
    assert(c->mode == BOT_MODE_NORMAL);

    CLI_ProcessInput(c, "HARVEST ON");
    assert(c->mode == BOT_MODE_HARVEST);

    CLI_ProcessInput(c, "harvest   OFF");
    assert(c->mode == BOT_MODE_NORMAL);

    /* Harvest invalid parameters should not change mode */
    CLI_ProcessInput(c, "harvest");
    assert(c->mode == BOT_MODE_NORMAL);

    CLI_ProcessInput(c, "harvest invalid_param");
    assert(c->mode == BOT_MODE_NORMAL);

    /* Shield mode command */
    c->protection.shield_always_on = false;
    CLI_ProcessInput(c, "shield on");
    assert(c->protection.shield_always_on == true);

    CLI_ProcessInput(c, "shield off");
    assert(c->protection.shield_always_on == false);

    CLI_ProcessInput(c, "SHIELD ON");
    assert(c->protection.shield_always_on == true);

    CLI_ProcessInput(c, "shield   OFF");
    assert(c->protection.shield_always_on == false);

    /* Shield invalid parameters should not change state */
    CLI_ProcessInput(c, "shield");
    assert(c->protection.shield_always_on == false);

    CLI_ProcessInput(c, "shield foo");
    assert(c->protection.shield_always_on == false);

    /* Unknown command handling */
    CLI_ProcessInput(c, "unknown_command_123");
    CLI_ProcessInput(c, "invalid command here");

    /* Quit and Exit commands */
    c->running = true;
    CLI_ProcessInput(c, "quit");
    assert(c->running == false);

    c->running = true;
    CLI_ProcessInput(c, "QUIT");
    assert(c->running == false);

    c->running = true;
    CLI_ProcessInput(c, "exit");
    assert(c->running == false);

    /* Shield on should enable protection.enabled if it was false */
    c->protection.enabled = false;
    c->protection.shield_always_on = false;
    CLI_ProcessInput(c, "shield on");
    assert(c->protection.enabled == true);
    assert(c->protection.shield_always_on == true);

    /* Test CLI reload command */
    FILE *reload_cmd_fp = fopen("test_cli_reload_cmd.cfg", "w");
    assert(reload_cmd_fp != NULL);
    fprintf(reload_cmd_fp, "whitelist.players = CLI Reload Test Player\n");
    fclose(reload_cmd_fp);

    strncpy(c->config_path, "test_cli_reload_cmd.cfg", sizeof(c->config_path) - 1);
    CLI_ProcessInput(c, "reload");
    assert(c->whitelist_count == 1);
    assert(strcmp(c->whitelist[0], "CLI Reload Test Player") == 0);
    remove("test_cli_reload_cmd.cfg");

    /* Test CLI_Tick batch execution and quit stopping further commands */
    CommandQueue_Init(&c->cmd_queue);
    c->mode = BOT_MODE_NORMAL;
    c->running = true;
    CommandQueue_Push(&c->cmd_queue, "status");
    CommandQueue_Push(&c->cmd_queue, "quit");
    CommandQueue_Push(&c->cmd_queue, "harvest on");
    CLI_Tick(c);
    assert(c->running == false);
    assert(c->mode == BOT_MODE_NORMAL); /* harvest on after quit must NOT execute */
    CommandQueue_Destroy(&c->cmd_queue);

    free(c);
    printf("[PASS] CLI command parsing tests passed.\n");
}

/* 4. Configuration Hot Reload (ReloadConfig) Tests */
static void test_hot_reload(void)
{
    printf("[TEST] Testing configuration hot reload without network reset...\n");

    /* Create initial config file */
    FILE *fp = fopen(TEST_CFG_INIT, "w");
    assert(fp != NULL);
    fprintf(fp, "server.addr = 127.0.0.1\n");
    fprintf(fp, "server.port = 8888\n");
    fprintf(fp, "admin.name = admin_initial\n");
    fprintf(fp, "command.prefix = $\n");
    fprintf(fp, "protection.enabled = true\n");
    fprintf(fp, "protection.shield_always_on = false\n");
    fprintf(fp, "whitelist.players = Main Player 1, Main Player 2\n");
    fclose(fp);

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);

    bool loaded = LoadConfig(c, TEST_CFG_INIT);
    assert(loaded == true);
    assert(c->whitelist_count == 2);
    assert(strcmp(c->whitelist[0], "Main Player 1") == 0);
    assert(strcmp(c->whitelist[1], "Main Player 2") == 0);
    assert(c->protection.shield_always_on == false);

    /* Simulate active network connection and session state */
    c->sock = 100;
    c->auth.igg_id = 999888777;
    strcpy(c->auth.session, "ACTIVE_SESSION_TOKEN_XYZ");
    c->lobby_login = true;
    c->server_time = 1700000000;
    c->resources.food = 2500000;
    strncpy(c->config_path, TEST_CFG_INIT, sizeof(c->config_path) - 1);

    /* Create modified config file for hot reload */
    fp = fopen(TEST_CFG_RELOAD, "w");
    assert(fp != NULL);
    fprintf(fp, "server.addr = 127.0.0.1\n");
    fprintf(fp, "server.port = 8888\n");
    fprintf(fp, "admin.name = new_admin\n");
    fprintf(fp, "command.prefix = #\n");
    fprintf(fp, "protection.enabled = true\n");
    fprintf(fp, "protection.shield_always_on = true\n");
    fprintf(fp, "whitelist.players = Hero Unit, King Arthur, My Farm 01\n");
    fclose(fp);

    /* Execute ReloadConfig */
    bool reloaded = ReloadConfig(c, TEST_CFG_RELOAD);
    assert(reloaded == true);

    /* Verify configuration was updated dynamically */
    assert(c->whitelist_count == 3);
    assert(strcmp(c->whitelist[0], "Hero Unit") == 0);
    assert(strcmp(c->whitelist[1], "King Arthur") == 0);
    assert(strcmp(c->whitelist[2], "My Farm 01") == 0);
    assert(IsPlayerWhitelisted(c, "Hero Unit") == true);
    assert(IsPlayerWhitelisted(c, "Main Player 1") == false); /* Old entry gone */
    assert(c->protection.shield_always_on == true);
    assert(c->bot.command_prefix == '#');
    assert(strcmp(c->bot.admin_name, "new_admin") == 0);

    /* Verify network socket and runtime session state were completely preserved */
    assert(c->sock == 100);
    assert(c->auth.igg_id == 999888777);
    assert(strcmp(c->auth.session, "ACTIVE_SESSION_TOKEN_XYZ") == 0);
    assert(c->lobby_login == true);
    assert(c->server_time == 1700000000);
    assert(c->resources.food == 2500000);

    /* Test reload with non-existent file: must fail and keep existing state intact */
    assert(ReloadConfig(c, "non_existent_reload_file.cfg") == false);
    assert(c->whitelist_count == 3);
    assert(c->protection.shield_always_on == true);
    assert(c->sock == 100);

    /* Test reload with corrupt file: must fail and keep existing state intact */
    fp = fopen(TEST_CFG_CORRUPT, "w");
    assert(fp != NULL);
    fprintf(fp, "invalid syntax without equal sign\n");
    fclose(fp);

    assert(ReloadConfig(c, TEST_CFG_CORRUPT) == false);
    assert(c->whitelist_count == 3);
    assert(c->protection.shield_always_on == true);
    assert(c->sock == 100);

    remove(TEST_CFG_INIT);
    remove(TEST_CFG_RELOAD);
    remove(TEST_CFG_CORRUPT);

    free(c);
    printf("[PASS] Configuration hot reload tests passed.\n");
}

/* 5. Thread-safe Command Queue Tests */

#define PRODUCER_THREADS 4
#define CMDS_PER_PRODUCER 50

typedef struct {
    CommandQueue *q;
    int thread_id;
} ThreadWorkerArg;

#ifdef _WIN32
static DWORD WINAPI ProducerThread(LPVOID lpParam)
#else
static void *ProducerThread(void *lpParam)
#endif
{
    ThreadWorkerArg *arg = (ThreadWorkerArg *)lpParam;
    char cmd[CLI_MAX_CMD_LEN];

    for (int i = 0; i < CMDS_PER_PRODUCER; i++) {
        snprintf(cmd, sizeof(cmd), "cmd_%d_%d", arg->thread_id, i);
        while (!CommandQueue_Push(arg->q, cmd)) {
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1000);
#endif
        }
    }

    return 0;
}

static void test_command_queue_thread_safety(void)
{
    printf("[TEST] Testing thread-safe command queue (single and multi-threaded)...\n");

    CommandQueue q;
    CommandQueue_Init(&q);

    /* FIFO ordering test */
    assert(CommandQueue_Count(&q) == 0);
    assert(CommandQueue_Push(&q, "first") == true);
    assert(CommandQueue_Push(&q, "second") == true);
    assert(CommandQueue_Push(&q, "third") == true);
    assert(CommandQueue_Count(&q) == 3);

    char out[CLI_MAX_CMD_LEN];
    assert(CommandQueue_Pop(&q, out, sizeof(out)) == true);
    assert(strcmp(out, "first") == 0);

    assert(CommandQueue_Pop(&q, out, sizeof(out)) == true);
    assert(strcmp(out, "second") == 0);

    assert(CommandQueue_Pop(&q, out, sizeof(out)) == true);
    assert(strcmp(out, "third") == 0);

    assert(CommandQueue_Count(&q) == 0);
    assert(CommandQueue_Pop(&q, out, sizeof(out)) == false);

    /* Null safety */
    assert(CommandQueue_Push(NULL, "cmd") == false);
    assert(CommandQueue_Push(&q, NULL) == false);
    assert(CommandQueue_Pop(NULL, out, sizeof(out)) == false);
    assert(CommandQueue_Pop(&q, NULL, sizeof(out)) == false);
    assert(CommandQueue_Pop(&q, out, 0) == false);

    /* Idempotent re-initialization safety */
    CommandQueue_Init(&q);
    assert(CommandQueue_Count(&q) == 0);

    /* Queue capacity and overflow drop test (CLI_QUEUE_CAPACITY = 64) */
    char item_buf[CLI_MAX_CMD_LEN];
    for (int i = 0; i < CLI_QUEUE_CAPACITY; i++) {
        snprintf(item_buf, sizeof(item_buf), "item_%d", i);
        assert(CommandQueue_Push(&q, item_buf) == true);
    }
    assert(CommandQueue_Count(&q) == CLI_QUEUE_CAPACITY);
    /* 65th push must be rejected safely */
    assert(CommandQueue_Push(&q, "overflow_item") == false);
    assert(CommandQueue_Count(&q) == CLI_QUEUE_CAPACITY);

    /* Drain all 64 items in strict FIFO order */
    for (int i = 0; i < CLI_QUEUE_CAPACITY; i++) {
        snprintf(item_buf, sizeof(item_buf), "item_%d", i);
        assert(CommandQueue_Pop(&q, out, sizeof(out)) == true);
        assert(strcmp(out, item_buf) == 0);
    }
    assert(CommandQueue_Count(&q) == 0);
    assert(CommandQueue_Pop(&q, out, sizeof(out)) == false);

    /* Multi-threaded producer stress test */
    ThreadWorkerArg args[PRODUCER_THREADS];
#ifdef _WIN32
    HANDLE threads[PRODUCER_THREADS];
    for (int i = 0; i < PRODUCER_THREADS; i++) {
        args[i].q = &q;
        args[i].thread_id = i;
        threads[i] = CreateThread(NULL, 0, ProducerThread, &args[i], 0, NULL);
        assert(threads[i] != NULL);
    }

    int popped_total = 0;
    int expected_total = PRODUCER_THREADS * CMDS_PER_PRODUCER;

    while (popped_total < expected_total) {
        if (CommandQueue_Pop(&q, out, sizeof(out))) {
            popped_total++;
            assert(strncmp(out, "cmd_", 4) == 0);
        } else {
            Sleep(1);
        }
    }

    WaitForMultipleObjects(PRODUCER_THREADS, threads, TRUE, 5000);
    for (int i = 0; i < PRODUCER_THREADS; i++) {
        CloseHandle(threads[i]);
    }
#else
    pthread_t threads[PRODUCER_THREADS];
    for (int i = 0; i < PRODUCER_THREADS; i++) {
        args[i].q = &q;
        args[i].thread_id = i;
        int rc = pthread_create(&threads[i], NULL, ProducerThread, &args[i]);
        assert(rc == 0);
    }

    int popped_total = 0;
    int expected_total = PRODUCER_THREADS * CMDS_PER_PRODUCER;

    while (popped_total < expected_total) {
        if (CommandQueue_Pop(&q, out, sizeof(out))) {
            popped_total++;
            assert(strncmp(out, "cmd_", 4) == 0);
        } else {
            usleep(1000);
        }
    }

    for (int i = 0; i < PRODUCER_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
#endif

    assert(popped_total == expected_total);
    assert(CommandQueue_Count(&q) == 0);

    CommandQueue_Destroy(&q);
    printf("[PASS] Thread-safe command queue tests passed (%d messages enqueued/dequeued).\n", expected_total);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Running Phase 2 Comprehensive Verification Tests ===\n");

    printf("Step 1: test_whitelist_parsing...\n");
    test_whitelist_parsing();
    printf("Step 2: test_whitelist_case_matching...\n");
    test_whitelist_case_matching();
    printf("Step 3: test_cli_command_processing...\n");
    test_cli_command_processing();
    printf("Step 4: test_hot_reload...\n");
    test_hot_reload();
    printf("Step 5: test_command_queue_thread_safety...\n");
    test_command_queue_thread_safety();

    printf("=== All Phase 2 Tests Passed Successfully ===\n");
    return 0;
}
