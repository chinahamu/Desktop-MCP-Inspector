# TransportConnection API

`TransportConnection` is the high-level connection boundary for UI and application code.

It wraps one concrete `Transport` implementation and exposes a small lifecycle API:

- `connect_to(profile)` starts the underlying transport with a `ServerProfile`.
- `disconnect()` stops an active transport.
- `reconnect()` restarts the last known profile.
- `send(message)` forwards JSON-RPC messages to the active transport.

The class also forwards transport events:

- `messageReceived`
- `stderrReceived`
- `errorOccurred`
- `stateChanged`

## Behavior

`TransportConnection` keeps the last profile after disconnect. This allows reconnect without requiring the UI to re-read or rebuild the profile.

When `connect_to()` is called while the underlying transport is `Starting`, `Running`, or `Stopping`, the current transport is stopped before the new profile is started.

`reconnect()` returns `false` until a profile has been provided by `connect_to()`.

## Intended use

UI code should prefer `TransportConnection` instead of calling concrete transports directly. Concrete transport implementations such as `StdioTransport` remain responsible for process-level details, while `TransportConnection` handles lifecycle orchestration.
