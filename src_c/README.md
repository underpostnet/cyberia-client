# Cyberia Client - Build Instructions

## Quick Start

```bash
cd src_c
make
make serve
```

Open browser at: http://localhost:8081/index.html

---

## Build Commands

### Default Build (Debug)
```bash
make
```
Output: `bin/PLATFORM_WEB/DEBUG/`

### Debug Build
```bash
make debug
```
Output: `bin/PLATFORM_WEB/DEBUG/`

### Release Build
```bash
make release
```
Output: `bin/PLATFORM_WEB/RELEASE/`

### Clean
```bash
make clean
```

### Info
```bash
make info
```

---

## Running the Application

### Option 1: Use make serve (recommended)
```bash
make serve
```

### Option 2: Manual server
```bash
make
emrun --no_browser --port 8081 bin/PLATFORM_WEB/DEBUG
```

Then open: http://localhost:8081/cyberia-client.html

---

## Build Modes

- **DEBUG** (default): Includes debugging symbols, assertions, profiling
- **RELEASE**: Optimized for production with minification

Override build mode:
```bash
make BUILD_MODE=RELEASE
```

---

## Output Structure

```
bin/
└── PLATFORM_WEB/
    ├── DEBUG/
    │   ├── index.html
    │   ├── index.js
    │   └── index.wasm
    └── RELEASE/
        ├── index.html
        ├── index.js
        └── index.wasm
```

---

## Development Setup

### For Local Development (Recommended)

To avoid CORS issues, use a local API server:

1. **Edit `config.c`** - Uncomment localhost URLs:
   ```c
   const char* API_BASE_URL = "http://localhost:8080/api/v1";
   const char* ASSETS_BASE_URL = "http://localhost:8080/assets";
   ```

2. **Rebuild:**
   ```bash
   make clean && make
   ```

3. **Start local server** (in separate terminal):
   ```bash
   # Start your Cyberia API server on port 8080
   ```

4. **Serve the client:**
   ```bash
   make serve
   ```
