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
#include "packet_enum.h"
#include "net_rw.h"

/* 1. UTC+08:00 Night Time Boundaries and Forced Shield Schedule Tests */
static void test_night_shield_boundaries(void)
{
    printf("[TEST] Testing UTC+08:00 night time boundaries and schedule...\n");

    /* Reference base: a known epoch timestamp divisible by 86400 (UTC midnight) */
    const uint64_t midnight_utc = 1700006400ULL;
    assert((midnight_utc % 86400ULL) == 0);

    /* Local midnight in UTC+08:00 occurs 8 hours earlier in UTC */
    const uint64_t local_midnight = midnight_utc - (8 * 3600);

    /* Test 00:29 (1740s) -> Outside night forced window (false) */
    assert(IsNightShieldTime(8, local_midnight + 1740) == false);

    /* Test 00:29:59 (1799s) -> Outside night forced window (false) */
    assert(IsNightShieldTime(8, local_midnight + 1799) == false);

    /* Test 00:30:00 (1800s) -> Start of night forced window (true) */
    assert(IsNightShieldTime(8, local_midnight + 1800) == true);

    /* Test 03:00:00 (10800s) -> Mid-night forced window (true) */
    assert(IsNightShieldTime(8, local_midnight + 10800) == true);

    /* Test 06:30:00 (23400s) -> End of night forced window boundary (true) */
    assert(IsNightShieldTime(8, local_midnight + 23400) == true);

    /* Test 06:30:01 (23401s) -> Outside night forced window (false) */
    assert(IsNightShieldTime(8, local_midnight + 23401) == false);

    /* Test 06:31:00 (23460s) -> Outside night forced window (false) */
    assert(IsNightShieldTime(8, local_midnight + 23460) == false);

    /* Test daytime 12:00:00 (43200s) -> (false) */
    assert(IsNightShieldTime(8, local_midnight + 43200) == false);

    /* Test late night 23:59:00 (86340s) -> (false) */
    assert(IsNightShieldTime(8, local_midnight + 86340) == false);

    /* Test other timezones: UTC+00:00 */
    assert(IsNightShieldTime(0, midnight_utc + 1740) == false);
    assert(IsNightShieldTime(0, midnight_utc + 1800) == true);
    assert(IsNightShieldTime(0, midnight_utc + 23400) == true);
    assert(IsNightShieldTime(0, midnight_utc + 23460) == false);

    /* Test negative timezone offset: UTC-05:00 */
    const uint64_t est_midnight = midnight_utc + (5 * 3600);
    assert(IsNightShieldTime(-5, est_midnight + 1740) == false);
    assert(IsNightShieldTime(-5, est_midnight + 1800) == true);
    assert(IsNightShieldTime(-5, est_midnight + 23400) == true);
    assert(IsNightShieldTime(-5, est_midnight + 23460) == false);

    /* Test ShieldTick integration with night schedule */
    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);
    c->sock = -1;
    c->protection.enabled = true;
    c->protection.shield_always_on = false; // routine 24/7 is off
    c->protection.timezone_offset = 8;
    c->shield_info.loaded = true;
    c->items[SHIELD_8H].quantity = 2;

    /* Daytime 12:00 -> no shield triggered */
    c->server_time = local_midnight + 43200;
    c->shield_info.active = false;
    c->shield_info.pending = false;
    ShieldTick(c);
    assert(c->shield_info.pending == false);
    assert(c->items[SHIELD_8H].quantity == 2);

    /* Night time 03:00 -> forced shield triggered! */
    c->server_time = local_midnight + 10800;
    c->shield_info.active = false;
    c->shield_info.pending = false;
    ShieldTick(c);
    assert(c->shield_info.pending == true);
    assert(c->items[SHIELD_8H].quantity == 1);

    /* Night time 03:00 with shield active and plenty of time (> 5 min) -> no renewal */
    c->shield_info.pending = false;
    c->shield_info.active = true;
    c->shield_info.begin_time = c->server_time - 3600;
    c->shield_info.duration = 7200; // 3600s remaining > 300s
    ShieldTick(c);
    assert(c->shield_info.pending == false);
    assert(c->items[SHIELD_8H].quantity == 1);

    /* Night time with shield active but <= 5 minutes remaining -> renewal triggered */
    c->shield_info.begin_time = c->server_time - 7000;
    c->shield_info.duration = 7200; // 200s remaining <= 300s
    c->shield_info.pending = false;
    ShieldTick(c);
    assert(c->shield_info.pending == true);
    assert(c->items[SHIELD_8H].quantity == 0);

    /* Harvest mode suspends night forced shield */
    c->mode = BOT_MODE_HARVEST;
    c->shield_info.active = false;
    c->shield_info.pending = false;
    c->items[SHIELD_8H].quantity = 5;
    ShieldTick(c);
    assert(c->shield_info.pending == false);
    assert(c->items[SHIELD_8H].quantity == 5);

    free(c);
    printf("[PASS] UTC+08:00 night time boundaries and schedule tests passed.\n");
}

/* 2. Shield Item Consumption Ladder Tests (8h -> 4h -> 24h -> 24h Crystal) */
static void test_shield_consumption_ladder(void)
{
    printf("[TEST] Testing shield item consumption ladder (8h -> 4h -> 24h -> crystals)...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);
    c->sock = -1;

    /* Setup inventory with all tiers */
    c->items[SHIELD_8H].quantity = 1;
    c->items[SHIELD_4H].quantity = 2;
    c->items[SHIELD_1D].quantity = 1;
    c->player.gems = 3000;
    c->shield_alert_24h_active = false;

    /* Step 1: 8h shield must be consumed first */
    c->shield_info.pending = false;
    bool ok = SelectAndUseShield(c);
    assert(ok == true);
    assert(c->shield_info.pending == true);
    assert(c->items[SHIELD_8H].quantity == 0);
    assert(c->items[SHIELD_4H].quantity == 2);
    assert(c->items[SHIELD_1D].quantity == 1);
    assert(c->player.gems == 3000);
    assert(c->shield_alert_24h_active == false);

    /* Step 2: 8h exhausted, 4h shield must be consumed next */
    c->shield_info.pending = false;
    ok = SelectAndUseShield(c);
    assert(ok == true);
    assert(c->items[SHIELD_4H].quantity == 1);
    assert(c->items[SHIELD_1D].quantity == 1);
    assert(c->player.gems == 3000);
    assert(c->shield_alert_24h_active == false);

    /* Step 3: Consume second 4h shield */
    c->shield_info.pending = false;
    ok = SelectAndUseShield(c);
    assert(ok == true);
    assert(c->items[SHIELD_4H].quantity == 0);
    assert(c->items[SHIELD_1D].quantity == 1);
    assert(c->player.gems == 3000);
    assert(c->shield_alert_24h_active == false);

    /* Step 4: 8h and 4h exhausted -> forced to use 24h shield item! */
    c->shield_info.pending = false;
    ok = SelectAndUseShield(c);
    assert(ok == true);
    assert(c->items[SHIELD_1D].quantity == 0);
    assert(c->player.gems == 3000);
    assert(c->shield_alert_24h_active == true); // 24h alert locked!

    /* Step 5: 8h, 4h, and 24h items exhausted -> 24h crystal purchase (1000 gems) */
    c->shield_info.pending = false;
    ok = SelectAndUseShield(c);
    assert(ok == true);
    assert(c->player.gems == 2000);
    assert(c->shield_alert_24h_active == true);

    /* Step 6: Empty inventory and insufficient gems */
    c->player.gems = 500;
    c->shield_info.pending = false;
    ok = SelectAndUseShield(c);
    assert(ok == false); // Cannot afford shield
    assert(c->shield_info.pending == false);

    free(c);
    printf("[PASS] Shield item consumption ladder tests passed.\n");
}

/* 3. 24h Shield Shortage Alert Lock & Unlock Tests */
static void test_shield_alert_lock_and_unlock(void)
{
    printf("[TEST] Testing 24h shield shortage alert lock and unlock mechanism...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);
    c->sock = -1;

    /* Initially alert is inactive */
    assert(c->shield_alert_24h_active == false);

    /* Case A: Triggering lock via 24h inventory item */
    c->items[SHIELD_8H].quantity = 0;
    c->items[SHIELD_4H].quantity = 0;
    c->items[SHIELD_1D].quantity = 1;
    c->player.gems = 0;

    bool ok = SelectAndUseShield(c);
    assert(ok == true);
    assert(c->shield_alert_24h_active == true);

    /* Alert remains locked when calling update without replenishment */
    UpdateShieldAlertState(c);
    assert(c->shield_alert_24h_active == true);

    /* Replenish 4h shield -> unlock alert! */
    c->items[SHIELD_4H].quantity = 3;
    UpdateShieldAlertState(c);
    assert(c->shield_alert_24h_active == false);

    /* Case B: Triggering lock via 24h gem purchase */
    c->items[SHIELD_8H].quantity = 0;
    c->items[SHIELD_4H].quantity = 0;
    c->items[SHIELD_1D].quantity = 0;
    c->player.gems = 2000;
    c->shield_info.pending = false;

    ok = SelectAndUseShield(c);
    assert(ok == true);
    assert(c->player.gems == 1000);
    assert(c->shield_alert_24h_active == true);

    /* Replenish 8h shield -> unlock alert! */
    c->items[SHIELD_8H].quantity = 1;
    UpdateShieldAlertState(c);
    assert(c->shield_alert_24h_active == false);

    /* Case C: Automatic unlock via server RecvItemInfo bag packet */
    c->items[SHIELD_8H].quantity = 0;
    c->items[SHIELD_4H].quantity = 0;
    c->items[SHIELD_1D].quantity = 1;
    c->shield_info.pending = false;
    ok = SelectAndUseShield(c);
    assert(ok == true);
    assert(c->shield_alert_24h_active == true);

    uint8_t item_pkt[7];
    item_pkt[0] = (uint8_t)EMS_BeginAndEnd;
    write_u16(item_pkt + 1, 1);
    write_u16(item_pkt + 3, SHIELD_4H);
    write_u16(item_pkt + 5, 5);
    RecvItemInfo(c, item_pkt, sizeof(item_pkt));
    assert(c->items[SHIELD_4H].quantity == 5);
    assert(c->shield_alert_24h_active == false);

    free(c);
    printf("[PASS] 24h shield shortage alert lock and unlock tests passed.\n");
}

/* 4. Incoming March Whitelist Filtering Tests */
static void test_whitelist_march_filtering(void)
{
    printf("[TEST] Testing incoming march whitelist filtering...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);
    c->sock = -1;

    ParseWhitelist(c, "King Arthur, My Farm 01");
    c->mode = BOT_MODE_NORMAL;
    c->protection.enabled = true;
    c->protection.shield_on_incoming_attack = true;
    c->protection.shield_on_incoming_scout = true;
    c->shield_info.active = false;
    c->shield_info.pending = false;
    c->items[SHIELD_8H].quantity = 5;

    /* Whitelisted player attack: no shield triggered */
    WhenEnemyArmyApproachingTurf(c, "King Arthur");
    assert(c->shield_info.pending == false);
    assert(c->items[SHIELD_8H].quantity == 5);

    /* Whitelisted player scout: no shield triggered */
    WhenEnemyScoutApproachingTurf(c, "My Farm 01");
    assert(c->shield_info.pending == false);
    assert(c->items[SHIELD_8H].quantity == 5);

    /* Case-insensitive whitelisted match: no shield triggered */
    WhenEnemyArmyApproachingTurf(c, "king arthur");
    assert(c->shield_info.pending == false);
    assert(c->items[SHIELD_8H].quantity == 5);

    /* Unwhitelisted enemy army: instant shield triggered! */
    WhenEnemyArmyApproachingTurf(c, "Invader 99");
    assert(c->shield_info.pending == true);
    assert(c->items[SHIELD_8H].quantity == 4);

    /* Reset pending flag and test unwhitelisted scout */
    c->shield_info.pending = false;
    WhenEnemyScoutApproachingTurf(c, "Sneaky Scout");
    assert(c->shield_info.pending == true);
    assert(c->items[SHIELD_8H].quantity == 3);

    /* NULL attacker name: treated as unwhitelisted enemy */
    c->shield_info.pending = false;
    WhenEnemyArmyApproachingTurf(c, NULL);
    assert(c->shield_info.pending == true);
    assert(c->items[SHIELD_8H].quantity == 2);

    free(c);
    printf("[PASS] Incoming march whitelist filtering tests passed.\n");
}

/* 5. Harvest Mode Emergency Truncation Tests */
static void test_harvest_mode_emergency_truncation(void)
{
    printf("[TEST] Testing harvest mode third-party attack emergency truncation...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);
    c->sock = -1;

    ParseWhitelist(c, "King Arthur");
    c->mode = BOT_MODE_HARVEST;
    c->protection.enabled = true;
    c->protection.shield_on_incoming_attack = true;
    c->protection.shield_on_incoming_scout = true;
    c->shield_info.active = false;
    c->shield_info.pending = false;
    c->items[SHIELD_8H].quantity = 5;

    /* Whitelisted main account attacks to take resources: allow march without truncation */
    WhenEnemyArmyApproachingTurf(c, "King Arthur");
    assert(c->mode == BOT_MODE_HARVEST);
    assert(c->shield_info.pending == false);
    assert(c->items[SHIELD_8H].quantity == 5);

    /* Third-party unwhitelisted enemy attacks: emergency truncation triggered! */
    WhenEnemyArmyApproachingTurf(c, "ThirdPartyThief");
    assert(c->mode == BOT_MODE_NORMAL); // Harvest mode aborted!
    assert(c->shield_info.pending == true); // Emergency shield activated!
    assert(c->items[SHIELD_8H].quantity == 4);

    /* Test scout emergency truncation during harvest mode */
    c->mode = BOT_MODE_HARVEST;
    c->shield_info.pending = false;
    WhenEnemyScoutApproachingTurf(c, "EnemyScoutDrone");
    assert(c->mode == BOT_MODE_NORMAL);
    assert(c->shield_info.pending == true);
    assert(c->items[SHIELD_8H].quantity == 3);

    free(c);
    printf("[PASS] Harvest mode emergency truncation tests passed.\n");
}

/* 6. Shelter Packet Generation and State Transition Tests */
static void test_shelter_packets_and_state(void)
{
    printf("[TEST] Testing shelter packet generation and state transitions...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);
    c->sock = -1;
    c->server_time = 1700000000ULL;
    c->items[SHIELD_8H].quantity = 5;

    /* Initial shelter state */
    assert(c->troops_in_shelter == false);
    assert(c->shelter_expire_time == 0);

    /* 1. Direct RequestHideTroopInShelter (12 hours) */
    RequestHideTroopInShelter(c, 12, true, NULL);
    uint16_t opcode = read_u16(c->data + 2);
    assert(opcode == _MSG_REQUEST_HIDETROOPINSHELTER);
    assert(c->troops_in_shelter == true);
    assert(c->shelter_expire_time == 1700000000ULL + (12 * 3600));

    /* Test automatic troop packing and payload verification via DES decryption */
    c->troop.loaded = true;
    c->troop.infantry[0] = 500;
    c->troop.ranged[1] = 1500;
    c->troop.cavalry[2] = 2500;
    c->troop.siege[3] = 3500;
    RequestHideTroopInShelter(c, 12, true, NULL);
    assert(read_u16(c->data) == 74);
    assert(read_u16(c->data + 2) == _MSG_REQUEST_HIDETROOPINSHELTER);

    uint8_t dec_payload[70];
    DecryptData(c->data + 4, 70, dec_payload, ENCRYPTION_KEY);
    assert(dec_payload[4] == 12); // 12 hours
    assert(dec_payload[5] == 1);  // hide lord = true
    assert(read_u32(dec_payload + 6) == 500);                     // T1 infantry
    assert(read_u32(dec_payload + 6 + (4 + 1) * 4) == 1500);     // T2 ranged (offset: 4 inf + 1)
    assert(read_u32(dec_payload + 6 + (8 + 2) * 4) == 2500);     // T3 cavalry (offset: 8 inf/rng + 2)
    assert(read_u32(dec_payload + 6 + (12 + 3) * 4) == 3500);    // T4 siege (offset: 12 + 3)

    /* 2. Direct RequestReleaseShelterTroop */
    RequestReleaseShelterTroop(c);
    opcode = read_u16(c->data + 2);
    assert(opcode == _MSG_REQUEST_RELEASESHELTERTROOP);
    assert(c->troops_in_shelter == false);
    assert(c->shelter_expire_time == 0);

    /* 3. CLI harvest on -> Triggers RequestHideTroopInShelter */
    c->mode = BOT_MODE_NORMAL;
    CLI_ProcessInput(c, "harvest on");
    assert(c->mode == BOT_MODE_HARVEST);
    assert(c->troops_in_shelter == true);
    assert(c->shelter_expire_time == 1700000000ULL + (12 * 3600));
    opcode = read_u16(c->data + 2);
    assert(opcode == _MSG_REQUEST_HIDETROOPINSHELTER);

    /* 4. CLI harvest off -> Triggers RequestReleaseShelterTroop and shield restoration */
    c->shield_info.pending = false;
    CLI_ProcessInput(c, "harvest off");
    assert(c->mode == BOT_MODE_NORMAL);
    assert(c->troops_in_shelter == false);
    assert(c->shelter_expire_time == 0);
    assert(c->shield_info.pending == true); // Shield restored!
    assert(c->items[SHIELD_8H].quantity == 4);

    /* 5. Server response handlers */
    RecvHideTroopInShelter(c, NULL);
    assert(c->troops_in_shelter == true);

    RecvReleaseShelterTroop(c, NULL);
    assert(c->troops_in_shelter == false);
    assert(c->shelter_expire_time == 0);

    /* 6. CLI status dashboard check */
    c->troops_in_shelter = true;
    c->shield_alert_24h_active = true;
    CLI_ShowStatus(c);

    free(c);
    printf("[PASS] Shelter packet generation and state transition tests passed.\n");
}

int main(void)
{
    printf("============================================================\n");
    printf("   LORDS MOBILE BOT - PHASE 3 UNIT TESTS                    \n");
    printf("   Smart Shielding, Schedule, Alert, Whitelist & Shelter    \n");
    printf("============================================================\n");

    test_night_shield_boundaries();
    test_shield_consumption_ladder();
    test_shield_alert_lock_and_unlock();
    test_whitelist_march_filtering();
    test_harvest_mode_emergency_truncation();
    test_shelter_packets_and_state();

    printf("\n>>> ALL PHASE 3 UNIT TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
