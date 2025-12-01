@echo off
docker run -v %cd%:/src emscripten/emsdk emar %*