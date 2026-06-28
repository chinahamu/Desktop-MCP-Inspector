# 01. アーキテクチャ詳細

> Desktop MCP Inspector の C++/Qt 実装における内部設計、責務分割、データモデル、UI 構成を定義する。

---

## 1. 設計方針

本プロジェクトでは、MCP プロトコル処理、transport、UI、永続化、セキュリティ診断を明確に分離する。

設計原則:

1. **Transport 非依存**: MCP の request/response 処理は stdio / HTTP の違いを知らない。
2. **Raw JSON を失わない**: parse 後の構造化データだけでなく、送受信した raw payload を Timeline に保存する。
3. **UI と protocol を疎結合にする**: UI 変更が MCP client の単体テストに影響しない構造にする。
4. **セキュリティ診断を後付け可能にする**: RuleEngine によるルール追加で危険判定を拡張できるようにする。
5. **OSS として読みやすい構成**: `src/mcp`, `src/transport`, `src/security`, `src/ui` のように責務をディレクトリで分ける。

---

## 2. ディレクトリ構成案

```text
Desktop-MCP-Inspector/
  CMakeLists.txt
  README.md
  LICENSE
  plan/
    00_overview.md
    01_architecture.md
    02_phase_plan.md
    07_task_list.md

  src/
    app/
      main.cpp
      MainWindow.cpp
      MainWindow.hpp
      AppController.cpp
      AppController.hpp
      AppState.cpp
      AppState.hpp

    mcp/
      McpClient.cpp
      McpClient.hpp
      McpSession.cpp
      McpSession.hpp
      McpMessage.cpp
      McpMessage.hpp
      McpTypes.hpp
      McpError.hpp
      McpCapabilities.hpp
      McpTool.hpp
      McpResource.hpp
      McpPrompt.hpp

    transport/
      Transport.hpp
      TransportEvent.hpp
      StdioTransport.cpp
      StdioTransport.hpp
      StreamableHttpTransport.cpp
      StreamableHttpTransport.hpp

    timeline/
      TimelineEvent.hpp
      TimelineStore.cpp
      TimelineStore.hpp
      TimelineModel.cpp
      TimelineModel.hpp

    config/
      ServerProfile.hpp
      ProfileStore.cpp
      ProfileStore.hpp
      McpJsonParser.cpp
      McpJsonParser.hpp
      ConfigValidator.cpp
      ConfigValidator.hpp

    security/
      SecurityScanner.cpp
      SecurityScanner.hpp
      RuleEngine.cpp
      RuleEngine.hpp
      RiskFinding.hpp
      RiskScore.hpp
      rules/
        ToolNameRules.cpp
        SchemaRules.cpp
        CommandRules.cpp
        EnvRules.cpp
        EndpointRules.cpp

    recorder/
      TestCase.hpp
      TestCaseRecorder.cpp
      TestCaseRecorder.hpp
      ReplayRunner.cpp
      ReplayRunner.hpp

    report/
      ReportExporter.cpp
      ReportExporter.hpp
      MarkdownReportWriter.cpp
      HtmlReportWriter.cpp

    ui/
      ServerPanel.cpp
      ToolPanel.cpp
      ResourcePanel.cpp
      PromptPanel.cpp
      TimelinePanel.cpp
      JsonViewer.cpp
      SecurityPanel.cpp
      ProfileEditor.cpp

    storage/
      SqliteConnection.cpp
      SqliteConnection.hpp
      SqliteTimelineStore.cpp
      SqliteTimelineStore.hpp
      SqliteProfileStore.cpp
      SqliteProfileStore.hpp

  tests/
    mcp/
    transport/
    config/
    security/
    recorder/

  examples/
    profiles/
    reports/
    test-cases/
```

---

## 3. レイヤ構成

```text
UI Layer
  ↓
Application Controller
  ↓
MCP Client / Profile Manager / Security Scanner
  ↓
Transport Interface
  ├─ StdioTransport
  └─ StreamableHttpTransport
  ↓
MCP Server
```

### 3.1 UI Layer

Qt の Model/View を基本にする。

- `ServerPanel`: 接続先 profile の一覧、接続/切断、起動コマンド表示
- `ToolPanel`: tools 一覧、schema、入力、実行結果
- `ResourcePanel`: resources 一覧、metadata、content preview
- `PromptPanel`: prompts 一覧、引数入力、message preview
- `TimelinePanel`: JSON-RPC event と stderr log の時系列表示
- `SecurityPanel`: Security Score、finding 一覧、詳細説明
- `ProfileEditor`: mcp.json import/export、server profile 編集

### 3.2 Application Controller

UI 操作と backend service を接続する層。

責務:

- profile 選択
- MCP server 接続/切断
- tools/resources/prompts 取得
- tool 実行リクエストの組み立て
- Timeline への event 反映
- SecurityScanner 実行
- ReportExporter 実行

### 3.3 MCP Client

MCP protocol の実行責務を持つ。

主要 API:

```cpp
class McpClient {
public:
    Task<InitializeResult> initialize(ClientInfo info);
    Task<std::vector<McpTool>> listTools();
    Task<ToolCallResult> callTool(std::string name, Json arguments);
    Task<std::vector<McpResource>> listResources();
    Task<ResourceReadResult> readResource(std::string uri);
    Task<std::vector<McpPrompt>> listPrompts();
    Task<PromptGetResult> getPrompt(std::string name, Json arguments);
    void cancel(RequestId id);
};
```

実装メモ:

- request id は monotonic counter で採番する
- request/response の対応関係を `pendingRequests` で管理する
- notification は id なしイベントとして Timeline に流す
- protocol version は initialize 結果を保持し、HTTP transport に渡せるようにする

---

## 4. Transport 設計

### 4.1 Transport Interface

```cpp
class Transport {
public:
    virtual ~Transport() = default;
    virtual void start(const ServerProfile& profile) = 0;
    virtual void stop() = 0;
    virtual void send(const Json& message) = 0;

    Signal<Json> messageReceived;
    Signal<std::string> stderrReceived;
    Signal<TransportError> errorOccurred;
    Signal<TransportState> stateChanged;
};
```

### 4.2 StdioTransport

MVP の最優先 transport。

責務:

- `QProcess` による MCP server 起動
- command / args / env / workingDirectory の設定
- stdout を newline-delimited JSON-RPC として parse
- stderr を log として Timeline に送る
- process exit、timeout、起動失敗を通知

注意点:

- stdout に JSON-RPC 以外の文字列が出た場合は protocol violation として表示する
- stderr は valid MCP message ではないため、MCP message と混同しない
- Windows のパス、quote、環境変数展開を慎重に扱う

### 4.3 StreamableHttpTransport

Phase 2 以降で実装する。

責務:

- HTTP POST で JSON-RPC message を送信
- `Accept: application/json, text/event-stream` を付与
- SSE response を逐次 parse
- `Mcp-Session-Id` を保存し、以後の request に付与
- `MCP-Protocol-Version` header を付与
- GET stream による server-to-client notification を受信
- 404 session expired 時に再 initialize する

---

## 5. データモデル

### 5.1 ServerProfile

```json
{
  "id": "local-filesystem",
  "name": "Local Filesystem Server",
  "transport": "stdio",
  "command": "npx",
  "args": ["-y", "@modelcontextprotocol/server-filesystem", "/tmp"],
  "workingDirectory": null,
  "env": {
    "NODE_ENV": "development"
  },
  "protocolVersion": "2025-06-18",
  "timeoutMs": 30000,
  "createdAt": "2026-06-27T00:00:00+09:00",
  "updatedAt": "2026-06-27T00:00:00+09:00"
}
```

### 5.2 TimelineEvent

```json
{
  "id": "evt_000001",
  "sessionId": "session_abc",
  "timestamp": "2026-06-27T00:00:00.000+09:00",
  "direction": "client_to_server",
  "kind": "jsonrpc_request",
  "jsonrpcId": 1,
  "method": "tools/call",
  "status": "pending",
  "durationMs": null,
  "payloadSize": 512,
  "raw": { "jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {} },
  "error": null
}
```

### 5.3 RiskFinding

```json
{
  "id": "finding_001",
  "severity": "high",
  "category": "tool.permission",
  "target": "tools.run_shell_command",
  "title": "Arbitrary command execution tool detected",
  "description": "The tool name and schema suggest arbitrary shell command execution.",
  "recommendation": "Require explicit confirmation and restrict accepted command patterns.",
  "evidence": {
    "toolName": "run_shell_command",
    "schema": { "command": "string" }
  }
}
```

---

## 6. SecurityScanner 設計

SecurityScanner は以下の入力を受け取り、finding と score を返す。

入力:

- ServerProfile
- InitializeResult / capabilities
- tools/list result
- resources/list result
- prompts/list result
- Timeline events

出力:

- `RiskScore`: A/B/C/D/F と数値 score
- `RiskFinding[]`: severity、category、evidence、recommendation
- Markdown report fragment

### 6.1 初期ルール

| ルールID | 対象 | 判定例 | Severity |
|----------|------|--------|----------|
| `CMD-001` | command | `npx` でバージョン固定なし | Medium |
| `CMD-002` | command | shell 経由起動、`cmd /c`, `sh -c` | High |
| `ENV-001` | env | `TOKEN`, `SECRET`, `KEY`, `PASSWORD` | Medium |
| `HTTP-001` | endpoint | localhost 以外 | Medium |
| `TOOL-001` | tool name | `shell`, `exec`, `command`, `run` | High |
| `TOOL-002` | tool name | `delete`, `remove`, `write`, `update` | Medium |
| `SCHEMA-001` | inputSchema | 任意 path string | Medium |
| `SCHEMA-002` | inputSchema | `additionalProperties: true` の広すぎる入力 | Low |
| `RESOURCE-001` | resource | file URI の広すぎる露出 | Medium |

---

## 7. UI レイアウト案

```text
+--------------------------------------------------------------------------------+
| Toolbar: Profile [v]  Connect  Disconnect  Scan  Export Report                 |
+----------------------+--------------------------+------------------------------+
| Servers / Profiles   | Inspector Tabs           | Detail                       |
| - local filesystem   | [Tools] [Resources]      | Tool schema / result         |
| - github mcp         | [Prompts] [Security]     | JSON viewer / form           |
| - healthcare mcp     |                          |                              |
+----------------------+--------------------------+------------------------------+
| Timeline: time | dir | method | id | status | duration | size | summary          |
+--------------------------------------------------------------------------------+
| Raw JSON / stderr / selected event detail                                      |
+--------------------------------------------------------------------------------+
```

MVP では Qt Widgets で構築する。将来的に QML 化する場合でも、backend model を再利用できるようにする。

---

## 8. テスト方針

| テスト種別 | 対象 | 目的 |
|------------|------|------|
| Unit | McpMessage parse/serialize | JSON-RPC 変換の正確性 |
| Unit | StdioTransport line parser | newline-delimited JSON の扱い |
| Unit | Security rules | finding 検出の再現性 |
| Unit | mcp.json parser | import/export の互換性 |
| Integration | mock MCP server | initialize/tools/list/tools/call の往復 |
| UI smoke | MainWindow 起動 | 最低限の起動確認 |
| Snapshot | ReportExporter | Markdown report の差分確認 |

---

## 9. 将来拡張

- MCP Apps / UI resource の inspection
- OAuth flow inspection
- tool description と実装コードの不整合検出
- GitHub Actions での replay runner
- `mcp-inspector-cli scan mcp.json` の提供
- 複数サーバーの capability diff
- ログの pcap 風 export/import
- 日本語/英語 UI 切替
