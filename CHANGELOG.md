# 更新日誌 (Changelog)

本專案的所有重要變更均將記錄於此文件中。

本格式基於 [Keep a Changelog](https://keepachangelog.com/zh-TW/1.0.0/)，
且本專案遵循 [語意化版本 2.0.0](https://semver.org/lang/zh-TW/)。

## [未發布] (Unreleased)

### 新增 (Added)
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
