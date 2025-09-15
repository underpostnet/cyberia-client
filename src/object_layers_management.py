from typing import Dict, Optional, Any, List
from dataclasses import asdict
from queue import Queue, Empty

from src.object_layer import (
    ObjectLayer,
    ObjectLayerData,
    Stats,
    Item,
    Render,
    RenderFrames,
)
from src.services.object_layers import ObjectLayersService
from src.texture_manager import TextureManager
from src.direction_converter import DirectionConverter
from config import ASSETS_BASE_URL


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
        ol = self._parse_object_layer(data)
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

    def build_hud_items(self, item_ids: List[str]) -> List[Dict[str, Any]]:
        hud_items: List[Dict[str, Any]] = []
        # Create a mapping of existing items by ID to preserve their state
        existing_items = {item["id"]: item for item in self.hud if "id" in item}

        for iid in item_ids or []:
            ol = self.get_or_fetch(iid)
            if not ol:
                # fallback minimal placeholder to keep HUD stable
                hud_items.append(
                    {
                        "id": iid,
                        "name": iid,
                        "icon": iid[:1].upper() if iid else "?",
                        "stats": Stats(),
                        "desc": "",
                        "isActivable": False,
                        "isActive": (
                            existing_items.get(iid, {}).get("isActive", False)
                            if iid in existing_items
                            else False
                        ),
                        "type": "unknown",
                    }
                )
                continue

            # Check if this item was previously active
            was_active = existing_items.get(ol.data.item.id or "unknown", {}).get(
                "isActive", False
            )

            # map to HUD-friendly dict while keeping Stats dataclass
            hud_items.append(
                {
                    "id": ol.data.item.id or "unknown",
                    "name": ol.data.item.id,
                    "icon": ol.data.item.id[:1].upper(),
                    "stats": ol.data.stats or Stats(),
                    "desc": ol.data.item.description or "unknown",
                    "isActivable": bool(ol.data.item.activable),
                    "isActive": was_active,
                    "type": ol.data.item.type or "unknown",
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

    def _parse_object_layer(self, data: Dict[str, Any]) -> Optional[ObjectLayer]:
        """
        Parses a raw dictionary of object layer data into a structured ObjectLayer object.
        This includes robust handling for all nested dataclasses (Stats, Render, Item)
        with default values for missing or malformed data to prevent crashes.
        """
        try:
            # Stats parsing with robust type casting and defaults
            stats_dict = data.get("stats") or {}
            stats = Stats(
                effect=int(stats_dict.get("effect", 0) or 0),
                resistance=int(stats_dict.get("resistance", 0) or 0),
                agility=int(stats_dict.get("agility", 0) or 0),
                range=int(stats_dict.get("range", 0) or 0),
                intelligence=int(stats_dict.get("intelligence", 0) or 0),
                utility=int(stats_dict.get("utility", 0) or 0),
            )

            # Item parsing with robust type casting and defaults
            item_src = data.get("item") or {}
            item = Item(
                id=str(item_src.get("id") or data.get("id") or ""),
                type=str(item_src.get("type") or data.get("type") or ""),
                description=str(
                    item_src.get("description") or data.get("description") or ""
                ),
                activable=bool(item_src.get("activable", data.get("activable", False))),
            )

            # Render parsing including all frames, colors, and duration
            render = Render()
            render_data = data.get("render") or {}
            render.colors = render_data.get("colors", [])
            render.frame_duration = int(render_data.get("frame_duration", 0) or 0)
            render.is_stateless = bool(render_data.get("is_stateless", False))

            # RenderFrames parsing - iterate through all defined directions and modes
            frames_data = render_data.get("frames") or {}

            # Helper function to get frame list from dict
            def get_frames(key, default_val=[]):
                return frames_data.get(key, default_val)

            # Assign all idle frames
            render.frames.up_idle = get_frames("up_idle")
            render.frames.down_idle = get_frames("down_idle")
            render.frames.right_idle = get_frames("right_idle")
            render.frames.left_idle = get_frames("left_idle")
            render.frames.up_right_idle = get_frames("up_right_idle")
            render.frames.down_right_idle = get_frames("down_right_idle")
            render.frames.up_left_idle = get_frames("up_left_idle")
            render.frames.down_left_idle = get_frames("down_left_idle")
            render.frames.default_idle = get_frames("default_idle")
            render.frames.none_idle = get_frames("none_idle")

            # Assign all walking frames
            render.frames.up_walking = get_frames("up_walking")
            render.frames.down_walking = get_frames("down_walking")
            render.frames.right_walking = get_frames("right_walking")
            render.frames.left_walking = get_frames("left_walking")
            render.frames.up_right_walking = get_frames("up_right_walking")
            render.frames.down_right_walking = get_frames("down_right_walking")
            render.frames.up_left_walking = get_frames("up_left_walking")
            render.frames.down_left_walking = get_frames("down_left_walking")

            # Create ObjectLayer with the new structure
            return ObjectLayer(
                data=ObjectLayerData(stats=stats, render=render, item=item),
                sha256=str(data.get("sha256", "")),
            )
        except Exception as e:
            # Log the error for debugging purposes
            print(f"Error parsing object layer data: {e}")
            return None
