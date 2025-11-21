### Emscripten Raylib Example (WebAssembly)

This project also includes a basic Raylib example that can be compiled to WebAssembly using Emscripten.

#### Prerequisites for WebAssembly Build

-   **Emscripten SDK**: Follow the official [Emscripten SDK documentation](https://emscripten.org/docs/getting_started/downloads.html) to install and activate the SDK. Ensure `EMSDK` environment variable is set and configured.
-   **Raylib Precompiled Library for Emscripten**: You will need a `libraylib.a` compiled for Emscripten. This library is typically located within your `emsdk` installation, e.g., `$(EMSDK)/upstream/emscripten/cache/sysroot/lib/libraylib.a`.

#### Building and Running the Example

1.  Navigate to the Raylib test directory:
    ```bash
    cd test/raylib
    ```
2.  Build the example using the provided Makefile:
    ```bash
    make
    ```
    This command will download `shell.html` (if missing) and compile `emscripten.raylib.c` into `emscripten.raylib.html`, `emscripten.raylib.js`, and `emscripten.raylib.wasm`.
3.  Run the compiled example using `emrun`:
    ```bash
    emrun --no_browser --port 8081 .
    ```
    Then, open your web browser and navigate to `http://localhost:8081/emscripten.raylib.html` to see the example running.
