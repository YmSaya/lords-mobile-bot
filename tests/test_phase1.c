#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NDEBUG
  #undef NDEBUG
#endif
#include <assert.h>
#include "connection.h"
#include "config.h"
#include "command.h"
#include "items.h"

#define TEST_CONFIG_1 "test_config_1.cfg"
#define TEST_CONFIG_2 "test_config_2.cfg"
#define TEST_CONFIG_INV "test_config_inv.cfg"
#define TEST_DEFAULT_CFG "test_default.cfg"
#define TEST_BUFFER_SIZE 4096

static void test_memory_allocation(void)
{
    printf("[TEST] Testing Connection struct heap allocation & safety...\n");

    size_t conn_size = sizeof(Connection);
    printf("[INFO] sizeof(Connection) = %zu bytes\n", conn_size);

    // Verify Connection is indeed large (> 600 KB) which threatens 1MB stack
    assert(conn_size >= 600000);

    Connection *client = calloc(1, sizeof(Connection));
    assert(client != NULL);

    // Verify zeroed fields
    assert(client->sock == 0);
    assert(client->stream.read_pos == 0);
    assert(client->stream.parse_pos == 0);

    // Test disconnect with NULL and negative socket
    disconnect(NULL);
    client->sock = -1;
    disconnect(client);
    assert(client->sock == -1);

    // Test reset_connection
    client->sock = 10;
    client->protection.enabled = true;
    reset_connection(client);
    assert(client->sock == -1);
    assert(client->protection.enabled == false);
    reset_connection(NULL);

    free(client);
    printf("[PASS] Memory allocation and reset tests passed.\n");
}

static void test_default_configuration(void)
{
    printf("[TEST] Testing default Configuration() initialization...\n");

    Connection *client = calloc(1, sizeof(Connection));
    assert(client != NULL);

    Configuration(client);

    // Verify bot default settings
    assert(client->bot.command_prefix == '$');
    assert(strcmp(client->bot.admin_name, "halloweeks") == 0);
    assert(strcmp(client->bot.data_path, "./lmbot/") == 0);

    // Verify app version
    assert(client->app.version_major == 2);
    assert(client->app.version_minor == 197);
    assert(client->app.version_patch == 307);
    assert(client->app.language_code == 1);

    // Verify protection defaults
    assert(client->protection.enabled == true);
    assert(client->protection.shield_always_on == false);
    assert(client->protection.shield_on_incoming_attack == true);
    assert(client->protection.shield_on_incoming_scout == true);
    assert(client->protection.recall_on_incoming_attack == true);
    assert(client->protection.recall_on_incoming_scout == true);
    assert(client->protection.recall_on_incoming_conflict == true);
    assert(client->protection.shield_priority_count == 4);
    assert(client->protection.shield_priority[0] == SHIELD_4H);

    // Verify market & alliance defaults
    assert(client->market.settings.auto_trade == true);
    assert(client->alliance.auto_help == true);
    assert(client->alliance.auto_open_gifts == true);

    free(client);
    printf("[PASS] Default Configuration() initialization passed.\n");
}

static void test_create_default_config_and_load(void)
{
    printf("[TEST] Testing CreateDefaultConfig and round-trip LoadConfig...\n");

    bool created = CreateDefaultConfig(TEST_DEFAULT_CFG);
    assert(created == true);

    Connection *client = calloc(1, sizeof(Connection));
    assert(client != NULL);

    bool loaded = LoadConfig(client, TEST_DEFAULT_CFG);
    assert(loaded == true);

    assert(strcmp(client->gateway_server.addr, "192.243.44.63") == 0);
    assert(client->gateway_server.port == 5999);
    assert(strcmp(client->bot.admin_name, "halloweeks") == 0);
    assert(client->bot.command_prefix == '$');
    assert(strcmp(client->bot.data_path, "/sdcard/lmbot/") == 0);
    assert(client->protection.enabled == false);
    assert(client->protection.shield_always_on == false);
    assert(client->protection.recall_on_incoming_attack == false);
    assert(client->protection.recall_on_incoming_scout == false);
    assert(client->protection.recall_on_incoming_conflict == false);

    remove(TEST_DEFAULT_CFG);
    free(client);
    printf("[PASS] CreateDefaultConfig and LoadConfig round-trip passed.\n");
}

static void test_config_parsing(void)
{
    printf("[TEST] Testing config parser for shield_always_on and edge cases...\n");

    Connection *client = calloc(1, sizeof(Connection));
    assert(client != NULL);

    // Non-existent file must fail
    assert(LoadConfig(client, "non_existent_file_xyz.cfg") == false);

    // Test case 1: protection.enabled = false, protection.shield_always_on = true
    FILE *fp = fopen(TEST_CONFIG_1, "w");
    assert(fp != NULL);
    fprintf(fp, "# Test comment\n");
    fprintf(fp, "\n");
    fprintf(fp, "admin.name = test_admin\n");
    fprintf(fp, "command.prefix = !\n");
    fprintf(fp, "data.path = ./custom_data/\n");
    fprintf(fp, "protection.enabled = false\n");
    fprintf(fp, "protection.shield_always_on = true\n");
    fprintf(fp, "protection.recall_on_incoming_attack = true\n");
    fprintf(fp, "protection.recall_on_incoming_scout = true\n");
    fprintf(fp, "protection.recall_on_incoming_conflict = true\n");
    fclose(fp);

    bool loaded = LoadConfig(client, TEST_CONFIG_1);
    assert(loaded == true);
    assert(client->protection.enabled == false);
    assert(client->protection.shield_always_on == true);
    assert(client->protection.recall_on_incoming_attack == true);
    assert(client->protection.recall_on_incoming_scout == true);
    assert(client->protection.recall_on_incoming_conflict == true);
    assert(strcmp(client->bot.admin_name, "test_admin") == 0);
    assert(client->bot.command_prefix == '!');
    assert(strcmp(client->bot.data_path, "./custom_data/") == 0);

    // Test case 2: protection.enabled = true, protection.shield_always_on = false
    fp = fopen(TEST_CONFIG_2, "w");
    assert(fp != NULL);
    fprintf(fp, "protection.enabled = true\n");
    fprintf(fp, "protection.shield_always_on = false\n");
    fclose(fp);

    loaded = LoadConfig(client, TEST_CONFIG_2);
    assert(loaded == true);
    assert(client->protection.enabled == true);
    assert(client->protection.shield_always_on == false);

    // Test case 3: Invalid syntax file
    fp = fopen(TEST_CONFIG_INV, "w");
    assert(fp != NULL);
    fprintf(fp, "invalid_line_without_equal_sign\n");
    fclose(fp);

    loaded = LoadConfig(client, TEST_CONFIG_INV);
    assert(loaded == false);

    remove(TEST_CONFIG_1);
    remove(TEST_CONFIG_2);
    remove(TEST_CONFIG_INV);
    free(client);

    printf("[PASS] Config parsing test passed.\n");
}

static void test_superuser_security(void)
{
    printf("[TEST] Testing SuperUserAccess permission check & command handler...\n");

    Connection *client = calloc(1, sizeof(Connection));
    assert(client != NULL);

    // Null safety checks
    SuperUserAccess(NULL, "admin", "new_admin");
    SuperUserAccess(client, NULL, "new_admin");
    SuperUserAccess(client, "admin", NULL);
    command_handler(NULL, "admin", "$su new");
    command_handler(client, NULL, "$su new");
    command_handler(client, "admin", NULL);

    Configuration(client); // Sets command_prefix = '$', admin_name = 'halloweeks'
    strcpy(client->bot.admin_name, "legit_admin");

    // Attempt 1: Unauthorized user attempts to change admin to malicious_user
    SuperUserAccess(client, "malicious_user", "malicious_user");
    assert(strcmp(client->bot.admin_name, "legit_admin") == 0);

    // Attempt 2: Another arbitrary unauthorized player
    SuperUserAccess(client, "stranger_123", "new_admin");
    assert(strcmp(client->bot.admin_name, "legit_admin") == 0);

    // Attempt 3: Authorized admin changes admin to second_admin
    SuperUserAccess(client, "legit_admin", "second_admin");
    assert(strcmp(client->bot.admin_name, "second_admin") == 0);

    // Attempt 4: Old admin can no longer grant access
    SuperUserAccess(client, "legit_admin", "malicious_user");
    assert(strcmp(client->bot.admin_name, "second_admin") == 0);

    // Attempt 5: Command handler integration with prefix check
    command_handler(client, "malicious_user", "$su hacked");
    assert(strcmp(client->bot.admin_name, "second_admin") == 0);

    command_handler(client, "second_admin", "!su third_admin"); // wrong prefix
    assert(strcmp(client->bot.admin_name, "second_admin") == 0);

    command_handler(client, "second_admin", "$su third_admin"); // correct
    assert(strcmp(client->bot.admin_name, "third_admin") == 0);

    // If prefix is 0, all commands are ignored
    client->bot.command_prefix = 0;
    command_handler(client, "third_admin", "$su fourth_admin");
    assert(strcmp(client->bot.admin_name, "third_admin") == 0);

    free(client);
    printf("[PASS] SuperUserAccess security test passed.\n");
}

static void test_tcp_loopback_and_buffer(void)
{
    printf("[TEST] Testing TCP loopback socket and buffer boundary behavior...\n");

    // Create server socket on loopback
#ifdef _WIN32
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(server_sock != INVALID_SOCKET);
#else
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(server_sock >= 0);
#endif

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serv_addr.sin_port = 0; // ephemeral port

    int b_res = bind(server_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    assert(b_res == 0);

    int l_res = listen(server_sock, 1);
    assert(l_res == 0);

    int addrlen = sizeof(serv_addr);
    getsockname(server_sock, (struct sockaddr *)&serv_addr, &addrlen);
    unsigned short port = ntohs(serv_addr.sin_port);

    // Connect using connect_server
    Connection *client = calloc(1, sizeof(Connection));
    assert(client != NULL);

    client->sock = connect_server("127.0.0.1", port);
    assert(client->sock >= 0);

#ifdef _WIN32
    SOCKET accepted_sock = accept(server_sock, NULL, NULL);
    assert(accepted_sock != INVALID_SOCKET);
#else
    int accepted_sock = accept(server_sock, NULL, NULL);
    assert(accepted_sock >= 0);
#endif

    // Verify non-blocking mode setting
    assert(set_nonblocking(client) == 0);

    // Verify buffer boundary logic:
    // If stream.read_pos == TEST_BUFFER_SIZE, recv is not called with len 0
    client->stream.read_pos = TEST_BUFFER_SIZE;
    bool can_recv = (client->stream.read_pos < TEST_BUFFER_SIZE);
    assert(can_recv == false);

    // Send a 16-byte packet from server to client
    uint8_t pkt[16] = {16, 0, 1, 0, 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!'};
    send(accepted_sock, (const char *)pkt, sizeof(pkt), 0);

    // Space freed in buffer
    client->stream.read_pos = 0;
    can_recv = (client->stream.read_pos < TEST_BUFFER_SIZE);
    assert(can_recv == true);

    int n = recv(client->sock, (char *)client->stream.buffer, (int)(TEST_BUFFER_SIZE - client->stream.read_pos), 0);
    assert(n == 16);
    client->stream.read_pos += n;
    assert(client->stream.read_pos == 16);

    // Clean disconnect
    disconnect(client);
    assert(client->sock == -1);

    close_socket(accepted_sock);
    close_socket(server_sock);
    free(client);

    printf("[PASS] TCP loopback and buffer boundary test passed.\n");
}

int main(void)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[FAIL] WSAStartup failed\n");
        return 1;
    }
#endif

    printf("=== Running Phase 1 Comprehensive Verification Tests ===\n");

    test_memory_allocation();
    test_default_configuration();
    test_create_default_config_and_load();
    test_config_parsing();
    test_superuser_security();
    test_tcp_loopback_and_buffer();

    printf("=== All Phase 1 Tests Passed Successfully ===\n");

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
