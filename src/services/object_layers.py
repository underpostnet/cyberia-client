import requests
from typing import Optional, Dict, Any

from config import API_BASE_URL

# Lightweight service to fetch ObjectLayer data by item_id from REST API
# GET {API_BASE_URL}/object-layers?item_id=<id>&page=1&page_size=10


class ObjectLayersService:
    def __init__(self, base_url: Optional[str] = None, timeout: float = 3.0):
        self.base_url = base_url or API_BASE_URL
        self.timeout = timeout

    def get_object_layer_by_item_id(self, item_id: str) -> Optional[Dict[str, Any]]:
        try:
            url = f"{self.base_url}/object-layers"
            params = {"item_id": item_id, "page": 1, "page_size": 10}
            resp = requests.get(url, params=params, timeout=self.timeout)
            if resp.status_code != 200:
                return None
            data = resp.json()
            # Accept both list or paginated dict forms
            if isinstance(data, list):
                first = data[0] if data else None
                if isinstance(first, dict) and "doc" in first:
                    return first.get("doc") or None

            if isinstance(data, dict):
                # common pagination shape: {"items": [...]} or {"data": [...]}
                items = (
                    data.get("items")
                    if isinstance(data.get("items"), list)
                    else data.get("data")
                )
                if isinstance(items, list) and items:
                    first = items[0]
                    if isinstance(first, dict) and "doc" in first:
                        return first.get("doc") or None

            return None
        except Exception:
            # Network or parsing error -> None (manager will fallback)
            return None
