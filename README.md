<p align="center">
  <img src="https://www.cyberiaonline.com/assets/splash/apple-touch-icon-precomposed.png" alt="CYBERIA online"/>
</p>

<div align="center">

<h1>cyberia client</h1>

</div>

A modular client for Cyberia, built with C, Raylib, and Emscripten. This project supports building for Desktop (Linux, Windows) and Web (WebAssembly) using a unified build system.

## Prerequisites

### General
*   **GNU Make**: Required for the build system.
*   **C Compiler**: GCC, Clang, or MinGW (for Windows).

### Desktop (Linux/Windows)
*   **Raylib**: You need the Raylib library installed or available.
    *   **Linux (RHEL/Fedora)**: `sudo dnf install raylib-devel`
    *   **Linux (Debian/Ubuntu)**: `sudo apt install libraylib-dev`
    *   **Windows**: Ensure Raylib headers and libraries are accessible to the compiler (MinGW recommended).

### Web (WebAssembly)
*   **Emscripten SDK (emsdk)**: Required to compile C to WebAssembly.
    *   [Installation Guide](https://emscripten.org/docs/getting_started/downloads.html)
    *   Ensure `emsdk_env` is activated in your terminal.

## Building

The project uses `Web.mk` as the primary makefile for all platforms.

### Web Build (WASM) - Default

To build for the web:

1.  **Activate Emscripten**:
    *   Linux/macOS: `source /path/to/emsdk/emsdk_env.sh`
    *   Windows: `call path\to\emsdk\emsdk_env.bat`

2.  **Build**:
    ```bash
    # Debug build (Default)
    make -f Web.mk web

    # Release build (Optimized)
    make -f Web.mk web BUILD_MODE=RELEASE
    ```

The web artifacts (`.html`, `.js`, `.wasm`, `.data`) will be generated in:
*   `bin/platform_web/debug/`
*   `bin/platform_web/release/`

### Desktop Build (Linux & Windows)

To build the native desktop application:

```bash
# Debug build
make -f Web.mk desktop

# Release build
make -f Web.mk desktop BUILD_MODE=RELEASE
```

The executable will be generated in:
*   `bin/PLATFORM_DESKTOP/DEBUG/`
*   `bin/PLATFORM_DESKTOP/RELEASE/`

## Running the Application

### Desktop
Run the generated executable directly from the terminal or file explorer.

**Linux:**
```bash
./bin/PLATFORM_DESKTOP/DEBUG/cyberia-client
```

**Windows:**
```cmd
bin\PLATFORM_DESKTOP\DEBUG\cyberia-client.exe
```

### Web (Local Server)

WebAssembly applications require a local web server to run due to browser security restrictions (CORS).

#### Method 1: Using Make (Python)
The makefile includes helpers to serve the build directory for both development and production environments.

**Development Server:**
Serves the debug build on port `8080` by default.
```bash
make -f Web.mk serve_development
# Override port:
make -f Web.mk serve_development DEV_PORT=9000
```

**Production Server:**
Serves the release build on port `8000` by default. This is intended to run behind an Envoy reverse proxy in production.
```bash
make -f Web.mk serve_production
# Override port:
make -f Web.mk serve_production PROD_PORT=9000
```

#### Method 2: Python Manual
If you want to serve a specific build mode manually:

**Linux/macOS:**
```bash
cd bin/platform_web/release
python3 -m http.server 8080
# Open http://localhost:8080/index.html
```

**Windows:**
```cmd
cd bin\platform_web\release
python -m http.server 8080
:: Open http://localhost:8080/index.html
```

#### Method 3: Emrun
Emscripten provides a lightweight server that handles COOP/COEP headers automatically (useful for threading support):

```bash
emrun bin/platform_web/debug/index.html
```

## Project Structure

*   `src/`: Source code (C).
*   `libs/`: External libraries (Raylib, cJSON).
*   `src/js/`: JavaScript interop files for Web builds.
*   `src/public/`: Static assets copied to the output directory.
*   `Web.mk`: Main build configuration file.
*   `config.mk`: Shared configuration variables.
