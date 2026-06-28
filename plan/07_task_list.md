# 07. 全フェーズ統合タスク一覧

> GitHub Issues 連動を想定したチェックボックス形式。各タスクに優先度（High/Medium/Low）・担当モジュール・依存関係を付記。
> 凡例: 優先度 = 🔴High / 🟡Medium / 🟢Low ／ 依存 = 前提タスク番号

---

## 共通基盤 / リポジトリ初期化 (Phase 0)

- [x] **T-00** リポジトリ初期ファイル整備（README/LICENSE/.gitignore/CONTRIBUTING/SECURITY） — 🔴High / `repo` / 依存: なし
- [x] **T-01** CMake プロジェクト初期化（C++20/Qt6/警告設定） — 🔴High / `infra.build` / 依存: T-00
- [x] **T-02** Qt 6 MainWindow の最小起動実装 — 🔴High / `app.ui` / 依存: T-01
- [x] **T-03** ディレクトリ構成作成（src/app,mcp,transport,timeline,security,config,ui,tests） — 🔴High / `repo` / 依存: T-01
- [x] **T-04** テスト基盤導入（GoogleTest または Catch2） — 🟡Medium / `infra.test` / 依存: T-01
- [x] **T-05** GitHub Actions build/test workflow 作成 — 🟡Medium / `infra.ci` / 依存: T-01
- [x] **T-06** 開発者向けセットアップ手順書作成 — 🟡Medium / `docs` / 依存: T-01
- [x] **T-07** コードフォーマット設定（clang-format/clang-tidy 方針） — 🟢Low / `infra.lint` / 依存: T-01
- [x] **T-08** アプリ基本設定（名称、バージョン、Organization、設定保存先） — 🟡Medium / `app` / 依存: T-02
- [x] **T-09** 最小 README と roadmap の整備 — 🟡Medium / `docs` / 依存: T-00

## MCP Core / JSON-RPC 基盤 (Phase 1)

- [x] **T-10** JSON-RPC message 型定義（request/response/notification/error） — 🔴High / `mcp.types` / 依存: T-03
- [x] **T-11** JSON parse/serialize ヘルパ実装 — 🔴High / `mcp.message` / 依存: T-10
- [x] **T-12** RequestId 採番・pending request 管理 — 🔴High / `mcp.client` / 依存: T-10
- [x] **T-13** MCP error 型・UI 表示用 error mapper — 🟡Medium / `mcp.error` / 依存: T-10
- [x] **T-14** protocol version / client info 型定義 — 🟡Medium / `mcp.types` / 依存: T-10
- [x] **T-15** initialize request/response 実装 — 🔴High / `mcp.client` / 依存: T-12,T-14
- [x] **T-16** initialized notification 実装 — 🔴High / `mcp.client` / 依存: T-15
- [x] **T-17** capabilities 型定義・保持 — 🟡Medium / `mcp.capabilities` / 依存: T-15
- [x] **T-18** timeout/cancel 用 request lifecycle 管理 — 🟡Medium / `mcp.client` / 依存: T-12
- [x] **T-19** MCP Core 単体テスト — 🔴High / `tests.mcp` / 依存: T-10〜T-18

## Transport / stdio (Phase 1)

- [x] **T-20** Transport 抽象 interface 定義 — 🔴High / `transport` / 依存: T-03
- [x] **T-21** ServerProfile 型定義（transport/command/args/env/cwd/timeout） — 🔴High / `config.profile` / 依存: T-03
- [x] **T-22** StdioTransport の QProcess 起動実装 — 🔴High / `transport.stdio` / 依存: T-20,T-21
- [x] **T-23** stdout newline-delimited JSON parser 実装 — 🔴High / `transport.stdio` / 依存: T-22,T-11
- [x] **T-24** stderr log capture 実装 — 🟡Medium / `transport.stdio` / 依存: T-22
- [x] **T-25** process exit / crash / start failure handling — 🔴High / `transport.stdio` / 依存: T-22
- [x] **T-26** Windows/macOS/Linux の command/args quote 動作確認 — 🟡Medium / `transport.stdio` / 依存: T-22
- [x] **T-27** stdout protocol violation 検出 — 🟡Medium / `transport.stdio` / 依存: T-23
- [x] **T-28** StdioTransport mock server 結合テスト — 🔴High / `tests.transport` / 依存: T-22〜T-27
- [x] **T-29** 接続/切断/再接続 API の整備 — 🔴High / `transport` / 依存: T-22,T-25

## Timeline / ログ可視化 (Phase 1)

- [x] **T-30** TimelineEvent 型定義 — 🔴High / `timeline` / 依存: T-10
- [x] **T-31** TimelineStore 実装（memory store） — 🔴High / `timeline` / 依存: T-30
- [x] **T-32** request/response の duration 計算 — 🟡Medium / `timeline` / 依存: T-12,T-31
- [x] **T-33** stderr log を TimelineEvent として保存 — 🟡Medium / `timeline` / 依存: T-24,T-31
- [x] **T-34** TimelineModel（Qt table model）実装 — 🔴High / `timeline.ui` / 依存: T-31
- [x] **T-35** TimelinePanel UI 実装 — 🔴High / `ui.timeline` / 依存: T-34
- [x] **T-36** Raw JSON detail viewer 実装 — 🔴High / `ui.json` / 依存: T-35
- [x] **T-37** Timeline filter（method/status/direction） — 🟡Medium / `ui.timeline` / 依存: T-35
- [x] **T-38** Timeline export JSON 実装 — 🟢Low / `timeline.export` / 依存: T-31
- [x] **T-39** Timeline 表示テスト / snapshot テスト — 🟡Medium / `tests.timeline` / 依存: T-34〜T-36

## Tools Inspector (Phase 1)

- [x] **T-40** McpTool 型定義（name/description/inputSchema） — 🔴High / `mcp.tool` / 依存: T-10
- [x] **T-41** tools/list 実装 — 🔴High / `mcp.client` / 依存: T-15,T-40
- [x] **T-42** tools/call 実装 — 🔴High / `mcp.client` / 依存: T-41
- [x] **T-43** ToolPanel 一覧 UI 実装 — 🔴High / `ui.tools` / 依存: T-41
- [x] **T-44** tool detail 表示（description/inputSchema） — 🔴High / `ui.tools` / 依存: T-43
- [x] **T-45** JSON 入力欄と validation — 🔴High / `ui.tools` / 依存: T-44
- [x] **T-46** tools/call 実行ボタンと結果表示 — 🔴High / `ui.tools` / 依存: T-42,T-45
- [x] **T-47** tool 実行 error 表示 — 🔴High / `ui.tools` / 依存: T-46,T-13
- [x] **T-48** tools/list / tools/call 結合テスト — 🔴High / `tests.mcp` / 依存: T-41,T-42
- [x] **T-49** MVP デモ profile 作成 — 🟡Medium / `examples.profiles` / 依存: T-46

## Profile Store / 接続 UI (Phase 1 → 5)

- [x] **T-50** ProfileStore memory 実装 — 🔴High / `config.profile` / 依存: T-21
- [x] **T-51** ProfileEditor 最小 UI（name/command/args/cwd/env） — 🔴High / `ui.profile` / 依存: T-50
- [x] **T-52** Connect/Disconnect toolbar 実装 — 🔴High / `app.ui` / 依存: T-29,T-51
- [x] **T-53** 最近使った profile 保存 — 🟡Medium / `config.profile` / 依存: T-50
- [x] **T-54** profile validation（空 command/不正 timeout/env key） — 🟡Medium / `config.validator` / 依存: T-21,T-51
- [x] **T-55** SQLite 導入・profile 永続化 — 🟡Medium / `storage.sqlite` / 依存: T-50
- [x] **T-56** profile clone/delete 実装 — 🟢Low / `ui.profile` / 依存: T-55
- [x] **T-57** env secret masking UI — 🔴High / `ui.profile` / 依存: T-51
- [x] **T-58** working directory picker — 🟡Medium / `ui.profile` / 依存: T-51
- [x] **T-59** 起動テスト専用ボタン — 🟡Medium / `ui.profile` / 依存: T-52,T-54

## Resources / Prompts / Notifications (Phase 2)

- [x] **T-60** McpResource 型定義 — 🔴High / `mcp.resource` / 依存: T-10
- [x] **T-61** resources/list 実装 — 🔴High / `mcp.client` / 依存: T-60
- [x] **T-62** resources/read 実装 — 🔴High / `mcp.client` / 依存: T-61
- [x] **T-63** ResourcePanel UI 実装 — 🟡Medium / `ui.resources` / 依存: T-61,T-62
- [x] **T-64** resource metadata / MIME type 表示 — 🟡Medium / `ui.resources` / 依存: T-63
- [x] **T-65** McpPrompt 型定義 — 🔴High / `mcp.prompt` / 依存: T-10
- [x] **T-66** prompts/list 実装 — 🔴High / `mcp.client` / 依存: T-65
- [x] **T-67** prompts/get 実装 — 🔴High / `mcp.client` / 依存: T-66
- [x] **T-68** PromptPanel UI 実装 — 🟡Medium / `ui.prompts` / 依存: T-66,T-67
- [x] **T-69** notifications/logging/progress の受信・表示 — 🔴High / `mcp.client` / 依存: T-31

## Schema-driven Form (Phase 2)

- [x] **T-70** JSON Schema subset parser 実装 — 🔴High / `ui.schema` / 依存: T-40
- [x] **T-71** string/number/boolean 入力 widget 生成 — 🔴High / `ui.schema` / 依存: T-70
- [x] **T-72** enum/select 入力 widget 生成 — 🟡Medium / `ui.schema` / 依存: T-70
- [x] **T-73** object nesting の表示 — 🟡Medium / `ui.schema` / 依存: T-70
- [x] **T-74** array 入力の簡易対応 — 🟢Low / `ui.schema` / 依存: T-70
- [x] **T-75** required field validation — 🔴High / `ui.schema` / 依存: T-71
- [x] **T-76** form → JSON arguments 生成 — 🔴High / `ui.schema` / 依存: T-71,T-75
- [x] **T-77** JSON input mode / Form mode 切替 — 🟡Medium / `ui.tools` / 依存: T-76
- [x] **T-78** schema form 単体テスト — 🟡Medium / `tests.ui.schema` / 依存: T-70〜T-76
- [x] **T-79** invalid schema fallback 表示 — 🟡Medium / `ui.schema` / 依存: T-70

## Streamable HTTP Transport (Phase 3)

- [x] **T-80** HTTP endpoint profile 型追加 — 🔴High / `config.profile` / 依存: T-21
- [x] **T-81** StreamableHttpTransport skeleton — 🔴High / `transport.http` / 依存: T-20,T-80
- [x] **T-82** HTTP POST request 実装 — 🔴High / `transport.http` / 依存: T-81
- [x] **T-83** application/json response parse — 🔴High / `transport.http` / 依存: T-82
- [x] **T-84** text/event-stream response parse — 🔴High / `transport.http` / 依存: T-82
- [x] **T-85** GET SSE stream 受信 — 🟡Medium / `transport.http` / 依存: T-84
- [x] **T-86** Mcp-Session-Id 管理 — 🔴High / `transport.http` / 依存: T-83,T-84
- [x] **T-87** MCP-Protocol-Version header 管理 — 🔴High / `transport.http` / 依存: T-15,T-86
- [x] **T-88** HTTP error / JSON-RPC error 分離表示 — 🟡Medium / `transport.http` / 依存: T-83
- [x] **T-89** HTTP transport 結合テスト — 🔴High / `tests.transport` / 依存: T-82〜T-88

## Security Workbench (Phase 4)

- [x] **T-90** RiskFinding / RiskScore 型定義 — 🔴High / `security` / 依存: T-21,T-40
- [x] **T-91** RuleEngine 基盤実装 — 🔴High / `security.rules` / 依存: T-90
- [x] **T-92** command/args 危険判定ルール — 🔴High / `security.rules.command` / 依存: T-91
- [x] **T-93** env secret 検出ルール — 🔴High / `security.rules.env` / 依存: T-91
- [x] **T-94** HTTP endpoint 診断ルール — 🟡Medium / `security.rules.endpoint` / 依存: T-91,T-80
- [x] **T-95** tool name/description 危険語検出 — 🔴High / `security.rules.tool` / 依存: T-91,T-40
- [x] **T-96** inputSchema 危険判定ルール — 🔴High / `security.rules.schema` / 依存: T-91,T-40
- [x] **T-97** resource exposure 診断ルール — 🟡Medium / `security.rules.resource` / 依存: T-60,T-91
- [x] **T-98** SecurityScanner 統合 — 🔴High / `security` / 依存: T-92〜T-97
- [x] **T-99** SecurityPanel UI 実装 — 🔴High / `ui.security` / 依存: T-98
- [x] **T-100** finding detail / recommendation 表示 — 🟡Medium / `ui.security` / 依存: T-99
- [x] **T-101** Security Score 算出式調整 — 🟡Medium / `security` / 依存: T-98
- [x] **T-102** Security rule 単体テスト — 🔴High / `tests.security` / 依存: T-92〜T-97
- [x] **T-103** 危険 tool サンプル profile 作成 — 🟢Low / `examples` / 依存: T-98

## Report Export (Phase 4)

- [x] **T-110** Report model 定義 — 🟡Medium / `report` / 依存: T-90,T-31
- [x] **T-111** MarkdownReportWriter 実装 — 🔴High / `report.markdown` / 依存: T-110
- [x] **T-112** HTMLReportWriter 実装 — 🟡Medium / `report.html` / 依存: T-111
- [x] **T-113** Security report export UI — 🔴High / `ui.report` / 依存: T-99,T-111
- [x] **T-114** Timeline summary report — 🟡Medium / `report.timeline` / 依存: T-31,T-111
- [x] **T-115** report snapshot test — 🟡Medium / `tests.report` / 依存: T-111

## mcp.json Editor / Profile Diff (Phase 5)

- [x] **T-120** mcp.json parser 実装 — 🔴High / `config.mcpjson` / 依存: T-21
- [x] **T-121** mcp.json import UI — 🔴High / `ui.profile` / 依存: T-120,T-51
- [x] **T-122** mcp.json export 実装 — 🔴High / `config.mcpjson` / 依存: T-120,T-55
- [x] **T-123** export UI — 🟡Medium / `ui.profile` / 依存: T-122
- [x] **T-124** mcp.json validation report — 🔴High / `config.validator` / 依存: T-120,T-54
- [x] **T-125** profile diff model — 🟡Medium / `config.diff` / 依存: T-55
- [x] **T-126** profile diff UI — 🟡Medium / `ui.profile` / 依存: T-125
- [x] **T-127** protocol version selector — 🟢Low / `ui.profile` / 依存: T-21
- [x] **T-128** mcp.json import/export テスト — 🔴High / `tests.config` / 依存: T-120,T-122
- [x] **T-129** profile diff テスト — 🟡Medium / `tests.config` / 依存: T-125

## Test Recorder / Replay (Phase 6)

- [x] **T-130** TestCase 型定義 — 🔴High / `recorder` / 依存: T-42
- [x] **T-131** tool call から TestCase 保存 — 🔴High / `recorder` / 依存: T-130,T-46
- [x] **T-132** TestCase SQLite 永続化 — 🟡Medium / `storage.sqlite` / 依存: T-55,T-130
- [x] **T-133** TestCase list UI — 🟡Medium / `ui.recorder` / 依存: T-132
- [x] **T-134** ReplayRunner 実装 — 🔴High / `recorder.replay` / 依存: T-42,T-130
- [x] **T-135** contains / equals assertion — 🔴High / `recorder.assertion` / 依存: T-134
- [x] **T-136** jsonpath assertion — 🟡Medium / `recorder.assertion` / 依存: T-135
- [x] **T-137** replay result UI — 🟡Medium / `ui.recorder` / 依存: T-134
- [x] **T-138** TestCase JSON import/export — 🟡Medium / `recorder.io` / 依存: T-130
- [x] **T-139** CLI replay command — 🟢Low / `cli` / 依存: T-134,T-138

## Server Diff / Compare (Phase 7)

- [x] **T-140** capability diff 実装 — 🟡Medium / `compare` / 依存: T-17
- [x] **T-141** tools diff 実装 — 🔴High / `compare` / 依存: T-40,T-41
- [x] **T-142** inputSchema diff 実装 — 🔴High / `compare` / 依存: T-141
- [x] **T-143** resources diff 実装 — 🟡Medium / `compare` / 依存: T-60,T-61
- [x] **T-144** prompts diff 実装 — 🟡Medium / `compare` / 依存: T-65,T-66
- [x] **T-145** Compare UI — 🟡Medium / `ui.compare` / 依存: T-140〜T-144
- [x] **T-146** breaking change 判定 — 🟡Medium / `compare` / 依存: T-142
- [x] **T-147** diff report export — 🟢Low / `report.diff` / 依存: T-111,T-145

## 配布 / OSS 運用 (Phase 8)

- [x] **T-150** CPack による Windows installer 生成 — 🟡Medium / `infra.release` / 依存: T-01
- [x] **T-151** macOS dmg 生成 workflow — 🟡Medium / `infra.release` / 依存: T-01
- [x] **T-152** Linux AppImage/deb 生成 workflow — 🟡Medium / `infra.release` / 依存: T-01
- [x] **T-153** GitHub Releases 自動アップロード — 🔴High / `infra.release` / 依存: T-150〜T-152
- [x] **T-154** README スクリーンショット追加 — 🟡Medium / `docs` / 依存: T-46,T-99
- [x] **T-155** サンプル MCP server 接続手順追加 — 🔴High / `docs` / 依存: T-49
- [x] **T-156** Issue template / PR template 整備 — 🟡Medium / `repo` / 依存: T-00
- [x] **T-157** v0.1.0 release checklist 作成 — 🟡Medium / `docs.release` / 依存: T-153,T-155
- [x] **T-158** 日本語/英語 README 併記方針整理 — 🟢Low / `docs` / 依存: T-09
- [x] **T-159** Apache-2.0 ライセンス確定・依存ライセンス確認 — 🔴High / `repo.license` / 依存: T-00

---

## MVP 最短実装順

最初に動くものを作る場合は、以下の順で進める。

1. T-00〜T-05: リポジトリ・CMake・CI
2. T-10〜T-16: JSON-RPC / initialize
3. T-20〜T-29: stdio transport
4. T-30〜T-36: Timeline
5. T-40〜T-47: tools/list / tools/call UI
6. T-50〜T-52: profile / connect UI
7. T-49, T-155: デモと README

---

## v0.1.0 完了条件

- [ ] Windows でアプリが起動する
- [ ] stdio MCP server に接続できる
- [ ] initialize が成功する
- [ ] tools/list が表示される
- [ ] tools/call が実行できる
- [ ] Timeline に raw JSON-RPC が表示される
- [ ] stderr log が表示される
- [ ] GitHub Actions で build/test が通る
- [ ] README に使い方とスクリーンショットがある
