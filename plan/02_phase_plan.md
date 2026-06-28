# 02. 実装フェーズ計画

> Desktop MCP Inspector の実装を段階的に進めるためのフェーズ計画。各フェーズは単独でレビュー・リリース可能な単位に分割する。

---

## Phase 0: リポジトリ初期化・開発基盤

### 目的

C++/Qt プロジェクトとして最低限のビルド、テスト、CI、ドキュメント構成を整える。

### 成果物

- `CMakeLists.txt`
- `src/app/main.cpp`
- Qt 6 の空 MainWindow
- `tests/` 構成
- GitHub Actions の build/test workflow
- `README.md`, `LICENSE`, `.gitignore`, `CONTRIBUTING.md`

### 完了条件

- Windows/Linux で CMake configure/build が成功する
- `ctest` が実行できる
- GitHub Actions が push 時に build/test を実行する
- README にプロジェクト目的と初期スクリーンショット枠がある

---

## Phase 1: MVP — stdio 接続と MCP tool 実行

### 目的

任意の stdio MCP サーバーを GUI から起動し、`initialize`、`tools/list`、`tools/call` を実行できる状態にする。

### 対象機能

1. ServerProfile の作成
2. StdioTransport
3. McpClient の基本 request/response 管理
4. initialize / initialized
5. tools/list
6. tool 詳細表示
7. JSON 入力による tools/call
8. tool 実行結果表示
9. raw JSON-RPC Timeline
10. stderr log 表示

### UI 範囲

- 接続設定パネル
- tools 一覧
- tool detail
- JSON 入力欄
- 実行結果 JSON viewer
- Timeline table
- raw event detail

### 完了条件

- `npx -y @modelcontextprotocol/server-filesystem <dir>` のような stdio server に接続できる
- tools 一覧を取得できる
- 任意 tool を JSON 入力で実行できる
- request/response/error/stderr が Timeline に表示される
- 不正 JSON 入力時に UI 上でエラーを表示する

---

## Phase 2: Inspector 基本機能拡張

### 目的

公式 Inspector の基本領域である resources/prompts/notifications と、schema-driven form を実装する。

### 対象機能

1. resources/list
2. resources/read
3. prompts/list
4. prompts/get
5. notification 受信表示
6. logging message 表示
7. progress notification 表示
8. inputSchema からのフォーム自動生成
9. request cancel
10. timeout 設定

### 完了条件

- resources の metadata と content preview が表示できる
- prompts の引数を入力して生成結果を確認できる
- tools/call の入力フォームが schema から生成される
- notification が Timeline と専用ペインに表示される
- 長時間 request を cancel できる

---

## Phase 3: Streamable HTTP 対応

### 目的

stdio だけでなく、MCP の Streamable HTTP transport に接続できるようにする。

### 対象機能

1. HTTP endpoint profile
2. initialize POST
3. JSON response handling
4. SSE response handling
5. GET stream handling
6. `Mcp-Session-Id` 管理
7. `MCP-Protocol-Version` header 管理
8. session expired handling
9. Origin/localhost/auth 設定チェック
10. HTTP error と JSON-RPC error の分離表示

### 完了条件

- Streamable HTTP のローカル MCP endpoint に接続できる
- POST response が application/json / text/event-stream の両方で処理できる
- session id を以後の request に付与できる
- HTTP error と protocol error が Timeline 上で区別される

---

## Phase 4: Security Workbench

### 目的

Desktop 版独自の価値として、MCP サーバーの危険性を診断し、Security Score と finding を出す。

### 対象機能

1. SecurityScanner 基盤
2. RuleEngine
3. command/args 診断
4. env 診断
5. HTTP endpoint 診断
6. tool name/description 診断
7. inputSchema 診断
8. resource exposure 診断
9. Security Score 表示
10. finding detail / recommendation 表示
11. Markdown 監査レポート出力

### 完了条件

- 任意の ServerProfile と tools/list 結果から Security Score を算出できる
- High/Medium/Low finding が UI 表示される
- finding に evidence と recommendation が含まれる
- Markdown report を保存できる

---

## Phase 5: Profile / mcp.json Editor

### 目的

MCP クライアント設定を GUI で管理できるようにし、初心者が mcp.json 設定で詰まる問題を軽減する。

### 対象機能

1. profile 永続化
2. mcp.json import
3. mcp.json export
4. command/args/env editor
5. env secret masking
6. working directory picker
7. 起動テスト
8. profile clone
9. profile diff
10. protocol version selection

### 完了条件

- Claude Desktop / Cursor / Claude Code 風の mcp.json を読み込める
- GUI で server profile を編集できる
- 編集した profile を mcp.json として export できる
- command 起動テストの結果が Timeline に表示される
- env secret が UI 上で不用意に露出しない

---

## Phase 6: Test Recorder / Replay

### 目的

GUI 上の tool 実行をテストケースとして保存し、後から再実行できるようにする。

### 対象機能

1. tools/call の test case 保存
2. request input / response assertion の保存
3. contains / equals / jsonpath assertion
4. replay runner
5. replay result UI
6. JSON test case import/export
7. CLI replay command
8. CI 用 JUnit/JSON 出力

### 完了条件

- GUI で実行した tool call を test case として保存できる
- 保存した test case を GUI から再実行できる
- assertion 成功/失敗が表示される
- CLI で test case file を実行できる

---

## Phase 7: Diff / Compare

### 目的

複数 MCP サーバー、または同一サーバーのバージョン差分を比較できるようにする。

### 対象機能

1. capabilities diff
2. tools diff
3. tool description diff
4. inputSchema diff
5. resources diff
6. prompts diff
7. protocol version diff
8. Markdown diff report

### 完了条件

- 2つの profile を選択して差分を表示できる
- tools の追加/削除/変更が分かる
- schema の破壊的変更を警告できる
- diff report を export できる

---

## Phase 8: 配布・OSS 運用

### 目的

OSS として使いやすい状態に整え、GitHub Releases で配布可能にする。

### 対象機能

1. Windows installer
2. macOS dmg
3. Linux AppImage または deb
4. GitHub Releases 自動生成
5. README スクリーンショット
6. サンプル MCP profile
7. Issue template
8. PR template
9. Security policy
10. Release checklist

### 完了条件

- GitHub Releases に OS 別 artifact が生成される
- 新規ユーザーが README だけでサンプル MCP server に接続できる
- CONTRIBUTING と SECURITY が整備される
- v0.1.0 の tag を切れる

---

## 推奨リリース計画

| バージョン | 対象フェーズ | リリース内容 |
|------------|--------------|--------------|
| v0.0.1 | Phase 0 | 空 UI + ビルド基盤 |
| v0.1.0 | Phase 1 | stdio MVP / tools 実行 |
| v0.2.0 | Phase 2 | resources/prompts/schema form |
| v0.3.0 | Phase 3 | Streamable HTTP |
| v0.4.0 | Phase 4 | Security Workbench |
| v0.5.0 | Phase 5 | mcp.json Editor |
| v0.6.0 | Phase 6 | Test Recorder / Replay |
| v0.7.0 | Phase 7 | MCP Server Diff |
| v1.0.0 | Phase 8 | 安定配布版 |

---

## 初期デモシナリオ

MVP の README では、以下のデモを最初に掲載する。

1. アプリを起動
2. stdio profile を作成
3. command に `npx`、args に filesystem server 起動引数を設定
4. Connect
5. tools/list が表示される
6. tool を選択
7. JSON input を入力
8. Execute
9. Timeline に initialize / tools/list / tools/call が並ぶ
10. Security Scan で basic finding を表示

---

## 非対象範囲（初期）

- LLM 本体のチャット UI
- MCP server hosting
- MCP server marketplace
- production proxy / gateway
- 認証情報のクラウド同期
- SaaS 化
- 企業向けポリシー管理サーバー

これらは OSS MVP 後の拡張候補とし、初期はローカル検査・開発・監査に集中する。
