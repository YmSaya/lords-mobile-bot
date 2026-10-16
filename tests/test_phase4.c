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
#include "items.h"
#include "map_point.h"

/* 1. Resource Tile Weighted Scoring and Priority Tests */
static void test_tile_scoring_and_priorities(void)
{
    printf("[TEST] Testing resource tile weighted scoring and priority algorithm...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);

    /* Set bot position at (100, 100) */
    PointCode pc = getPointCodeByMapPos(100, 100);
    c->player.zone_id = pc.zoneID;
    c->player.point_id = pc.pointID;

    /* Verify type priority ordering: Gem > Gold > Food > Ore > Wood > Stone
     * Create 6 identical Lv.1 tiles at identical distance (110, 100) and same remaining resources (50,000)
     */
    ResourceTile tiles[6];
    uint8_t kinds[6] = { PK_CRYSTAL, PK_GOLD, PK_FOOD, PK_IRON, PK_WOOD, PK_STONE };
    double scores[6];

    for (int i = 0; i < 6; i++) {
        memset(&tiles[i], 0, sizeof(tiles[i]));
        tiles[i].x = 110;
        tiles[i].y = 100;
        tiles[i].kind = kinds[i];
        tiles[i].level = 1;
        tiles[i].remaining_resources = 50000;
        tiles[i].max_resources = 50000;
        tiles[i].is_occupied = false;
        scores[i] = CalculateTileScore(c, &tiles[i]);
    }

    /* Assert strict descending order */
    assert(scores[0] > scores[1]); /* Gem > Gold */
    assert(scores[1] > scores[2]); /* Gold > Food */
    assert(scores[2] > scores[3]); /* Food > Ore */
    assert(scores[3] > scores[4]); /* Ore > Wood */
    assert(scores[4] > scores[5]); /* Wood > Stone */

    /* Type dominance check: even a rich Stone tile (250,000) must score below a moderate Wood tile (10,000) */
    ResourceTile rich_stone = {0};
    rich_stone.x = 110; rich_stone.y = 100;
    rich_stone.kind = PK_STONE; rich_stone.level = 2; rich_stone.remaining_resources = 250000;
    ResourceTile mod_wood = {0};
    mod_wood.x = 110; mod_wood.y = 100;
    mod_wood.kind = PK_WOOD; mod_wood.level = 1; mod_wood.remaining_resources = 10000;
    assert(CalculateTileScore(c, &mod_wood) > CalculateTileScore(c, &rich_stone));

    /* Distance decay test: closer tile must score higher */
    ResourceTile near_tile = {0};
    near_tile.x = 105;
    near_tile.y = 100; /* Distance = 5 */
    near_tile.kind = PK_FOOD;
    near_tile.level = 1;
    near_tile.remaining_resources = 50000;

    ResourceTile far_tile = {0};
    far_tile.x = 150;
    far_tile.y = 100; /* Distance = 50 */
    far_tile.kind = PK_FOOD;
    far_tile.level = 1;
    far_tile.remaining_resources = 50000;

    double near_score = CalculateTileScore(c, &near_tile);
    double far_score = CalculateTileScore(c, &far_tile);
    assert(near_score > far_score);

    /* Remaining resources weighting test: higher remaining must score higher */
    ResourceTile rich_tile = {0};
    rich_tile.x = 110;
    rich_tile.y = 100;
    rich_tile.kind = PK_FOOD;
    rich_tile.level = 1;
    rich_tile.remaining_resources = 150000;

    ResourceTile poor_tile = {0};
    poor_tile.x = 110;
    poor_tile.y = 100;
    poor_tile.kind = PK_FOOD;
    poor_tile.level = 1;
    poor_tile.remaining_resources = 50000;

    assert(CalculateTileScore(c, &rich_tile) > CalculateTileScore(c, &poor_tile));

    /* Low resource threshold test (too low to gather: < 1000) */
    ResourceTile low_tile = {0};
    low_tile.x = 110;
    low_tile.y = 100;
    low_tile.kind = PK_FOOD;
    low_tile.level = 1;
    low_tile.remaining_resources = 999;
    assert(CalculateTileScore(c, &low_tile) == 0.0);

    low_tile.remaining_resources = 0;
    assert(CalculateTileScore(c, &low_tile) == 0.0);

    /* Level restriction test: only Lv.1 and Lv.2 valid */
    ResourceTile lvl_tile = {0};
    lvl_tile.x = 110;
    lvl_tile.y = 100;
    lvl_tile.kind = PK_FOOD;
    lvl_tile.remaining_resources = 50000;

    lvl_tile.level = 0;
    assert(CalculateTileScore(c, &lvl_tile) == 0.0);
    lvl_tile.level = 1;
    assert(CalculateTileScore(c, &lvl_tile) > 0.0);
    lvl_tile.level = 2;
    assert(CalculateTileScore(c, &lvl_tile) > 0.0);
    lvl_tile.level = 3;
    assert(CalculateTileScore(c, &lvl_tile) == 0.0);
    lvl_tile.level = 4;
    assert(CalculateTileScore(c, &lvl_tile) == 0.0);
    lvl_tile.level = 5;
    assert(CalculateTileScore(c, &lvl_tile) == 0.0);

    /* Occupied tile test: cannot gather occupied tile */
    ResourceTile occ_tile = {0};
    occ_tile.x = 110;
    occ_tile.y = 100;
    occ_tile.kind = PK_CRYSTAL;
    occ_tile.level = 1;
    occ_tile.remaining_resources = 50000;
    occ_tile.is_occupied = true;
    assert(CalculateTileScore(c, &occ_tile) == 0.0);

    /* Active march collision avoidance: tile already targeted by active march must score 0.0 */
    poor_tile.x = 120; poor_tile.y = 100;
    rich_tile.x = 115; rich_tile.y = 100;
    c->tile_count = 3;
    c->tiles[0] = poor_tile;
    c->tiles[1] = rich_tile;
    c->tiles[2] = tiles[0]; /* Gem tile at (110, 100) */
    const ResourceTile *best = FindBestResourceTile(c);
    assert(best != NULL);
    assert(best->kind == PK_CRYSTAL);

    /* March 0 targets the Gem tile */
    c->marches[0].active = true;
    c->marches[0].state = MARCH_STATE_OUTGOING;
    c->marches[0].target_x = 110;
    c->marches[0].target_y = 100;

    /* Gem tile must now evaluate to 0.0, and FindBestResourceTile must pick next best tile (rich food) */
    assert(CalculateTileScore(c, &c->tiles[2]) == 0.0);
    const ResourceTile *next_best = FindBestResourceTile(c);
    assert(next_best != NULL);
    assert(next_best->kind == PK_FOOD);
    assert(next_best->remaining_resources == 150000);

    c->marches[0].active = false;

    free(c);
    printf("[PASS] Resource tile weighted scoring and priority tests passed.\n");
}

/* 2. Precision Troop Payload and Tier Dispatch Ladder Tests */
static void test_precision_troop_payload_and_ladder(void)
{
    printf("[TEST] Testing precision troop payload and tier dispatch ladder...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);

    /* Verify payload per slot */
    assert(GetSlotPayload(T1_SIEGE) == 20);
    assert(GetSlotPayload(T1_INFANTRY) == 8);
    assert(GetSlotPayload(T1_CAVALRY) == 8);
    assert(GetSlotPayload(T1_RANGED) == 8);
    assert(GetSlotPayload(T2_SIEGE) == 24);
    assert(GetSlotPayload(T2_INFANTRY) == 10);
    assert(GetSlotPayload(T3_SIEGE) == 28);
    assert(GetSlotPayload(T4_SIEGE) == 32);

    /* Case A: Sufficient T1 siege available -> exactly match payload with T1 siege only */
    c->troop.loaded = true;
    c->troop.siege[0] = 1000; /* 1000 T1 siege available (20,000 capacity) */
    TroopDispatch dispatch = {0};

    bool ok = CalculateGatherTroops(c, 10000, &dispatch);
    assert(ok == true);
    assert(dispatch.troops[T1_SIEGE] == 500); /* 500 * 20 = 10,000 */
    assert(dispatch.total_troops == 500);
    assert(dispatch.total_payload == 10000);
    for (int i = 0; i < 16; i++) {
        if (i != T1_SIEGE) assert(dispatch.troops[i] == 0);
    }

    /* Case B: Insufficient T1 siege -> filled with T1 infantry and cavalry */
    memset(&c->troop, 0, sizeof(c->troop));
    c->troop.loaded = true;
    c->troop.siege[0] = 200;    /* 200 * 20 = 4,000 payload */
    c->troop.infantry[0] = 500; /* 500 * 8 = 4,000 payload */
    c->troop.cavalry[0] = 300;  /* 300 * 8 = 2,400 payload */
    memset(&dispatch, 0, sizeof(dispatch));

    ok = CalculateGatherTroops(c, 10000, &dispatch);
    assert(ok == true);
    assert(dispatch.troops[T1_SIEGE] == 200);      /* 4,000 */
    assert(dispatch.troops[T1_INFANTRY] == 500);   /* 4,000 */
    assert(dispatch.troops[T1_CAVALRY] == 250);    /* 2,000 */
    assert(dispatch.troops[T1_RANGED] == 0);
    assert(dispatch.total_payload == 10000);
    assert(dispatch.total_troops == 200 + 500 + 250);

    /* Case C: T1 exhausted -> filled with T2 and T3 */
    memset(&c->troop, 0, sizeof(c->troop));
    c->troop.loaded = true;
    c->troop.siege[1] = 100;    /* T2 siege: 100 * 24 = 2,400 */
    c->troop.infantry[1] = 200; /* T2 inf: 200 * 10 = 2,000 */
    c->troop.siege[2] = 200;    /* T3 siege: 200 * 28 = 5,600 */
    memset(&dispatch, 0, sizeof(dispatch));

    ok = CalculateGatherTroops(c, 10000, &dispatch);
    assert(ok == true);
    assert(dispatch.troops[T2_SIEGE] == 100);
    assert(dispatch.troops[T2_INFANTRY] == 200);
    assert(dispatch.troops[T3_SIEGE] == 200);
    assert(dispatch.total_payload == 2400 + 2000 + 5600);
    assert(dispatch.total_payload == 10000);

    /* Case D: Precision dispatch without over-dispatching */
    memset(&c->troop, 0, sizeof(c->troop));
    c->troop.loaded = true;
    c->troop.siege[0] = 50000; /* 50,000 T1 siege */
    memset(&dispatch, 0, sizeof(dispatch));

    ok = CalculateGatherTroops(c, 2000, &dispatch);
    assert(ok == true);
    assert(dispatch.troops[T1_SIEGE] == 100); /* Exactly 100 troops, not 50,000 */
    assert(dispatch.total_troops == 100);
    assert(dispatch.total_payload == 2000);

    /* Case E: Small remainder handling (e.g. 1005 resources needed) with T1 siege */
    memset(&c->troop, 0, sizeof(c->troop));
    c->troop.loaded = true;
    c->troop.siege[0] = 100;
    c->troop.infantry[0] = 10;
    memset(&dispatch, 0, sizeof(dispatch));

    ok = CalculateGatherTroops(c, 1005, &dispatch);
    assert(ok == true);
    assert(dispatch.troops[T1_SIEGE] == 51); /* 51 * 20 = 1020 */
    assert(dispatch.troops[T1_INFANTRY] == 0);
    assert(dispatch.total_payload == 1020);

    /* Case F: Remainder where T1 siege alone covers smaller payload (e.g. 15 needed with 10 siege + 10 inf)
     * Must dispatch exactly 1 T1 siege (20 payload), 0 infantry!
     */
    memset(&c->troop, 0, sizeof(c->troop));
    c->troop.loaded = true;
    c->troop.siege[0] = 10;
    c->troop.infantry[0] = 10;
    memset(&dispatch, 0, sizeof(dispatch));

    ok = CalculateGatherTroops(c, 15, &dispatch);
    assert(ok == true);
    assert(dispatch.troops[T1_SIEGE] == 1);
    assert(dispatch.troops[T1_INFANTRY] == 0);
    assert(dispatch.total_troops == 1);
    assert(dispatch.total_payload == 20);

    /* Case G: Partial T1 siege shortage (25 needed with only 1 T1 siege and 10 T1 inf)
     * 1 T1 siege (20) + 1 T1 inf (8) = 28 payload
     */
    memset(&c->troop, 0, sizeof(c->troop));
    c->troop.loaded = true;
    c->troop.siege[0] = 1;
    c->troop.infantry[0] = 10;
    memset(&dispatch, 0, sizeof(dispatch));

    ok = CalculateGatherTroops(c, 25, &dispatch);
    assert(ok == true);
    assert(dispatch.troops[T1_SIEGE] == 1);
    assert(dispatch.troops[T1_INFANTRY] == 1);
    assert(dispatch.total_payload == 28);

    /* Case H: 25 needed with 10 T1 siege -> exactly 2 T1 siege, 0 infantry */
    memset(&c->troop, 0, sizeof(c->troop));
    c->troop.loaded = true;
    c->troop.siege[0] = 10;
    c->troop.infantry[0] = 10;
    memset(&dispatch, 0, sizeof(dispatch));

    ok = CalculateGatherTroops(c, 25, &dispatch);
    assert(ok == true);
    assert(dispatch.troops[T1_SIEGE] == 2);
    assert(dispatch.troops[T1_INFANTRY] == 0);
    assert(dispatch.total_payload == 40);

    /* Case I: Army payload less than needed -> dispatches all available troops */
    memset(&c->troop, 0, sizeof(c->troop));
    c->troop.loaded = true;
    c->troop.siege[0] = 50; /* 50 * 20 = 1000 payload */
    memset(&dispatch, 0, sizeof(dispatch));

    ok = CalculateGatherTroops(c, 50000, &dispatch);
    assert(ok == true);
    assert(dispatch.troops[T1_SIEGE] == 50);
    assert(dispatch.total_payload == 1000);

    /* Case J: Empty army -> returns false */
    memset(&c->troop, 0, sizeof(c->troop));
    memset(&dispatch, 0, sizeof(dispatch));
    ok = CalculateGatherTroops(c, 5000, &dispatch);
    assert(ok == false);

    free(c);
    printf("[PASS] Precision troop payload and tier dispatch ladder tests passed.\n");
}

/* 3. Tile Collision Detection and Emergency Troop Recall Tests */
static void test_tile_collision_and_emergency_recall(void)
{
    printf("[TEST] Testing tile collision detection and emergency troop recall...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);
    c->sock = -1;

    ParseWhitelist(c, "King Arthur, FarmOwner");

    /* Setup active gathering march targeting (120, 250) */
    c->marches[0].index = 0;
    c->marches[0].active = true;
    c->marches[0].state = MARCH_STATE_GATHERING;
    c->marches[0].target_x = 120;
    c->marches[0].target_y = 250;
    c->marches[0].total_troops = 500;
    c->items[WITHDRAW_SQUAD].quantity = 2;

    /* Case A: Whitelisted ally approaches gathering tile -> no recall */
    WhenEnemyApproachingGather(c, 120, 250, "King Arthur");
    assert(c->marches[0].active == true);
    assert(c->marches[0].state == MARCH_STATE_GATHERING);
    assert(c->items[WITHDRAW_SQUAD].quantity == 2);

    /* Case B: Hostile enemy approaches a different tile (300, 400) -> unaffected */
    WhenEnemyApproachingGather(c, 300, 400, "EnemyRaider");
    assert(c->marches[0].active == true);
    assert(c->marches[0].state == MARCH_STATE_GATHERING);
    assert(c->items[WITHDRAW_SQUAD].quantity == 2);

    /* Case C: Hostile unwhitelisted enemy approaches gathering tile (120, 250) -> emergency recall! */
    WhenEnemyApproachingGather(c, 120, 250, "EnemyRaider");
    assert(c->marches[0].state == MARCH_STATE_RETURNING);
    assert(c->items[WITHDRAW_SQUAD].quantity == 1); /* Item consumed */

    uint16_t opcode = read_u16(c->data + 2);
    assert(opcode == _MSG_REQUEST_USEITEM);

    uint8_t dec[32] = {0};
    DecryptData(c->data + 4, c->size - 4, dec, ENCRYPTION_KEY);
    uint16_t item_id = read_u16(dec + 4);
    assert(item_id == WITHDRAW_SQUAD);
    uint8_t march_idx = read_u8(dec + 8);
    assert(march_idx == 0);

    /* Case D: Another hostile approaches the same tile while march is ALREADY returning -> no duplicate recall */
    c->size = 0;
    memset(c->data, 0, 32);
    WhenEnemyApproachingGather(c, 120, 250, "AnotherHostile");
    assert(c->marches[0].state == MARCH_STATE_RETURNING);
    assert(c->items[WITHDRAW_SQUAD].quantity == 1); /* Quantity not decremented again */
    assert(c->size == 0); /* No packet sent */

    /* Case E: Hostile approaches when bot has 0 Withdraw Squad items -> no recall packet sent */
    c->marches[1].index = 1;
    c->marches[1].active = true;
    c->marches[1].state = MARCH_STATE_GATHERING;
    c->marches[1].target_x = 220;
    c->marches[1].target_y = 330;
    c->items[WITHDRAW_SQUAD].quantity = 0; /* Out of stock */

    c->size = 0;
    WhenEnemyApproachingGather(c, 220, 330, "SneakyAttacker");
    assert(c->marches[1].state == MARCH_STATE_GATHERING); /* Remains gathering */
    assert(c->size == 0); /* Packet aborted due to 0 items */

    free(c);
    printf("[PASS] Tile collision detection and emergency troop recall tests passed.\n");
}

/* 4. Harvest Mode Collaboration and Secondary Shelter Defense Tests */
static void test_harvest_mode_gather_collaboration(void)
{
    printf("[TEST] Testing harvest mode gather pause and secondary shelter defense...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);
    c->sock = -1;

    c->troop.loaded = true;
    c->troop.infantry[0] = 100;
    c->troop.siege[0] = 500;
    c->troop.total = 600;

    /* Setup a cached tile at (75, 80) */
    c->tile_count = 1;
    c->tiles[0].x = 75;
    c->tiles[0].y = 80;
    c->tiles[0].kind = PK_FOOD;
    c->tiles[0].level = 1;
    c->tiles[0].remaining_resources = 50000;
    c->tiles[0].is_occupied = true;

    /* Initial state: normal mode, gather active */
    assert(c->mode == BOT_MODE_NORMAL);
    assert(c->gather_paused == false);

    /* Step 1: CLI harvest on -> pauses gather and hides castle troops in shelter */
    CLI_ProcessInput(c, "harvest on");
    assert(c->mode == BOT_MODE_HARVEST);
    assert(c->gather_paused == true);
    assert(c->troops_in_shelter == true);

    /* Step 2: Attempting new gather march during harvest mode must be rejected */
    TroopDispatch dispatch = {0};
    dispatch.troops[T1_SIEGE] = 50;
    dispatch.total_troops = 50;
    dispatch.total_payload = 1000;

    bool sent = SendGatherMarch(c, 50, 50, &dispatch);
    assert(sent == false); /* Rejected while gather_paused */
    assert(c->player.current_marches == 0);

    /* CLI gather on while in harvest mode must NOT unpause gathering */
    CLI_ProcessInput(c, "gather on");
    assert(c->gather_enabled == true);
    assert(c->gather_paused == true); /* Still paused because mode is HARVEST */

    /* Step 3: Outside gathering march returns during harvest mode -> automatic secondary shelter defense */
    c->marches[1].index = 1;
    c->marches[1].active = true;
    c->marches[1].state = MARCH_STATE_GATHERING;
    c->marches[1].target_x = 75;
    c->marches[1].target_y = 80;
    c->marches[1].troops[T1_SIEGE] = 200;
    c->marches[1].total_troops = 200;
    c->marches[1].payload = 4000;
    c->player.current_marches = 1;

    /* March completes and returns to turf */
    OnMarchReturn(c, 1);
    assert(c->marches[1].active == false);
    assert(c->player.current_marches == 0);

    /* Cached tile resources deducted by march payload (50000 - 4000 = 46000) */
    assert(c->tiles[0].remaining_resources == 46000);
    assert(c->tiles[0].is_occupied == false);

    /* Automatic secondary shelter request dispatched to maintain 0 garrison! */
    uint16_t opcode = read_u16(c->data + 2);
    assert(opcode == _MSG_REQUEST_HIDETROOPINSHELTER);
    assert(c->troops_in_shelter == true);

    /* Step 4: CLI harvest off -> resumes normal mode, releases shelter, restores shield, allows gather */
    c->shield_info.pending = false;
    c->items[SHIELD_8H].quantity = 2;
    CLI_ProcessInput(c, "harvest off");
    assert(c->mode == BOT_MODE_NORMAL);
    assert(c->gather_paused == false);
    assert(c->troops_in_shelter == false);
    assert(c->shield_info.pending == true);

    /* Dispatch gather march now succeeds */
    c->troop.loaded = true;
    c->troop.siege[0] = 500;
    c->troop.total = 500;
    sent = SendGatherMarch(c, 50, 50, &dispatch);
    assert(sent == true);
    assert(c->player.current_marches == 1);
    assert(read_u16(c->data + 2) == _MSG_REQUEST_TROOPMARCH_NOTATK);

    free(c);
    printf("[PASS] Harvest mode gather pause and secondary shelter defense tests passed.\n");
}

/* 5. Max March Limit and Boundary Protection Tests */
static void test_max_march_limit_protection(void)
{
    printf("[TEST] Testing max march queue capacity and boundary protection...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);
    c->sock = -1;

    c->player.max_marches = 3;
    c->player.current_marches = 3; /* Queue full */

    c->troop.loaded = true;
    c->troop.siege[0] = 5000;
    c->troop.total = 5000;

    TroopDispatch dispatch = {0};
    dispatch.troops[T1_SIEGE] = 100;
    dispatch.total_troops = 100;
    dispatch.total_payload = 2000;

    /* When queue is full, dispatch must be rejected */
    bool sent = SendGatherMarch(c, 10, 10, &dispatch);
    assert(sent == false);
    assert(c->player.current_marches == 3);

    /* March returns -> capacity freed */
    c->marches[0].index = 0;
    c->marches[0].active = true;
    c->marches[0].total_troops = 100;
    c->marches[0].troops[T1_SIEGE] = 100;
    OnMarchReturn(c, 0);
    assert(c->player.current_marches == 2);

    /* Dispatch now succeeds */
    sent = SendGatherMarch(c, 10, 10, &dispatch);
    assert(sent == true);
    assert(c->player.current_marches == 3);

    free(c);
    printf("[PASS] Max march queue capacity and boundary protection tests passed.\n");
}

/* 6. Map Info Packet Decoding and Non-Resource Tile Filtering Tests */
static void test_map_packet_decoding(void)
{
    printf("[TEST] Testing map info packet decoding and non-resource tile filtering...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);

    /* Construct sample map packet with 5 tiles:
     * Tile 0: Gem (PK_CRYSTAL), Lv.1, (100, 200), remaining 50000, unoccupied
     * Tile 1: Monster (PK_NPC), Lv.2, (110, 210) -> Non-resource, must be filtered out!
     * Tile 2: Gold (PK_GOLD), Lv.2, (150, 250), remaining 80000, unoccupied
     * Tile 3: City (PK_CITY), Lv.25, (160, 260) -> Non-resource, must be filtered out!
     * Tile 4: Food (PK_FOOD), Lv.1, (120, 180), remaining 200000, unoccupied
     */
    uint8_t pkt[2 + 5 * 15];
    uint16_t offset = 0;

    write_u16(pkt + offset, 5); offset += 2; /* 5 entries */

    /* Tile 0: Gem */
    write_u16(pkt + offset, 100); offset += 2;
    write_u16(pkt + offset, 200); offset += 2;
    write_u8(pkt + offset, PK_CRYSTAL); offset += 1;
    write_u8(pkt + offset, 1); offset += 1;
    write_u32(pkt + offset, 50000); offset += 4;
    write_u32(pkt + offset, 50000); offset += 4;
    write_u8(pkt + offset, 0); offset += 1; /* Unoccupied */

    /* Tile 1: Monster (PK_NPC) */
    write_u16(pkt + offset, 110); offset += 2;
    write_u16(pkt + offset, 210); offset += 2;
    write_u8(pkt + offset, PK_NPC); offset += 1;
    write_u8(pkt + offset, 2); offset += 1;
    write_u32(pkt + offset, 0); offset += 4;
    write_u32(pkt + offset, 0); offset += 4;
    write_u8(pkt + offset, 0); offset += 1;

    /* Tile 2: Gold */
    write_u16(pkt + offset, 150); offset += 2;
    write_u16(pkt + offset, 250); offset += 2;
    write_u8(pkt + offset, PK_GOLD); offset += 1;
    write_u8(pkt + offset, 2); offset += 1;
    write_u32(pkt + offset, 80000); offset += 4;
    write_u32(pkt + offset, 80000); offset += 4;
    write_u8(pkt + offset, 0); offset += 1;

    /* Tile 3: City (PK_CITY) */
    write_u16(pkt + offset, 160); offset += 2;
    write_u16(pkt + offset, 260); offset += 2;
    write_u8(pkt + offset, PK_CITY); offset += 1;
    write_u8(pkt + offset, 25); offset += 1;
    write_u32(pkt + offset, 0); offset += 4;
    write_u32(pkt + offset, 0); offset += 4;
    write_u8(pkt + offset, 1); offset += 1;

    /* Tile 4: Food */
    write_u16(pkt + offset, 120); offset += 2;
    write_u16(pkt + offset, 180); offset += 2;
    write_u8(pkt + offset, PK_FOOD); offset += 1;
    write_u8(pkt + offset, 1); offset += 1;
    write_u32(pkt + offset, 200000); offset += 4;
    write_u32(pkt + offset, 200000); offset += 4;
    write_u8(pkt + offset, 0); offset += 1;

    assert(offset == sizeof(pkt));

    RecvMapInfoPlus(c, pkt, sizeof(pkt));

    /* Only the 3 resource tiles must be recorded; Monster and City safely ignored */
    assert(c->tile_count == 3);
    assert(c->tiles[0].x == 100 && c->tiles[0].y == 200);
    assert(c->tiles[0].kind == PK_CRYSTAL);
    assert(c->tiles[0].level == 1);
    assert(c->tiles[0].remaining_resources == 50000);

    assert(c->tiles[1].x == 150 && c->tiles[1].y == 250);
    assert(c->tiles[1].kind == PK_GOLD);
    assert(c->tiles[1].level == 2);
    assert(c->tiles[1].remaining_resources == 80000);

    assert(c->tiles[2].x == 120 && c->tiles[2].y == 180);
    assert(c->tiles[2].kind == PK_FOOD);
    assert(c->tiles[2].level == 1);
    assert(c->tiles[2].remaining_resources == 200000);

    /* Gem tile must be selected as best tile */
    const ResourceTile *best = FindBestResourceTile(c);
    assert(best != NULL);
    assert(best->kind == PK_CRYSTAL);

    free(c);
    printf("[PASS] Map info packet decoding and non-resource tile filtering tests passed.\n");
}

/* 7. Gather March Packet Serialization Layout Verification (_MSG_REQUEST_TROOPMARCH_NOTATK) */
static void test_gather_packet_serialization(void)
{
    printf("[TEST] Testing gather march packet serialization and troop layout...\n");

    Connection *c = calloc(1, sizeof(Connection));
    assert(c != NULL);
    Configuration(c);
    c->sock = -1;

    /* Setup castle troops: 500 T1 siege */
    c->troop.loaded = true;
    c->troop.siege[0] = 500;
    c->troop.total = 500;

    /* Setup 1 tile in c->tiles at (123, 456) */
    c->tile_count = 1;
    c->tiles[0].x = 123;
    c->tiles[0].y = 456;
    c->tiles[0].kind = PK_FOOD;
    c->tiles[0].level = 1;
    c->tiles[0].remaining_resources = 2000;
    c->tiles[0].is_occupied = false;

    TroopDispatch dispatch = {0};
    bool ok = CalculateGatherTroops(c, 2000, &dispatch);
    assert(ok == true);
    assert(dispatch.troops[T1_SIEGE] == 100);

    bool sent = SendGatherMarch(c, 123, 456, &dispatch);
    assert(sent == true);
    assert(c->tiles[0].is_occupied == true);

    /* Check packet opcode */
    uint16_t opcode = read_u16(c->data + 2);
    assert(opcode == _MSG_REQUEST_TROOPMARCH_NOTATK);

    /* Decrypt packet content */
    uint8_t dec[128] = {0};
    uint16_t enc_size = c->size - 4;
    DecryptData(c->data + 4, enc_size, dec, ENCRYPTION_KEY);

    /* Offset 4: target_x (2 bytes) */
    assert(read_u16(dec + 4) == 123);
    /* Offset 6: target_y (2 bytes) */
    assert(read_u16(dec + 6) == 456);
    /* Offset 8: march_type (1 byte) */
    assert(read_u8(dec + 8) == 1);

    /* Offset 9: 16 troop counts (64 bytes).
     * Lords Mobile protocol layout:
     * Slots 0..3:   Infantry T1..T4
     * Slots 4..7:   Ranged T1..T4
     * Slots 8..11:  Cavalry T1..T4
     * Slots 12..15: Siege T1..T4
     */
    for (int i = 0; i < 12; i++) {
        uint32_t count = read_u32(dec + 9 + i * 4);
        assert(count == 0); /* No infantry, ranged, or cavalry */
    }
    /* Slot 12: T1 Siege must be 100! */
    assert(read_u32(dec + 9 + 12 * 4) == 100);
    /* Slots 13..15: T2..T4 Siege must be 0 */
    assert(read_u32(dec + 9 + 13 * 4) == 0);
    assert(read_u32(dec + 9 + 14 * 4) == 0);
    assert(read_u32(dec + 9 + 15 * 4) == 0);

    free(c);
    printf("[PASS] Gather march packet serialization and troop layout tests passed.\n");
}

int main(void)
{
    printf("============================================================\n");
    printf("   LORDS MOBILE BOT - PHASE 4 UNIT TESTS                    \n");
    printf("   Map Scan, Precision Gathering & Tile Collision Evasion   \n");
    printf("============================================================\n");

    test_tile_scoring_and_priorities();
    test_precision_troop_payload_and_ladder();
    test_tile_collision_and_emergency_recall();
    test_harvest_mode_gather_collaboration();
    test_max_march_limit_protection();
    test_map_packet_decoding();
    test_gather_packet_serialization();

    printf("\n>>> ALL PHASE 4 UNIT TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
