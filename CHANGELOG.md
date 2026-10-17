# 更新日誌 (Changelog)

本專案的所有重要變更均將記錄於此文件中。

本格式基於 [Keep a Changelog](https://keepachangelog.com/zh-TW/1.0.0/)，
且本專案遵循 [語意化版本 2.0.0](https://semver.org/lang/zh-TW/)。

## [未發布] (Unreleased)

### 新增 (Added)
- 建立 Phase 5 自動抓取更新、Discord.js v14 側車與完全自主運作實作計畫與總結文檔 (.antigravity/Plan/phase5_auto_update_and_discord_v14/plan.md, .antigravity/Memory/phase5_auto_update_and_discord_v14/summary.md)。
- 實作 C 核心 Localhost TCP IPC 本機通訊服務模組 (include/ipc.h, src/ipc.c)，以背景執行緒安全監聽 127.0.0.1:28888 迴環介面，採用輕量 JSON-Line 換行分隔協定雙向傳遞控制指令與狀態回傳，並於連線端加入 select 逾時機制防範慢速請求掛死。
- 實作 Discord.js v14 側車機器人 (discord-bot/)，包含 package.json (discord.js@^14.14.1)、index.js、deploy-commands.js 與說明手冊 README.md，註冊 /status、/harvest、/shield、/gather、/reload、/update、/help 應用程式斜線指令。
- 實作 Discord 全指令嚴格管理員身分鎖定（Admin Whitelist Lock），全面校驗 interaction.user.id 是否於 config.cfg 之 discord.admin_users 白名單中，非白名單成員一律回傳短暫性拒絕提示「無權限執行此指令」。
- 實作全指令純 ASCII 結構化 Discord Embed 狀態與操作回覆儀表板，嚴格禁止任何 Emoji 表情符號。
- 實作語意化版本比對演算法 (SemVer_Parse, SemVer_Compare)，支援 Major.Minor.Patch 高低判定、前置 'v'/'V' 修剪、前後空白修剪與非十進位安全退避。
- 實作 GitHub Release 線上下載 (Updater_DownloadFile) 與 Windows 原生 MoveFileExA 就地重命名替換回滾保護平滑升級機制 (Updater_ReplaceAndRollback, Updater_ExecuteUpdate)，支援 64KB 資訊緩衝區與自動跟隨 302 重定向下載新版至 lords-mobile-bot.exe.new，完成現役檔案重命名為 .old、.new 換入為現役檔案、換入失敗時立即重命名 .old 自動回滾復原，並透過 CreateProcess 啟動新版子進程後優雅退出舊版。
- 實作網路中斷指數退避自動重連模組 (ReconnectTracker, Reconnect_Init, Reconnect_GetNextBackoff, Reconnect_Reset)，連線中斷時依序以 2s -> 4s -> 8s -> 16s -> 32s -> 60s 進行退避重試，連入遊戲伺服器後立即重置，搭配 100ms 切片休眠並於等待期間持續呼叫 CLI_Tick 確保指令即時響應。
- 實作控制台 update 指令並擴充 help 指令清單。
- 新增 Phase 5 自動化單元測試套件與 CMake CTest 整合 (tests/test_phase5.c)，涵蓋 Localhost TCP IPC 通訊、SemVer 演算法判定、二進位替換與回滾保護、Discord 管理員身分授權過濾、夜間強制護盾 5 分鐘前續期與全局自主運作狀態機聯動。
- 建立 Phase 4 地圖資源掃描與精準負重採集實作計畫與總結文檔 (.antigravity/Plan/phase4_map_scan_and_auto_gather/plan.md, .antigravity/Memory/phase4_map_scan_and_auto_gather/summary.md)。
- 實作地圖資源封包解析函式 RecvMapInfoPlus，解碼 _MSG_RESP_UPDATE_MAPINFO_PLUS (2220) 提取座標、種類、等級、剩餘儲量、最大儲量與佔領狀態。
- 實作資源地塊加權選點演算法 (CalculateTileScore, FindBestResourceTile)，落實水晶>金>糧>鐵>木>石種類權重優先序、鎖定 Lv.1~Lv.2 資源區、歐幾里得距離衰減與低於 1000 門檻過低不採過濾。
- 實作精準負重派兵演算法與全軍階逐級調度 (CalculateGatherTroops)，以目標負重精準等於地塊剩餘資源量為基準，依序由 T1 攻城車（20 負重）優先派出、T1 步/騎/弓（8 負重）補足、T2/T3/T4 高階兵種補齊，精準匹配不超派。
- 實作採集行軍派兵封包 SendGatherMarch，封裝發送 _MSG_REQUEST_TROOPMARCH_NOTATK (6615) 行軍出兵封包，並維護 current_marches < max_marches 隊列上限保護。
- 實作防撞田被鎖定自動緊急撤退召回 (WhenEnemyApproachingGather)，監聽敵襲行軍衝突，白名單成員逼近放行，非白名單敵軍逼近採集點時立即自動觸發 RequestTroopRecall 召回保全兵力。
- 實作破盾取糧模式（Harvest Mode）協同機制：harvest on 立即暫停派發新採集隊列（gather_paused = true），在外採集隊列於取糧期間採完返城瞬間自動觸發二次藏兵（RequestHideTroopInShelter）送入藏兵洞，確保主城 100% 隨時維持 0 守軍；harvest off 自動釋放部隊、補盾並恢復常態自動採集。
- 支援控制台 gather <on|off> 指令與 CLI_ShowStatus 狀態儀表板 Gathering 狀態列即時顯示。
- 新增 Phase 4 自動化單元測試套件與 CMake CTest 整合 (tests/test_phase4.c)，涵蓋資源地塊評分加權、精準負重與軍階階梯調度、撞田敵襲緊急召回、取糧模式暫停派兵與返城二次藏兵 0 守軍、行軍隊列上限保護與地圖封包解碼。
- 建立 Phase 3 智慧護盾排程與全自動藏兵洞庇護實作計畫與總結文檔 (.antigravity/Plan/phase3_smart_shield_and_shelter/plan.md, .antigravity/Memory/phase3_smart_shield_and_shelter/summary.md)。
- 實作 UTC+08:00 夜間強制護盾排程演算法 (IsNightShieldTime)，精準覆蓋每日 00:30 ~ 06:30 邊界時段，支援 config.cfg 設定 protection.timezone_offset（預設為 8）與熱重載。
- 實作智慧護盾道具消耗階梯 (SelectAndUseShield)，嚴格遵循 8h 道具 > 4h 道具 > 24h 道具 > 24h 水晶購買（1000 鑽石）。
- 實作 24h 護盾缺糧警戒狀態鎖定 (shield_alert_24h_active) 與 UpdateShieldAlertState 解鎖機制，動用 24h 盾時持續鎖定高亮終端警報，直到 4h 或 8h 道具入庫為止。
- 實作全自動藏兵洞封包收發（RequestHideTroopInShelter, RequestReleaseShelterTroop, RecvHideTroopInShelter, RecvReleaseShelterTroop），支援 12 小時庇護、領主標誌與兵力配比。
- 實作破盾取糧模式（Harvest Mode）聯動：控制台 harvest on 自動將領主與全軍藏入藏兵洞 12 小時並暫停日常自動補盾；harvest off 自動釋放全軍與領主並秒補盾恢復日常防衛。
- 實作行軍敵襲白名單過濾防禦機制：白名單成員攻擊/偵察一律放行不開盾，支援主號順暢取糧；非白名單敵襲日常模式秒開盾。
- 實作破盾取糧模式第三方敵襲「緊急截斷」防護機制：取糧期間遭遇第三方非白名單敵軍逼近時，立即觸發蜂鳴音提示與高亮警告，強制中止取糧回滾至日常模式並秒開盾截斷防禦，保全城內所有資源。
- 新增 Phase 3 自動化單元測試套件與 CMake CTest 整合 (tests/test_phase3.c)，涵蓋夜間時段邊界判定、護盾消耗階梯、24h 警戒鎖定與解鎖、行軍白名單過濾、取糧第三方敵襲緊急截斷、藏兵洞封包生成與狀態轉換。
- 建立專案核心代理人開發規範與架構藍圖 (.antigravity/Gemini.md)。
- 初始化 Plan 與 Memory 雙重文檔管理狀態系統 (.antigravity/Plan/state.md, .antigravity/Memory/state.md)。
- 建立 Phase 0 代碼審查與前期調研批判性修正總結 (.antigravity/Memory/phase0_codebase_audit/audit_summary.md)。
- 建立繁體中文規範之更新日誌 (CHANGELOG.md)。
- 建立 Phase 1 核心穩定加固實作計畫與總結文檔 (.antigravity/Plan/phase1_core_stabilization/plan.md, .antigravity/Memory/phase1_core_stabilization/summary.md)。
- 新增 Phase 1 自動化單元測試套件與 CMake CTest 整合 (tests/test_phase1.c)。
- 建立 Phase 2 控制台 CLI 互動線程與熱重載實作計畫與總結文檔 (.antigravity/Plan/phase2_console_cli_and_hot_reload/plan.md, .antigravity/Memory/phase2_console_cli_and_hot_reload/summary.md)。
- 建立獨立非同步控制台背景輸入線程與執行緒安全指令佇列 (include/cli.h, src/cli.c)。
- 支援控制台指令集：help, status, harvest on/off, shield on/off, reload, quit/exit。
- 支援結構化 ASCII 線框狀態儀表板，涵蓋連線狀態、運作模式、護盾剩餘時間、白名單成員、資源存量等，維持全專案 0 個 Emoji 表情符號。
- 支援主號白名單多帳號解析與專用 TrimWhitespace 函式，修剪前後空白並完整保留內部空格（如 "King Arthur"、"My Farm 01"）。
- 支援 IsPlayerWhitelisted 跨平台不區分大小寫白名單全文比對。
- 實作 ReloadConfig 設定檔熱重載機制，動態更新白名單與參數而不中斷既有 Socket 連線與 Token。
- 新增 Phase 2 自動化單元測試套件與 CMake CTest 整合 (tests/test_phase2.c)，涵蓋白名單解析、大小寫比對、指令驗證、熱重載安全與多執行緒指令佇列壓測。

### 變更 (Changed)
- 擴充 Connection 結構體，納入 DiscordSettings (ipc_port, bot_token, admin_users, admin_user_count)、ReconnectTracker (current_delay_sec, initial_delay_sec, max_delay_sec, reconnect_attempts, reconnecting)、ipc_thread 與 ipc_listen_sock 等 Phase 5 核心狀態欄位。
- 於 src/config.c 中支援 discord.ipc_port、discord.bot_token 與 discord.admin_users 鍵值解析、預設組態生成模板與熱重載設定同步。
- 於 src/main.c 中整合 Localhost TCP IPC 伺服器初始化 (IPC_Init) 與銷毀 (IPC_Cleanup)，並以指數退避演算法 (2s~60s) 重構連線主迴圈，達成全系統完全自主運作。
- 更新 CMakeLists.txt，將 src/ipc.c 與 src/updater.c 納入 client 與所有測試目標編譯，新增 test_phase5 目標，並於 WIN32 平台連結 winhttp 庫。
- 擴充 Connection 結構體，納入 gather_enabled、gather_paused、tile_count、tiles[MAX_MAP_TILES] 與 marches[MAX_MARCHES] 等 Phase 4 核心狀態欄位。
- 在 include/connection.h 統一定義 POINT_KIND、TroopSlot、MarchState、March、ResourceTile 與 TroopDispatch 結構體。
- 於 src/main.c 主循環 BotTick 中整合 AutoGatherTick 推進自動採集週期。
- 更新 CMakeLists.txt 新增 test_phase4 建置目標並登錄至 CTest 自動化測試。
- 擴充 RequestHideTroopInShelter：當未指定自訂部隊清單且 c->troop.loaded 為真時，自動打包現有全部 16 種兵階軍隊（T1~T4 步/弓/騎/車）送入藏兵洞，確保破盾取糧模式開啟時主城達成 0 守軍。
- 於 RecvItemInfo 與 RecvUseItem 伺服器背包封包處理中整合 UpdateShieldAlertState，使道具庫存更新時即時檢查並自動解除 24h 護盾缺糧警戒鎖定。
- 擴充 Connection.protection 結構體納入 timezone_offset 時區設定，並於 Connection 結構體擴充 shield_alert_24h_active、troops_in_shelter 與 shelter_expire_time 等 Phase 3 核心狀態欄位。
- 於 src/config.c 中支援 protection.timezone_offset 鍵值解析、預設組態生成模板與熱重載設定同步。
- 於 src/cli.c 的 CLI_ShowStatus 控制台狀態儀表板擴充 Shelter 藏兵狀態與 24H 缺糧高亮警報列顯示。
- 於 src/main.c 封包分發迴圈中接通 _MSG_RESP_HIDETROOPINSHELTER 與 _MSG_RESP_RELEASESHELTERTROOP 伺服器回應 opcode。
- 更新 CMakeLists.txt 新增 test_phase3 建置目標並登錄至 CTest 自動化測試。
- 調整 .gitignore 移除 tests/，確保專案單元測試原始碼正常納入 Git 版本控制。
- 擴充 Connection 結構體，納入運作模式 (BotMode)、白名單資料模型 (whitelist, whitelist_count)、執行狀態 (running)、設定檔路徑 (config_path) 與指令佇列 (CommandQueue)。
- 於 src/main.c 主循環 BotTick 中整合 CLI_Tick，達成非同步提取執行指令且不卡頓 TCP 串流接收。
- 於 src/main.c 中整合 CLI 初始化與乾淨清理生命週期，支援 quit/exit 指令優雅關閉。
- 於 src/config.c 的 LoadConfig 中將成功開啟之組態路徑同步記錄至 c->config_path，確保單元測試與各調用點狀態一致。
- 擴充 CreateDefaultConfig 預設組態生成模板，納入 whitelist.players 鍵值說明與預設範例。
- 擴充 tests/test_phase2.c 自動化單元測試，涵蓋大量空白修剪、Tab 鍵、連續逗號、超長名稱截斷、極端空格不區分大小寫匹配、CLI reload 指令回歸、quit 批次中斷與指令佇列滿載 64/65 邊界驗證。
- 更新 .gitignore，將 .antigravity/ 納入忽略清單，並明確保留 graphify-out/ 與專案公開文檔受 Git 版本控制。
- 在 src/main.c 中將 Connection 結構體改為 Heap 動態配置（calloc），並確保所有提前退出與程式正常結束路徑乾淨釋放資源，消除 Windows MSVC 1MB 執行緒堆疊溢位隱患。
- 將 Configuration 與 CreateDefaultConfig 實作自 src/main.c 遷移至 src/config.c 並於 include/config.h 統一宣告，使組態邏輯模組化並支援獨立單元測試。
- 調整 src/main.c 指令列參數校驗順序，於 Winsock 初始化（WSAStartup）前完成引數驗證，防止非預期參數造成網路資源洩漏。
- 取消 src/main.c 中被註解的 Configuration(client) 呼叫，確保 bot.command_prefix 與各項預設配置欄位正常初始化。
- 強化 tests/test_phase1.c 單元測試套件，涵蓋真實 TCP 本地環回（Loopback）通訊、預設組態完整性檢驗、設定檔雙向解析、指令前綴檢查與 SuperUser 提權安全測試。

### 修復 (Fixed)
- 修復 SendGatherMarch 封包 16 兵階序列未轉換官方協議排列格式，導致平鋪寫入將 T1 攻城車錯置為 T4 步兵之協議序列化重大缺陷。
- 修復 CalculateGatherTroops 精準負重派兵演算法在剩餘負重小於單位兵種負重時，整數除法得 0 誤跳過高優先級兵種並調用低階兵種之後又於補差階段加車引發的超派與雜兵問題，改採天花板除法 (Ceiling Division) 判定，實現首選兵種純粹覆蓋。
- 修復 CalculateTileScore 與 FindBestResourceTile 未檢查當前活躍行軍隊列目標，導致多隊列短時間內重疊撞向同一個資源地塊的重大邏輯缺陷。
- 修復 WhenEnemyApproachingGather 在撤退道具庫存為 0 時仍發送無效使用道具封包並將隊列誤標記為 RETURNING 的問題，並補齊重疊敵襲對已返程隊列重複觸發召回之狀態防護，以及召回成功時扣減背包撤退令庫存。
- 修復 OnMarchReturn 部隊返城時未同步扣減地圖快取地塊剩餘儲量與釋放佔用狀態，導致短期內對已採空地塊再次派兵的缺陷。
- 修復 RecvMapInfoPlus 未過濾野生怪 (PK_NPC) 與城市 (PK_CITY) 等非資源地塊，造成地塊緩衝區容量遭無效資料耗盡的邊界過濾缺陷。
- 修復 src/cli.c 中 gather on 指令在取糧模式 (BOT_MODE_HARVEST) 下誤將 c->gather_paused 清為 false 破壞取糧防禦暫停狀態的缺陷。
- 修復 include/protocol.h 中 RecvMapInfoPlus 重複宣告的語法冗餘。
- 修復 tests/test_phase2.c 中區域變數 CommandQueue q 未初始化包含隨機堆疊垃圾，導致 CommandQueue_Init 誤判已初始化而未調用 InitializeCriticalSection，引發多執行緒壓測隨機崩潰（STATUS_INVALID_HANDLE 0xC0000008）之記憶體安全隱患。
- 修復 src/config.c 中 ParseWhitelist 因在去除前置空格前即依 sizeof(token) 截斷候選片段，導致逗號後大量空格（如 32 位元組以上空格）使玩家名稱遭截斷甚至丟失的解析錯誤。
- 修復 src/config.c 中 IsPlayerWhitelisted 依賴固定長度緩衝區與截斷順序導致前置大量空格玩家名稱比對失敗的缺陷，改以直接修整前後空白並以長度與大小寫不敏感比對。
- 修復 src/config.c 中 ReloadConfig 於重新載入相同組態路徑時傳入自身緩衝區指標作為來源引發 strncpy 記憶體重疊之未定義行為（Undefined Behavior）。
- 修復 src/cli.c 中 CLI_Cleanup 在控制台線程阻塞於 fgets 時未實質終止線程，導致 Connection 記憶體釋放後存取懸空指標與已銷毀臨界區之釋放後使用（Use-After-Free）重大隱患。
- 修復 src/cli.c 中 CLI_ProcessInput 執行 shield on 時未連動啟用 protection.enabled 主開關，導致底層 ShieldTick 因主開關未開啟而靜默略過開盾動作的邏輯缺陷。
- 修復 src/cli.c 中 CLI_Tick 於批次指令遇到 quit/exit 指令後未立即終止迴圈仍繼續執行後續命令的時序問題。
- 修復 src/cli.c 中 CLI_ShowStatus 控制台表格輸出未嚴格限制最大字元寬度，改用 %-76.76s 確保 80 字元線框絕對對齊。
- 修復 src/cli.c 中 CommandQueue_Init 重複呼叫未具備冪等性之防護，並於單元測試補齊佇列滿載與溢位拒絕之邊界驗證。
- 修復 src/connection.c 中 reset_connection 函式誤先設定 c->sock = -1 再呼叫 memset(c, 0, sizeof(*c)) 導致 socket 描述元被覆寫為 0 的邏輯順序錯誤。
- 修復 src/config.c 中 LoadConfig 在組態值為空字串或包含內部空格時的解析邏輯，提升鍵值解析容錯性。
- 修復單元測試在 CMake Release 組態（-DNDEBUG）下 assert 巨集遭編譯器優化忽略的問題，確保測試斷言始終強制執行。
- 修復 src/connection.c 中 reset_connection 誤將指標變數位址傳入 memset(&c, 0, sizeof(c)) 的致命錯誤，修正為重置 Connection 結構體本體並補齊空指標防護。
- 修復 src/connection.c 中 disconnect 遺漏大括號引發的誤導性縮排隱患，並加入空指標安全檢查。
- 修復 src/config.c 遺漏解析 data.path、protection.recall_on_incoming_attack、protection.recall_on_incoming_scout 與 protection.recall_on_incoming_conflict 的缺陷。
- 修復 src/config.c 中 protection.shield_always_on 誤指派給 protection.enabled 的錯誤，並清理重複之解析分支。
- 修復 src/main.c 中 ProcessConnection 於接收緩衝區滿溢 (read_pos == BUFFER_SIZE) 時 recv 返回 0 誤判為對端斷線退出的邏輯缺陷，並補強滿溢防死鎖滑動機制。
- 修復 src/main.c 中 ProcessConnection 於非阻塞 WSAEWOULDBLOCK 分支遺漏 continue 導致無數據時無效重試解析封包的 CPU 浪費。
- 修復 src/main.c 中 recv 緩衝區長度計算引發的 MSVC x64 C4267 型態截斷警告。
- 修復 src/main.c 中 logger 格式化輸出 64 位元整數 user_id 引發的 MSVC x64 C4477 格式字串警告。

### 安全 (Security)
- 修復 src/command.c 中 SuperUserAccess() 遭註解的管理員身份校驗，防止非授權玩家透過 $su 指令無條件竄奪 Bot 最高管理權限。
- 補強 src/command.c 中 SuperUserAccess 與 command_handler 之傳入參數空指標檢查，防範非法封包引發程式崩潰。
- 補齊 include/command.h 中 SuperUserAccess 函式原型宣告，並移除標頭檔中不當的靜態函式前置宣告。
