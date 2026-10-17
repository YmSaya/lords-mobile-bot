#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NDEBUG
  #undef NDEBUG
#endif
#include <assert.h>

#include "connection.h"
#include "config.h"
#include "protocol.h"
#include "cli.h"
#include "ipc.h"
#include "updater.h"
#include "version.h"
#include "packet_enum.h"
#include "map_point.h"

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #define STR_CASE_CMP _stricmp
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <strings.h>
  #define STR_CASE_CMP strcasecmp
#endif

/* 1. Localhost TCP IPC Communication Test */
static void test_localhost_tcp_ipc(void)
{
    printf("[TEST] Testing Localhost TCP IPC communication and JSON-Line protocol...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);

    c->sock = 42;
    strncpy(c->player.name, "Commander", sizeof(c->player.name) - 1);
    c->player.power = 8888888;
    c->player.gems = 15000;
    c->mode = BOT_MODE_NORMAL;
    c->troops_in_shelter = false;
    c->gather_enabled = true;
    c->gather_paused = false;
    c->shield_info.active = true;
    c->shield_info.begin_time = 1000;
    c->shield_info.duration = 28800; /* 8h */
    c->server_time = 1000;
    c->protection.enabled = true;
    c->protection.shield_always_on = true;
    c->resources.food = 1000000;
    c->resources.rock = 2000000;
    c->resources.wood = 3000000;
    c->resources.ore = 4000000;
    c->resources.gold = 5000000;
    c->whitelist_count = 2;
    c->player.current_marches = 2;
    c->player.max_marches = 5;
    c->running = true;

    /* A. Direct JSON-Line command processing verification */
    char response[IPC_MAX_MSG_LEN];

    /* Test: status command */
    assert(IPC_ProcessCommand(c, "{\"cmd\":\"status\"}", response, sizeof(response)));
    assert(strstr(response, "\"status\":\"ok\"") != NULL);
    assert(strstr(response, "\"version\":") != NULL);
    assert(strstr(response, "\"Commander\"") != NULL);
    assert(strstr(response, "8888888") != NULL);
    assert(strstr(response, "\"food\":1000000") != NULL);
    assert(strstr(response, "\"whitelist_count\":2") != NULL);

    /* Test: compound space command {"cmd":"harvest on"} */
    assert(IPC_ProcessCommand(c, "{\"cmd\":\"harvest on\"}", response, sizeof(response)));
    assert(strstr(response, "\"status\":\"ok\"") != NULL);
    assert(c->mode == BOT_MODE_HARVEST);
    assert(c->gather_paused == true);
    assert(c->troops_in_shelter == true);

    /* Test: harvest off */
    assert(IPC_ProcessCommand(c, "{\"cmd\":\"harvest\",\"action\":\"off\"}", response, sizeof(response)));
    assert(strstr(response, "\"status\":\"ok\"") != NULL);
    assert(c->mode == BOT_MODE_NORMAL);
    assert(c->gather_paused == false);
    assert(c->troops_in_shelter == false);

    /* Test: shield on/off */
    assert(IPC_ProcessCommand(c, "{\"cmd\":\"shield\",\"action\":\"off\"}", response, sizeof(response)));
    assert(c->protection.shield_always_on == false);
    assert(IPC_ProcessCommand(c, "{\"cmd\":\"shield\",\"action\":\"on\"}", response, sizeof(response)));
    assert(c->protection.shield_always_on == true);

    /* Test: gather on/off */
    assert(IPC_ProcessCommand(c, "{\"cmd\":\"gather\",\"action\":\"off\"}", response, sizeof(response)));
    assert(c->gather_enabled == false);
    assert(IPC_ProcessCommand(c, "{\"cmd\":\"gather\",\"action\":\"on\"}", response, sizeof(response)));
    assert(c->gather_enabled == true);

    /* Test: reload command */
    assert(IPC_ProcessCommand(c, "{\"cmd\":\"reload\"}", response, sizeof(response)));
    assert(strstr(response, "\"status\":") != NULL);

    /* Test: update command */
    assert(IPC_ProcessCommand(c, "{\"cmd\":\"update\"}", response, sizeof(response)));
    assert(strstr(response, "\"status\":") != NULL);

    /* Test: help command */
    assert(IPC_ProcessCommand(c, "{\"cmd\":\"help\"}", response, sizeof(response)));
    assert(strstr(response, "\"status\":\"ok\"") != NULL);

    /* Test: unknown command */
    assert(IPC_ProcessCommand(c, "{\"cmd\":\"invalid_command_xyz\"}", response, sizeof(response)));
    assert(strstr(response, "\"status\":\"error\"") != NULL);

    /* Test: invalid harvest action */
    assert(IPC_ProcessCommand(c, "{\"cmd\":\"harvest\",\"action\":\"unknown_opt\"}", response, sizeof(response)));
    assert(strstr(response, "\"status\":\"error\"") != NULL);

    /* B. Real Localhost TCP socket networking test */
    uint16_t test_port = 29999;
    assert(IPC_Init(c, test_port));

#ifdef _WIN32
    Sleep(50);
#else
    usleep(50000);
#endif

#ifdef _WIN32
    SOCKET client_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(client_sock != INVALID_SOCKET);
#else
    int client_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(client_sock >= 0);
#endif

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(test_port);
    saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int conn_res = connect(client_sock, (struct sockaddr *)&saddr, sizeof(saddr));
    assert(conn_res == 0);

    /* Send status command over TCP */
    const char *req = "{\"cmd\":\"status\"}\n";
    send(client_sock, req, (int)strlen(req), 0);

    char sock_buf[IPC_MAX_MSG_LEN] = {0};
    int n = recv(client_sock, sock_buf, sizeof(sock_buf) - 1, 0);
    assert(n > 0);
    sock_buf[n] = '\0';

    assert(strstr(sock_buf, "\"status\":\"ok\"") != NULL);
    assert(strstr(sock_buf, "\"Commander\"") != NULL);

#ifdef _WIN32
    closesocket(client_sock);
#else
    close(client_sock);
#endif

    IPC_Cleanup(c);
    free(c);
    printf("[PASS] Localhost TCP IPC communication tests passed.\n");
}

/* 2. SemVer Version Comparison Algorithm Test */
static void test_semver_algorithm(void)
{
    printf("[TEST] Testing SemVer comparison algorithm...\n");

    /* Equality tests */
    assert(SemVer_Compare("1.0.2", "1.0.2") == 0);
    assert(SemVer_Compare("v1.0.2", "1.0.2") == 0);
    assert(SemVer_Compare("V1.0.2", "v1.0.2") == 0);
    assert(SemVer_Compare("  v1.0.2  ", "1.0.2") == 0);

    /* Major version differences */
    assert(SemVer_Compare("2.0.0", "1.99.99") > 0);
    assert(SemVer_Compare("1.99.99", "2.0.0") < 0);

    /* Minor version differences */
    assert(SemVer_Compare("1.10.0", "1.2.0") > 0);
    assert(SemVer_Compare("1.2.0", "1.10.0") < 0);
    assert(SemVer_Compare("1.5.0", "1.4.99") > 0);

    /* Patch version differences */
    assert(SemVer_Compare("1.0.10", "1.0.2") > 0);
    assert(SemVer_Compare("1.0.2", "1.0.10") < 0);
    assert(SemVer_Compare("1.0.3", "1.0.2") > 0);
    assert(SemVer_Compare("1.0.2", "1.0.3") < 0);

    /* Missing patch handling */
    assert(SemVer_Compare("1.0", "1.0.0") == 0);
    assert(SemVer_Compare("1.1", "1.0.9") > 0);

    /* Non-numeric / NULL safety */
    assert(SemVer_Compare("invalid", "1.0.0") < 0);
    assert(SemVer_Compare("1.0.0", "invalid") > 0);
    assert(SemVer_Compare("invalid", "invalid") == 0);
    assert(SemVer_Compare(NULL, "1.0.0") < 0);
    assert(SemVer_Compare("1.0.0", NULL) > 0);
    assert(SemVer_Compare(NULL, NULL) == 0);

    printf("[PASS] SemVer comparison algorithm tests passed.\n");
}

/* 3. Binary Rename Replacement and Rollback Protection Test */
static void test_binary_replacement_and_rollback(void)
{
    printf("[TEST] Testing binary replacement and rollback protection...\n");

    const char *curr_file = "test_phase5_app.exe";
    const char *new_file = "test_phase5_app.exe.new";
    const char *old_file = "test_phase5_app.exe.old";

    /* Clean up any leftovers */
    remove(curr_file);
    remove(new_file);
    remove(old_file);

    /* Setup initial current file and new file */
    FILE *fp = fopen(curr_file, "wb");
    assert(fp != NULL);
    fputs("VERSION_1_BINARY_DATA", fp);
    fclose(fp);

    fp = fopen(new_file, "wb");
    assert(fp != NULL);
    fputs("VERSION_2_BINARY_DATA", fp);
    fclose(fp);

    /* 1. Successful replacement test */
    bool ok = Updater_ReplaceAndRollback(curr_file, new_file, old_file);
    assert(ok == true);

    /* Verify curr_file now contains VERSION_2 */
    char buf[64] = {0};
    fp = fopen(curr_file, "rb");
    assert(fp != NULL);
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    assert(strcmp(buf, "VERSION_2_BINARY_DATA") == 0);

    /* Verify old_file contains VERSION_1 */
    memset(buf, 0, sizeof(buf));
    fp = fopen(old_file, "rb");
    assert(fp != NULL);
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    assert(strcmp(buf, "VERSION_1_BINARY_DATA") == 0);

    /* Verify new_file is gone */
    assert(fopen(new_file, "rb") == NULL);

    /* 2. Rollback protection test when .new is missing */
    bool rollback_ok = Updater_ReplaceAndRollback(curr_file, new_file, old_file);
    assert(rollback_ok == false);

    /* Verify curr_file remained completely untouched and intact */
    memset(buf, 0, sizeof(buf));
    fp = fopen(curr_file, "rb");
    assert(fp != NULL);
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    assert(strcmp(buf, "VERSION_2_BINARY_DATA") == 0);

    /* 3. Replacement test when old_file already exists from previous runs */
    fp = fopen(new_file, "wb");
    assert(fp != NULL);
    fputs("VERSION_3_BINARY_DATA", fp);
    fclose(fp);

    bool ok_existing_old = Updater_ReplaceAndRollback(curr_file, new_file, old_file);
    assert(ok_existing_old == true);

    memset(buf, 0, sizeof(buf));
    fp = fopen(curr_file, "rb");
    assert(fp != NULL);
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    assert(strcmp(buf, "VERSION_3_BINARY_DATA") == 0);

    /* Clean up temporary test files */
    remove(curr_file);
    remove(new_file);
    remove(old_file);

    /* Boundary safety check with NULL */
    assert(Updater_ReplaceAndRollback(NULL, "a", "b") == false);
    assert(Updater_ReplaceAndRollback("a", NULL, "b") == false);
    assert(Updater_ReplaceAndRollback("a", "b", NULL) == false);

    printf("[PASS] Binary replacement and rollback protection tests passed.\n");
}

/* 4. Discord Admin Authorization Filter Test */
static void test_discord_admin_authorization(void)
{
    printf("[TEST] Testing Discord admin authorization filter...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);

    /* Parse multiple admin IDs with whitespace */
    const char *admin_str = " 123456789012345678 , 987654321098765432 , 112233445566778899 ";
    assert(Discord_ParseAdminUsers(c, admin_str));
    assert(c->discord.admin_user_count == 3);

    /* Verify whitelisted IDs */
    assert(Discord_IsAdminUser(c, "123456789012345678") == true);
    assert(Discord_IsAdminUser(c, "987654321098765432") == true);
    assert(Discord_IsAdminUser(c, "112233445566778899") == true);

    /* Verify non-whitelisted ID */
    assert(Discord_IsAdminUser(c, "555555555555555555") == false);

    /* Verify whitespace trimming on input user ID */
    assert(Discord_IsAdminUser(c, "  123456789012345678  ") == true);
    assert(Discord_IsAdminUser(c, "\t987654321098765432\n") == true);

    /* Boundary and NULL checks */
    assert(Discord_IsAdminUser(c, "") == false);
    assert(Discord_IsAdminUser(c, "   ") == false);
    assert(Discord_IsAdminUser(c, NULL) == false);
    assert(Discord_IsAdminUser(NULL, "123456789012345678") == false);

    /* Empty whitelist configuration */
    assert(Discord_ParseAdminUsers(c, ""));
    assert(c->discord.admin_user_count == 0);
    assert(Discord_IsAdminUser(c, "123456789012345678") == false);

    free(c);
    printf("[PASS] Discord admin authorization tests passed.\n");
}

/* 5. Full Autonomous Operation State Machine Integration Test */
static void test_autonomous_state_machine(void)
{
    printf("[TEST] Testing full autonomous operation state machine...\n");

    /* A. Auto-Reconnect Exponential Backoff Tracker */
    ReconnectTracker rt;
    Reconnect_Init(&rt);

    assert(rt.initial_delay_sec == 2);
    assert(rt.max_delay_sec == 60);
    assert(rt.reconnect_attempts == 0);
    assert(rt.reconnecting == false);

    /* Sequence of backoffs */
    assert(Reconnect_GetNextBackoff(&rt) == 2);
    assert(rt.reconnect_attempts == 1);
    assert(rt.reconnecting == true);

    assert(Reconnect_GetNextBackoff(&rt) == 4);
    assert(rt.reconnect_attempts == 2);

    assert(Reconnect_GetNextBackoff(&rt) == 8);
    assert(rt.reconnect_attempts == 3);

    assert(Reconnect_GetNextBackoff(&rt) == 16);
    assert(rt.reconnect_attempts == 4);

    assert(Reconnect_GetNextBackoff(&rt) == 32);
    assert(rt.reconnect_attempts == 5);

    assert(Reconnect_GetNextBackoff(&rt) == 60);
    assert(rt.reconnect_attempts == 6);

    /* Must be capped at max_delay_sec */
    assert(Reconnect_GetNextBackoff(&rt) == 60);
    assert(rt.reconnect_attempts == 7);

    /* Reset backoff on successful connection */
    Reconnect_Reset(&rt);
    assert(rt.current_delay_sec == 2);
    assert(rt.reconnect_attempts == 0);
    assert(rt.reconnecting == false);

    /* B. Night Mandatory Shield & 5-minute renewal */
    const uint64_t midnight_utc = 1700006400ULL;
    const uint64_t local_midnight = midnight_utc - (8 * 3600);
    assert(IsNightShieldTime(8, local_midnight + 1800) == true);   /* 00:30 */
    assert(IsNightShieldTime(8, local_midnight + 10000) == true);  /* mid-night */
    assert(IsNightShieldTime(8, local_midnight + 23400) == true);  /* 06:30 */
    assert(IsNightShieldTime(8, local_midnight + 23401) == false); /* 06:30:01 */
    assert(IsNightShieldTime(8, local_midnight + 43200) == false); /* 12:00 noon */

    /* Verification of 5-minute renewal threshold via ShieldTick */
    Connection *c_shield = calloc(1, sizeof(Connection));
    assert(c_shield != NULL);
    Configuration(c_shield);
    c_shield->protection.enabled = true;
    c_shield->protection.timezone_offset = 8;
    c_shield->shield_info.loaded = true;
    c_shield->shield_info.active = true;
    c_shield->shield_info.begin_time = local_midnight + 1800; // 00:30
    c_shield->shield_info.duration = 14400; // 4h, expires at 04:30 (within 00:30~06:30 night window)
    c_shield->items[SHIELD_8H].quantity = 5;
    c_shield->items_loaded = true;

    /* Remaining time: 600s (> 300s / 5m) at 04:20 -> Should NOT renew yet */
    c_shield->server_time = c_shield->shield_info.begin_time + c_shield->shield_info.duration - 600;
    c_shield->shield_info.pending = false;
    ShieldTick(c_shield);
    assert(c_shield->shield_info.pending == false);
    assert(c_shield->items[SHIELD_8H].quantity == 5);

    /* Remaining time: 200s (<= 300s / 5m) at 04:26:40 -> Triggers 5-minute auto-renewal! */
    c_shield->server_time = c_shield->shield_info.begin_time + c_shield->shield_info.duration - 200;
    c_shield->shield_info.pending = false;
    ShieldTick(c_shield);
    assert(c_shield->shield_info.pending == true);
    assert(c_shield->items[SHIELD_8H].quantity == 4);
    free(c_shield);

    /* C. Map scan, weighted tile scoring and precision dispatch */
    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);

    PointCode pc = getPointCodeByMapPos(100, 100);
    c->player.zone_id = pc.zoneID;
    c->player.point_id = pc.pointID;

    /* Verify Crystal > Gold > Food > Iron > Wood > Stone */
    ResourceTile t_gem = { .x = 105, .y = 100, .kind = PK_CRYSTAL, .level = 1, .remaining_resources = 50000 };
    ResourceTile t_gold = { .x = 105, .y = 100, .kind = PK_GOLD, .level = 1, .remaining_resources = 50000 };
    ResourceTile t_food = { .x = 105, .y = 100, .kind = PK_FOOD, .level = 1, .remaining_resources = 50000 };
    ResourceTile t_iron = { .x = 105, .y = 100, .kind = PK_IRON, .level = 1, .remaining_resources = 50000 };
    ResourceTile t_wood = { .x = 105, .y = 100, .kind = PK_WOOD, .level = 1, .remaining_resources = 50000 };
    ResourceTile t_stone = { .x = 105, .y = 100, .kind = PK_STONE, .level = 1, .remaining_resources = 50000 };

    double s_gem = CalculateTileScore(c, &t_gem);
    double s_gold = CalculateTileScore(c, &t_gold);
    double s_food = CalculateTileScore(c, &t_food);
    double s_iron = CalculateTileScore(c, &t_iron);
    double s_wood = CalculateTileScore(c, &t_wood);
    double s_stone = CalculateTileScore(c, &t_stone);

    assert(s_gem > s_gold);
    assert(s_gold > s_food);
    assert(s_food > s_iron);
    assert(s_iron > s_wood);
    assert(s_wood > s_stone);

    /* D. Harvest mode secondary shelter defense on troop return (0 garrison) */
    c->mode = BOT_MODE_HARVEST;
    c->troops_in_shelter = false;
    c->player.current_marches = 1;
    c->marches[0].active = true;
    c->marches[0].state = MARCH_STATE_RETURNING;

    /* Simulate troop arrival in turf during harvest mode */
    OnMarchReturn(c, 0);

    /* Verification: must trigger secondary shelter to maintain 0 garrison! */
    assert(c->troops_in_shelter == true);

    free(c);
    printf("[PASS] Full autonomous state machine integration tests passed.\n");
}

int main(void)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    printf("================================================================================\n");
    printf("                  PHASE 5 TEST SUITE: AUTO UPDATE & DISCORD IPC                 \n");
    printf("================================================================================\n");

    test_localhost_tcp_ipc();
    test_semver_algorithm();
    test_binary_replacement_and_rollback();
    test_discord_admin_authorization();
    test_autonomous_state_machine();

    printf("================================================================================\n");
    printf("                  ALL PHASE 5 TESTS PASSED SUCCESSFULLY!                       \n");
    printf("================================================================================\n");

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
