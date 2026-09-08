#ifndef CYBERIA_ASSET_BRIDGE_H
#define CYBERIA_ASSET_BRIDGE_H

#include <stddef.h>

int asset_bridge_fetch(const char* url, unsigned timeout_ms, size_t limit);
int asset_bridge_status(int handle);
size_t asset_bridge_size(int handle);
void asset_bridge_copy(int handle, void* destination, size_t offset, size_t length);
void asset_bridge_release(int handle);
void asset_bridge_decode_image(int handle, int pixel_scale);
double asset_bridge_now(void);
unsigned asset_bridge_diagnostics(void);
void asset_bridge_trace(const char* event, const char* id, double bytes, double duration, int priority, const char* consumer);
int asset_bridge_image_width(int handle);
int asset_bridge_image_height(int handle);
void asset_bridge_upload(int handle, unsigned texture, int row, int count);

#endif
