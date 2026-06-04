<p align="center">
  <img src="https://www.cyberiaonline.com/assets/splash/apple-touch-icon-precomposed.png" alt="CYBERIA online"/>
</p>

<div align="center">

<h1>cyberia client</h1>

</div>

**`cyberia-client`** is the presentation runtime for the Cyberia MMO extension on [Underpost Platform](../src/client/public/cyberia-docs/UNDERPOST-PLATFORM.md). Written in C, compiled to WebAssembly via Emscripten, rendered with Raylib in the browser, and delivered as an installable Progressive Web App through the Underpost Platform static + Workbox pipeline.

It owns rendering, UI, input capture, prediction, reconciliation, interpolation, and client-side presentation defaults. It is **not** a world-simulation authority: that role belongs to [`cyberia-server`](../cyberia-server/README.md). Persistent content (atlases, object layers, asset metadata) is fetched directly from [`engine-cyberia`](../src/client/public/cyberia-docs/UNDERPOST-PLATFORM.md#engine-cyberia-nodejs--content-authority) over REST.

### Roles in the Cyberia stack

| Process | Role |
|---|---|
| `engine-cyberia` | content authority — persistence, maps, object layers, atlas metadata, optional client hints |
| `cyberia-server` | authoritative simulation — tick, AOI replication, input command processing |
| `cyberia-client` (this repo) | presentation runtime — render, UI, prediction, reconciliation, interpolation |

### Startup order — sequential

Startup is strictly sequential. The client is the last process to come up:

1. Persistent backend (databases, engine-cyberia, static asset backend).
2. cyberia-server (authoritative simulation).
3. cyberia-client (this repo) — connects to cyberia-server WebSocket; fetches optional presentation overrides from engine-cyberia REST.

Do not orchestrate these in parallel.

### Wire and REST surface

- **WebSocket binary** to `cyberia-server`: AOI snapshots (carry `tick` and `lastAcked` for reconciliation), typed input commands.
- **REST** to `engine-cyberia`: atlas sprite sheets, asset blobs, dialogue lines, UI icons, optional `/api/cyberia-client-hints/:code` for per-instance presentation overrides. The client runs correctly without ever calling the client-hints endpoint.

## Build

```bash
make -f Web.mk BUILD_MODE=RELEASE WS_URL=ws://server.cyberiaonline.com/ws API_BASE=https://www.cyberiaonline.com
```

Requires `cyberia-server` running on `:8081` (see [`cyberia-server/README.md`](../cyberia-server/README.md)).
