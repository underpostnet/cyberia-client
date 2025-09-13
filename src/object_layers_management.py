from typing import Dict, Optional, Any, List
from dataclasses import asdict

from src.object_layer import (
    ObjectLayer,
    ObjectLayerData,
    Stats,
    Item,
    Render,
    RenderFrames,
)
from src.services.object_layers import ObjectLayersService


class ObjectLayersManager:
    """
    In-memory cache and adapter for ObjectLayer data.
    - get_or_fetch(item_id): returns ObjectLayer, fetching from REST if not cached.
    - build_hud_items(item_ids): returns lightweight dicts for HUD with icon/name/stats/desc/activable.
    """

    def __init__(self, service: Optional[ObjectLayersService] = None):
        self.service = service or ObjectLayersService()
        self.cache: Dict[str, ObjectLayer] = {}
        self.hud: List[Dict[str, Any]] = []

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
            # print id and type
            print(f"Fetched object layer for {item_id}: {ol.data.item.type}")
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
                }
            )
        self.hud = hud_items
        return hud_items

    def _parse_object_layer(self, data: Dict[str, Any]) -> Optional[ObjectLayer]:
        try:
            # Stats can be under data["stats"] with keys matching our dataclass
            stats_dict = data.get("stats") or {}
            stats = Stats(
                effect=int(stats_dict.get("effect", 0) or 0),
                resistance=int(stats_dict.get("resistance", 0) or 0),
                agility=int(stats_dict.get("agility", 0) or 0),
                range=int(stats_dict.get("range", 0) or 0),
                intelligence=int(stats_dict.get("intelligence", 0) or 0),
                utility=int(stats_dict.get("utility", 0) or 0),
            )

            # Item may be under data["item"] or inline
            item_src = data.get("item") or {}
            item = Item(
                id=str(item_src.get("id") or data.get("id") or ""),
                type=str(item_src.get("type") or data.get("type") or ""),
                description=str(
                    item_src.get("description") or data.get("description") or ""
                ),
                activable=bool(item_src.get("activable", data.get("activable", False))),
            )

            # Render optional; keep defaults if not present
            render = Render()

            # Create ObjectLayer with the new structure
            return ObjectLayer(
                data=ObjectLayerData(stats=stats, render=render, item=item),
                sha256=str(data.get("sha256", "")),
            )
        except Exception:
            return None
