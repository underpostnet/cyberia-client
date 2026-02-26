<p align="center">
  <img src="https://www.cyberiaonline.com/assets/splash/apple-touch-icon-precomposed.png" alt="CYBERIA online"/>
</p>

<div align="center">

<h1>cyberia client</h1>

</div>

Web client for [Cyberia](https://www.cyberiaonline.com), built with C, Raylib, and Emscripten (WebAssembly).

## Prerequisites

*   **GNU Make**
*   **[Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)**
*   **[git-subrepo](https://github.com/ingydotnet/git-subrepo)**

Libraries (Raylib, cJSON) are already checked in under `lib/` via git-subrepo.

## Build

Activate Emscripten, then:

```bash
# Debug (default)
make -f Web.mk web

# Release
make -f Web.mk web BUILD_MODE=RELEASE
```

Output goes to `bin/platform_web/debug/` or `bin/platform_web/release/`.

## Run

```bash
# Development server (port 8081)
make -f Web.mk serve_development

# Production server (port 8081, release build)
make -f Web.mk serve_production
```

Override the port with `DEV_PORT=` or `PROD_PORT=`.

## Update Dependencies

```bash
git subrepo pull lib/raylib
git subrepo pull lib/cJSON
```

## Project Structure

```
src/           C source code
src/js/        JavaScript interop (Emscripten)
src/public/    Static assets (favicon, splash)
lib/           External libraries (git-subrepo)
scripts/       Setup and deploy scripts
Web.mk         Build configuration
config.mk      Shared variables
```
