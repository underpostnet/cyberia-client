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
        """Fetch object layer data by item_id from the REST API.

        Args:
            item_id: The ID of the item to fetch object layer data for

        Returns:
            Optional[Dict[str, Any]]: The object layer data if found, None otherwise
        """
        try:
            url = f"{self.base_url}/object-layers"
            params = {"item_id": item_id, "page": 1, "page_size": 1}
            resp = requests.get(url, params=params, timeout=self.timeout)
            resp.raise_for_status()

            data = resp.json()

            # Handle both list and paginated dict responses
            if isinstance(data, list) and data:
                return data[0].get("data")

            if isinstance(data, dict):
                items = data.get("items", []) or data.get("data", [])
                if items and isinstance(items, list):
                    return items[0].get("data")

            return None

        except (requests.RequestException, ValueError, AttributeError):
            # Handle network, JSON decode, or structure errors
            return None
