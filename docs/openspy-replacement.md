# OpenSpy Multiplayer Replacement

`Sources/sdk/openspy-core` is a submodule for the future server-side replacement
of the dead GameSpy backend. It is not wired into the Visual Studio game build.

The in-tree GameSpy SDK has been removed. The old game-facing wrapper class
names still exist as compatibility shells so existing UI commands and factories
continue to compile, but they no longer include or link GameSpy code:

- `Sources/src/Main/GameSpyPeerChat.*` delegates only to LAN in-game chat for now.
- `Sources/src/Main/GameSpyChat.*` delegates only to LAN in-game chat for now.
- `Sources/src/Net/GSServersList.*` is an empty internet server-list backend.
- `Sources/src/Net/GSQueryReportingDriver.*` is an empty host-reporting backend.

The next real multiplayer step is an OpenSpy client adapter behind those
game-facing interfaces. That work can be done later without bringing the
proprietary SDK back.

OpenSpy services that matter first for Blitzkrieg are:

- Peerchat, for internet lobby/chat behavior.
- QR, for hosted game reporting.
- Server Browsing, for visible internet game lists.
- NatNeg, if we want hosting without manual port forwarding.
- GP/Search, if account/buddy identity is required by the lobby/chat layer.

`openspy-core` depends on Redis, RabbitMQ, and usually the
`openspy-web-backend`. We prefer native builds over Docker for this project, so
the backend spike should document a direct Windows or Linux build/run path and
only use Docker documentation as protocol/runtime reference.
