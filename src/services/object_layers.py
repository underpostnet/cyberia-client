import traceback
from typing import Any, Dict, Optional

import requests

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
        url = None
        params = None
        resp = None
        json_data = None

        try:
            url = f"{self.base_url}/object-layers"
            params = {"item_id": item_id, "page": 1, "page_size": 1}
            print(f"[DEBUG API] Fetching object layer for item_id: {item_id}")
            print(f"[DEBUG API] Request URL: {url}")
            print(f"[DEBUG API] Request params: {params}")
            print(f"[DEBUG API] Timeout: {self.timeout}s")

            resp = requests.get(url, params=params, timeout=self.timeout)
            print(f"[DEBUG API] Response status code: {resp.status_code}")
            print(f"[DEBUG API] Response headers: {dict(resp.headers)}")

            resp.raise_for_status()

            # Get JSON data
            json_data = resp.json()
            print(f"[DEBUG API] Response JSON type: {type(json_data)}")
            print(f"[DEBUG API] Response JSON: {json_data}")

            # Check if json_data is None or not a dict
            if json_data is None:
                print(
                    f"[DEBUG API] ❌ ERROR: resp.json() returned None for item_id: {item_id}"
                )
                return None

            if not isinstance(json_data, dict):
                print(
                    f"[DEBUG API] ❌ ERROR: resp.json() returned non-dict type {type(json_data)} for item_id: {item_id}"
                )
                return None

            # Get items array
            items = json_data.get("items")
            print(f"[DEBUG API] Items from response: {items}")

            if items is None:
                print(
                    f"[DEBUG API] ⚠️ WARNING: No 'items' key in response for item_id: {item_id}"
                )
                return None

            if not isinstance(items, list):
                print(
                    f"[DEBUG API] ❌ ERROR: 'items' is not a list, got {type(items)} for item_id: {item_id}"
                )
                return None

            if len(items) == 0:
                print(f"[DEBUG API] ⚠️ WARNING: Empty items list for item_id: {item_id}")
                return None

            result = items[0]
            print(
                f"[DEBUG API] ✅ Successfully fetched object layer for item_id: {item_id}"
            )
            return result

        except requests.Timeout as e:
            print(f"[DEBUG API] ❌ Timeout error for item_id {item_id}: {e}")
            print(f"[DEBUG API] URL was: {url}, params: {params}")
            return None
        except requests.HTTPError as e:
            print(f"[DEBUG API] ❌ HTTP error for item_id {item_id}: {e}")
            print(f"[DEBUG API] Status code: {resp.status_code if resp else 'N/A'}")
            print(f"[DEBUG API] Response text: {resp.text if resp else 'N/A'}")
            return None
        except requests.RequestException as e:
            print(
                f"[DEBUG API] ❌ Request exception for item_id {item_id}: {type(e).__name__}: {e}"
            )
            print(f"[DEBUG API] URL was: {url}, params: {params}")
            traceback.print_exc()
            return None
        except ValueError as e:
            print(f"[DEBUG API] ❌ JSON decode error for item_id {item_id}: {e}")
            print(f"[DEBUG API] Response text: {resp.text if resp else 'N/A'}")
            traceback.print_exc()
            return None
        except (AttributeError, KeyError, IndexError, TypeError) as e:
            print(
                f"[DEBUG API] ❌ Data structure error for item_id {item_id}: {type(e).__name__}: {e}"
            )
            print(f"[DEBUG API] json_data type: {type(json_data)}")
            print(f"[DEBUG API] json_data value: {json_data}")
            traceback.print_exc()
            return None
        except Exception as e:
            print(
                f"[DEBUG API] ❌ Unexpected error for item_id {item_id}: {type(e).__name__}: {e}"
            )
            print(f"[DEBUG API] URL: {url}, params: {params}")
            traceback.print_exc()
            return None
