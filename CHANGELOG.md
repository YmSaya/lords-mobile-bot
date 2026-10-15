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

### 變更 (Changed)
- 更新 .gitignore，將 .antigravity/ 納入忽略清單，並明確保留 graphify-out/ 與專案公開文檔受 Git 版本控制。
- 在 src/main.c 中將 Connection 結構體改為 Heap 動態配置（calloc），並確保所有提前退出與程式正常結束路徑乾淨釋放資源，消除 Windows MSVC 1MB 執行緒堆疊溢位隱患。
- 將 Configuration 與 CreateDefaultConfig 實作自 src/main.c 遷移至 src/config.c 並於 include/config.h 統一宣告，使組態邏輯模組化並支援獨立單元測試。
- 調整 src/main.c 指令列參數校驗順序，於 Winsock 初始化（WSAStartup）前完成引數驗證，防止非預期參數造成網路資源洩漏。
- 取消 src/main.c 中被註解的 Configuration(client) 呼叫，確保 bot.command_prefix 與各項預設配置欄位正常初始化。
- 強化 tests/test_phase1.c 單元測試套件，涵蓋真實 TCP 本地環回（Loopback）通訊、預設組態完整性檢驗、設定檔雙向解析、指令前綴檢查與 SuperUser 提權安全測試。

### 修復 (Fixed)
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
