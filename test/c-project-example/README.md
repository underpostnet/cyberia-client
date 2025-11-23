### In Windows

1. Compile with EMSDK
> The script we pass as compiler is a hacky alias to use the emsdk docker image in windows
```pwsh
make -f Makefile CC=../../scripts/emcc-docker.bat
```

2. Start a local server
```pwsh
python -m http.server -d bin/PLATFORM_WEB/DEBUG
```

3. Then go to [localhost:3000/index.html`](http://localhost:3000/cyberia-client.html)