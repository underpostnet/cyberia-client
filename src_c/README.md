### Standard Build

```bash
cd src_c
make
```

### Debug Build (with assertions and debugging symbols)

```bash
make debug
```

### Release Build (optimized for production)

```bash
make release
```

### Clean Build Artifacts

```bash
make clean
```

### View Project Information

```bash
make info
```

### Running

1. **Build the project**:
   ```bash
   make
   ```

2. **Start a local web server**:
   ```bash
   emrun --no_browser --port 8081 .
   ```
    Or use the provided Makefile target:
   ```bash
   make serve
   ```
3. **Open in your browser**:
   ```
   http://localhost:8081/cyberia-client.html
   ```
