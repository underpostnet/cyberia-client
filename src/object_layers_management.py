from typing import Dict, Optional, Any, List
from dataclasses import asdict
from queue import Queue, Empty

from src.object_layer.object_layer import (
    ObjectLayerState,
    ObjectLayer,
    Stats,
)
from src.services.object_layers import ObjectLayersService
from src.texture_manager import TextureManager
from src.direction_converter import DirectionConverter
from config import ASSETS_BASE_URL
from src.serial import from_dict_generic


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
        if not item_id:
            return None
        if item_id in self.cache:
            return self.cache[item_id]
        data = self.service.get_object_layer_by_item_id(item_id)
        if not data:
            return None
        ol = from_dict_generic(data, ObjectLayer)
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

            self.texture_caching_queue.put(ol)

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
        while not self.texture_caching_queue.empty():
            try:
                ol = self.texture_caching_queue.get_nowait()
                self._cache_all_textures_for_layer(ol)
            except Empty:
                break

    def _cache_all_textures_for_layer(self, ol: ObjectLayer):
        """
        Iterates through all frames of an ObjectLayer and pre-caches their textures
        using the TextureManager.
        """
        if not self.texture_manager:
            print("Warning: TextureManager not available in ObjectLayersManager.")
            return

        item = ol.data.item
        if not item.id or not item.type:
            return

        frames_dict = asdict(ol.data.render.frames)

        print(f"Pre-caching textures for item: {item.id}")
        cached_count = 0

        for direction_name, frames_list in frames_dict.items():
            if not frames_list:
                continue

            direction_code = self.direction_converter.get_code_from_directions(
                [direction_name]
            )

            if not direction_code:
                continue

            for i in range(len(frames_list)):
                frame_number = i
                uri = self._build_uri(
                    item_type=item.type,
                    item_id=item.id,
                    direction_code=direction_code,
                    frame=frame_number,
                )
                self.texture_manager.load_texture_from_url(uri)
                cached_count += 1

        if cached_count > 0:
            print(f"Finished pre-caching {cached_count} textures for {item.id}.")

    def _build_uri(
        self,
        item_type: str = "skin",
        item_id: str = "anon",
        direction_code: str = "08",
        frame: int = 0,
        extension: str = "png",
    ) -> str:
        return f"{ASSETS_BASE_URL}/{item_type}/{item_id}/{direction_code}/{frame}.{extension}"
