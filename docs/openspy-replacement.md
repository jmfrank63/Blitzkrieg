# OpenSpy Multiplayer Replacement

`Sources/sdk/openspy-core` is a submodule for the server-side replacement of the
dead GameSpy backend. It is not wired into the Visual Studio game build.

The current game still uses the original GameSpy client-facing code in:

- `Sources/src/Main/GameSpyPeerChat.*` for lobby and room chat.
- `Sources/src/Net/GSServersList.*` for server browsing.
- `Sources/src/Net/GSQueryReportingDriver.*` for host registration and query
  reporting.

The sane migration path is deliberately incremental:

1. Build and run OpenSpy services outside the game.
2. Redirect the game's GameSpy service hostnames/configuration to the local
   OpenSpy instance.
3. Validate the legacy client behavior against OpenSpy:
   - Peerchat lobby connection, room join, messages, player state.
   - Query/Reporting host registration.
   - Server Browsing list refresh and password/map metadata.
   - NAT negotiation or direct-connect fallback.
4. Replace the in-tree GameSpy SDK wrappers with an `OpenMultiplayer` adapter
   once the protocol behavior is understood.

OpenSpy services that matter first for Blitzkrieg are:

- Peerchat, for GameSpy lobby/chat behavior.
- QR, for hosted game reporting.
- Server Browsing, for visible internet game lists.
- NatNeg, if we want hosting without manual port forwarding.
- GP/Search, if account/buddy identity is required by the peer/chat layer.

`openspy-core` depends on Redis, RabbitMQ, and usually the
`openspy-web-backend`. We prefer native builds over Docker for this project, so
the first backend spike should document a direct Windows or Linux build/run
path and only use Docker documentation as protocol/runtime reference.
