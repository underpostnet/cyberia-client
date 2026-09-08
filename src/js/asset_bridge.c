#include "asset_bridge.h"

#include <emscripten/emscripten.h>

EM_JS(int, asset_bridge_fetch, (const char* url, unsigned timeout_ms, size_t limit), {
    if (!Module.streamTransport) Module.streamTransport = {next: 0, entries: new Map()};
    const transport = Module.streamTransport;
    const handle = ++transport.next;
    const entry = {state: 0, bytes: null, size: 0, controller: new AbortController()};
    transport.entries.set(handle, entry);
    const signal = AbortSignal.any([entry.controller.signal, AbortSignal.timeout(timeout_ms || 15000)]);
    fetch(UTF8ToString(url), {signal}).then(async response => {
        if (!response.ok) throw new Error('Asset HTTP ' + response.status);
        const declared = Number(response.headers.get('content-length'));
        if (limit && declared > limit) throw new Error('Asset size limit');
        const buffer = await response.arrayBuffer();
        if (!buffer.byteLength || (limit && buffer.byteLength > limit)) throw new Error('Asset size limit');
        entry.bytes = new Uint8Array(buffer);
        entry.size = buffer.byteLength;
        entry.state = 1;
    }).catch(() => { entry.state = -1; });
    return handle;
})

EM_JS(int, asset_bridge_status, (int handle), {
    return Module.streamTransport.entries.get(handle)?.state ?? -1;
})

EM_JS(size_t, asset_bridge_size, (int handle), {
    return Module.streamTransport.entries.get(handle)?.size || 0;
})

EM_JS(void, asset_bridge_copy, (int handle, void* destination, size_t offset, size_t length), {
    HEAPU8.set(Module.streamTransport.entries.get(handle).bytes.subarray(offset, offset + length), destination);
})

EM_JS(void, asset_bridge_release, (int handle), {
    const entry = Module.streamTransport?.entries.get(handle);
    if (!entry) return;
    entry.controller.abort();
    Module.streamTransport.entries.delete(handle);
})

EM_JS(void, asset_bridge_decode_image, (int handle, int pixel_scale), {
    const transport = Module.streamTransport;
    const entry = transport.entries.get(handle);
    if (!entry || entry.state !== 1) return;
    entry.state = 2;
    try {
        if (!transport.worker) {
            const source = function() {
                self.onmessage = async function(event) {
                    const {handle, buffer, scale} = event.data;
                    let bitmap;
                    try {
                        bitmap = await createImageBitmap(new Blob([buffer], {type: 'image/png'}),
                                                         {premultiplyAlpha: 'none', colorSpaceConversion: 'none'});
                        if (bitmap.width > 4096 || bitmap.height > 4096) throw new Error('Texture size limit');
                        const width = Math.ceil(bitmap.width / scale);
                        const height = Math.ceil(bitmap.height / scale);
                        const canvas = new OffscreenCanvas(width, height);
                        const context = canvas.getContext('2d', {willReadFrequently: true});
                        context.imageSmoothingEnabled = false;
                        context.drawImage(bitmap, 0, 0, bitmap.width / scale, bitmap.height / scale);
                        const pixels = context.getImageData(0, 0, width, height).data.buffer;
                        self.postMessage({handle, width, height, pixels}, [pixels]);
                    } catch (error) {
                        self.postMessage({handle, failed: true});
                    } finally {
                        if (bitmap) bitmap.close();
                    }
                };
            };
            const workerUrl = URL.createObjectURL(new Blob(['(' + source.toString() + ')()'], {type: 'text/javascript'}));
            transport.worker = new Worker(workerUrl);
            URL.revokeObjectURL(workerUrl);
            transport.worker.onmessage = function(event) {
                const data = event.data;
                const target = transport.entries.get(data.handle);
                if (!target) return;
                target.state = data.failed ? -1 : 3;
                target.width = data.width;
                target.height = data.height;
                target.pixels = data.pixels ? new Uint8Array(data.pixels) : null;
            };
            transport.worker.onerror = function() {
                for (const target of transport.entries.values()) if (target.state === 2) target.state = -1;
                transport.worker.terminate();
                transport.worker = null;
            };
        }
        transport.worker.postMessage({handle, scale: pixel_scale, buffer: entry.bytes.buffer}, [entry.bytes.buffer]);
        entry.bytes = null;
    } catch (error) { entry.state = -1; }
})

EM_JS(int, asset_bridge_image_width, (int handle), {
    return Module.streamTransport.entries.get(handle)?.width || 0;
})

EM_JS(int, asset_bridge_image_height, (int handle), {
    return Module.streamTransport.entries.get(handle)?.height || 0;
})

// Call uploads after EndDrawing, with no raylib texture bound.
EM_JS(void, asset_bridge_upload, (int handle, unsigned texture, int row, int count), {
    const entry = Module.streamTransport.entries.get(handle);
    GLctx.bindTexture(GLctx.TEXTURE_2D, GL.textures[texture]);
    const start = row * entry.width * 4;
    GLctx.texSubImage2D(GLctx.TEXTURE_2D, 0, 0, row, entry.width, count,
                      GLctx.RGBA, GLctx.UNSIGNED_BYTE, entry.pixels.subarray(start, start + count * entry.width * 4));
    GLctx.bindTexture(GLctx.TEXTURE_2D, null);
})

double asset_bridge_now(void) { return emscripten_get_now(); }

EM_JS(unsigned, asset_bridge_diagnostics, (void), {
    const query = new URLSearchParams(location.search);
    const modes = {audio: 1, 'audio-network': 2, 'audio-runtime': 4, atlas: 8, dynamic: 16};
    let flags = 0;
    for (const mode of (query.get('stream-disable') || "").split(',')) flags |= modes[mode] || 0;
    Module.streamTrace = query.has('stream-profile') ? [] : null;
    Module.streamTraceCursor = 0;
    return flags;
})

EM_JS(void, asset_bridge_trace, (const char* event, const char* id, double bytes, double duration, int priority, const char* consumer), {
    if (!Module.streamTrace) return;
    Module.streamTrace[Module.streamTraceCursor++ % 30000] = {
        time: performance.now(), event: UTF8ToString(event), id: UTF8ToString(id),
        bytes, duration: priority < 0 ? duration : 0, residentBytes: priority < 0 ? 0 : duration,
        priority, consumer: UTF8ToString(consumer)
    };
})
