import traceback
from dataclasses import asdict
from queue import Empty, Queue
from typing import Any, Dict, List, Optional

from config import ASSETS_BASE_URL
from src.direction_converter import DirectionConverter
from src.object_layer.object_layer import (
    ObjectLayer,
    ObjectLayerState,
    Stats,
)
from src.serial import from_dict_generic
from src.services.object_layers import ObjectLayersService
from src.texture_manager import TextureManager


class ObjectLayersManager:
    """
    In-memory cache and adapter for ObjectLayer data.
    - get_or_fetch(item_id): returns ObjectLayer, fetching from REST if not cached.
    - build_hud_items(item_ids): returns lightweight dicts for HUD with icon/name/stats/desc/activable.
    """

    def __init__(
        self,
        texture_manager: TextureManager,
        direction_converter: DirectionConverter,
        service: Optional[ObjectLayersService] = None,
    ):
        self.service = service or ObjectLayersService()
        self.cache: Dict[str, ObjectLayer] = {}
        self.hud: List[Dict[str, Any]] = []
        self.texture_manager = texture_manager
        self.direction_converter = direction_converter
        self.texture_caching_queue: Queue[ObjectLayer] = Queue()

    def get_or_fetch(self, item_id: str) -> Optional[ObjectLayer]:
        print(f"[DEBUG OBJ_LAYER] get_or_fetch called for item_id: {item_id}")

        if not item_id:
            print(f"[DEBUG OBJ_LAYER] ⚠️ WARNING: item_id is None or empty")
            return None

        if item_id in self.cache:
            print(f"[DEBUG OBJ_LAYER] ✅ Cache hit for item_id: {item_id}")
            return self.cache[item_id]

        print(
            f"[DEBUG OBJ_LAYER] Cache miss for item_id: {item_id}, fetching from service..."
        )

        try:
            data = self.service.get_object_layer_by_item_id(item_id)
            print(f"[DEBUG OBJ_LAYER] Service returned data type: {type(data)}")

            if not data:
                print(
                    f"[DEBUG OBJ_LAYER] ⚠️ WARNING: Service returned None/empty for item_id: {item_id}"
                )
                return None

            print(
                f"[DEBUG OBJ_LAYER] Converting data to ObjectLayer for item_id: {item_id}"
            )
            ol = from_dict_generic(data, ObjectLayer)
            print(f"[DEBUG OBJ_LAYER] Conversion result type: {type(ol)}")

            if not ol:
                print(
                    f"[DEBUG OBJ_LAYER] ⚠️ WARNING: from_dict_generic returned None for item_id: {item_id}"
                )
                return None

        except Exception as e:
            print(
                f"[DEBUG OBJ_LAYER] ❌ ERROR fetching/converting item_id {item_id}: {type(e).__name__}: {e}"
            )
            import traceback

            traceback.print_exc()
            return None

        if ol:
            self.cache[item_id] = ol
            # Print object layer info with frame counts
            frames = ol.data.render.frames
            print(f"Fetched object layer for {item_id}: {ol.data.item.type}")
            print("Idle frame counts:")
            print(f"  Up: {len(frames.up_idle)}")
            print(f"  Down: {len(frames.down_idle)}")
            print(f"  Right: {len(frames.right_idle)}")
            print(f"  Left: {len(frames.left_idle)}")
            print(f"  Up-Right: {len(frames.up_right_idle)}")
            print(f"  Down-Right: {len(frames.down_right_idle)}")
            print(f"  Up-Left: {len(frames.up_left_idle)}")
            print(f"  Down-Left: {len(frames.down_left_idle)}")
            print(f"  Default: {len(frames.default_idle)}")
            print(f"  None: {len(frames.none_idle)}")
            print("Walking frame counts:")
            print(f"  Up: {len(frames.up_walking)}")
            print(f"  Down: {len(frames.down_walking)}")
            print(f"  Right: {len(frames.right_walking)}")
            print(f"  Left: {len(frames.left_walking)}")
            print(f"  Up-Right: {len(frames.up_right_walking)}")
            print(f"  Down-Right: {len(frames.down_right_walking)}")
            print(f"  Up-Left: {len(frames.up_left_walking)}")
            print(f"  Down-Left: {len(frames.down_left_walking)}")

            print(f"[DEBUG OBJ_LAYER] Adding {item_id} to texture caching queue")
            self.texture_caching_queue.put(ol)

        print(
            f"[DEBUG OBJ_LAYER] ✅ Successfully fetched and cached item_id: {item_id}"
        )
        return ol

    def build_hud_items(
        self, object_layers_state: List[ObjectLayerState]
    ) -> List[Dict[str, Any]]:
        hud_items: List[Dict[str, Any]] = []

        for ol_state in object_layers_state or []:
            if not ol_state.itemId:
                continue

            ol = self.get_or_fetch(ol_state.itemId)
            if not ol:
                # fallback minimal placeholder to keep HUD stable
                hud_items.append(
                    {
                        "id": ol_state.itemId,
                        "name": ol_state.itemId,
                        "icon": ol_state.itemId[:1].upper() if ol_state.itemId else "?",
                        "stats": Stats(),
                        "desc": "",
                        "isActivable": False,
                        "isActive": ol_state.active,
                        "type": "unknown",
                        "quantity": ol_state.quantity,
                    }
                )
                continue

            # map to HUD-friendly dict while keeping Stats dataclass
            hud_items.append(
                {
                    "id": ol.data.item.id or "unknown",
                    "name": ol.data.item.id,
                    "icon": ol.data.item.id[:1].upper(),
                    "stats": ol.data.stats or Stats(),
                    "desc": ol.data.item.description or "unknown",
                    "isActivable": bool(ol.data.item.activable),
                    "isActive": ol_state.active,
                    "type": ol.data.item.type or "unknown",
                    "quantity": ol_state.quantity,
                }
            )
        self.hud = hud_items
        return hud_items

    def process_texture_caching_queue(self):
        """
        Processes layers in the queue to pre-cache their textures.
        This should be called from the main thread (with the graphics context).
        """
        print(
            f"[DEBUG] process_texture_caching_queue() called. Queue empty: {self.texture_caching_queue.empty()}"
        )

        processed_count = 0
        while not self.texture_caching_queue.empty():
            print(f"[DEBUG] Processing texture cache item #{processed_count + 1}...")
            try:
                print(
                    f"[DEBUG] Getting item from queue (current size: {self.texture_caching_queue.qsize()})..."
                )
                ol = self.texture_caching_queue.get_nowait()
                print(
                    f"[DEBUG] Got ObjectLayer for item: {ol.data.item.id if ol and ol.data and ol.data.item else 'UNKNOWN'}"
                )

                print(f"[DEBUG] Calling _cache_all_textures_for_layer()...")
                self._cache_all_textures_for_layer(ol)
                print(f"[DEBUG] Finished _cache_all_textures_for_layer() successfully")

                processed_count += 1
            except Empty:
                # 🟢 Expected: The queue is empty, so we stop the loop.
                print(
                    "[DEBUG] Texture queue is empty (Empty Exception). Stopping processing."
                )
                break
            except Exception as e:
                # 🔴 Unexpected: Catch any other error that might happen

                # Print the simple error type and message
                print(f"❌ Unexpected Error during texture processing!")
                print(f"Error Type: {type(e).__name__}")
                print(f"Error Message: {e}")

                # Print the full stack trace (the "real" error)
                print("\n--- Full Traceback ---")
                traceback.print_exc()
                print("----------------------\n")

                # Decide if you want to stop or continue after an error
                # break # Uncomment if you want to stop processing immediately after an error
                pass  # Continue processing the next item in the queue (if any remain)
            finally:
                print(
                    f"[DEBUG] Finished processing texture caching task #{processed_count}."
                )

        print(
            f"[DEBUG] process_texture_caching_queue() completed. Total processed: {processed_count}"
        )

    def _cache_all_textures_for_layer(self, ol: ObjectLayer):
        """
        Iterates through all frames of an ObjectLayer and pre-caches their textures
        using the TextureManager.
        """
        print(f"[DEBUG] _cache_all_textures_for_layer() started")

        if not self.texture_manager:
            print("[ERROR] TextureManager not available in ObjectLayersManager.")
            return

        print(f"[DEBUG] Extracting item data...")
        item = ol.data.item
        if not item.id or not item.type:
            print(f"[ERROR] Item missing id or type. id={item.id}, type={item.type}")
            return

        print(f"[DEBUG] Converting frames to dict...")
        frames_dict = asdict(ol.data.render.frames)

        print(f"[DEBUG] Pre-caching textures for item: {item.id}, type: {item.type}")
        cached_count = 0
        direction_count = 0

        for direction_name, frames_list in frames_dict.items():
            if not frames_list:
                continue

            direction_count += 1
            print(
                f"[DEBUG] Processing direction #{direction_count}: {direction_name} ({len(frames_list)} frames)"
            )

            print(f"[DEBUG] Getting direction code for: {direction_name}")
            direction_code = self.direction_converter.get_code_from_directions(
                [direction_name]
            )

            if not direction_code:
                print(
                    f"[DEBUG] No direction code found for: {direction_name}, skipping"
                )
                continue

            print(f"[DEBUG] Direction code: {direction_code}")

            for i in range(len(frames_list)):
                frame_number = i
                uri = self._build_uri(
                    item_type=item.type,
                    item_id=item.id,
                    direction_code=direction_code,
                    frame=frame_number,
                )
                print(f"[DEBUG] Loading texture {cached_count + 1}: {uri}")
                self.texture_manager.load_texture_from_url(uri)
                cached_count += 1
                print(
                    f"[DEBUG] Texture loaded successfully ({cached_count} total so far)"
                )

        if cached_count > 0:
            print(
                f"[DEBUG] Finished pre-caching {cached_count} textures for {item.id}."
            )
        else:
            print(f"[DEBUG] No textures cached for {item.id}")

        print(f"[DEBUG] _cache_all_textures_for_layer() completed")

    def _build_uri(
        self,
        item_type: str = "skin",
        item_id: str = "anon",
        direction_code: str = "08",
        frame: int = 0,
        extension: str = "png",
    ) -> str:
        return f"{ASSETS_BASE_URL}/{item_type}/{item_id}/{direction_code}/{frame}.{extension}"
