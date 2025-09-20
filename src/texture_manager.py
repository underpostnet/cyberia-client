import argparse
import ctypes
import os
import sys
from typing import Dict, Optional

import pyray as pr
import requests

# Add project root to path to import config
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from config import ASSETS_BASE_URL


class TextureManager:
    """
    A manager for loading, caching, and drawing image textures using pyray.
    It handles loading from file paths and URLs, caching textures for performance,
    and unloading them to free VRAM.
    """

    def __init__(self):
        """Initializes the TextureManager with an empty cache."""
        self.texture_cache: Dict[str, pr.Texture] = {}
        print("TextureManager initialized.")

    def load_texture(self, path: str) -> Optional[pr.Texture]:
        """
        Loads a texture from a file path.
        If the texture is already cached, returns the cached version.
        Otherwise, loads it, caches it, and returns it.
        Returns None if loading fails.
        """
        if path in self.texture_cache:
            return self.texture_cache[path]

        try:
            texture = pr.load_texture(path)
            if texture.id > 0:  # A valid texture has a non-zero ID
                self.texture_cache[path] = texture
                print(f"Loaded and cached texture from: {path}")
                return texture
            else:
                print(f"Failed to load texture from: {path}")
                return None
        except Exception as e:
            print(f"Error loading texture from {path}: {e}")
            return None

    def load_texture_from_url(self, url: str) -> Optional[pr.Texture]:
        """
        Loads a texture from a URL.
        Requires the 'requests' library.
        Caches the texture using the URL as the key.
        Returns None if loading fails.
        """
        if url in self.texture_cache:
            return self.texture_cache[url]

        try:
            print(f"Downloading image from: {url}")
            response = requests.get(url, timeout=10)
            response.raise_for_status()  # Raise an exception for bad status codes

            image_data = response.content
            # The file extension is required for load_image_from_memory
            file_ext = "." + url.split("?")[0].split(".")[-1].lower()

            # In recent pyray versions, load_image_from_memory can accept a bytes object directly.
            # The library handles the conversion to a C-style pointer internally.
            image = pr.load_image_from_memory(file_ext, image_data, len(image_data))
            if image.data == 0:  # Check if image loading failed
                print(f"Failed to load image from memory for URL: {url}")
                return None

            texture = pr.load_texture_from_image(image)
            pr.unload_image(image)  # Unload CPU image data, it's now on the GPU

            if texture.id > 0:
                self.texture_cache[url] = texture
                print(f"Loaded and cached texture from URL: {url}")
                return texture
            else:
                print(f"Failed to create texture from image for URL: {url}")
                return None

        except requests.exceptions.RequestException as e:
            print(f"Failed to download image from {url}: {e}")
            return None
        except Exception as e:
            print(f"An error occurred while loading texture from URL {url}: {e}")
            return None

    def load_ui_icon(self, icon_name: str) -> Optional[pr.Texture]:
        """
        Loads a UI icon texture from the assets server's /ui-icons/ directory.
        Example: load_ui_icon("close-yellow.png") -> loads from ASSETS_BASE_URL/ui-icons/close-yellow.png
        """
        url = f"{ASSETS_BASE_URL}/ui-icons/{icon_name}"
        return self.load_texture_from_url(url)

    def get_texture(self, identifier: str) -> Optional[pr.Texture]:
        """
        Retrieves a texture from the cache.

        Args:
            identifier (str): The file path or URL of the texture.

        Returns:
            Optional[pr.Texture]: The cached texture, or None if not found.
        """
        return self.texture_cache.get(identifier)

    def unload_texture(self, identifier: str):
        """
        Unloads a specific texture from VRAM and removes it from the cache.
        """
        if identifier in self.texture_cache:
            pr.unload_texture(self.texture_cache[identifier])
            del self.texture_cache[identifier]
            print(f"Unloaded texture: {identifier}")

    def unload_all_textures(self):
        """
        Unloads all cached textures from VRAM and clears the cache.
        Should be called before closing the application.
        """
        print("Unloading all cached textures...")
        for texture in self.texture_cache.values():
            pr.unload_texture(texture)
        self.texture_cache.clear()
        print("All textures unloaded.")


def main_test(object_layer_uri: str):
    """
    A simple test function to demonstrate TextureManager usage.
    Loads an image from a URL and displays it in a window.
    """
    screen_width = 500
    screen_height = 500
    image_url = f"{ASSETS_BASE_URL}{object_layer_uri}"

    pr.init_window(screen_width, screen_height, "Texture Manager Test")
    pr.set_target_fps(60)

    texture_manager = TextureManager()

    # Load the texture from the URL.
    # Make sure you have a local server running that can serve this file.
    # For example, using `python -m http.server 8080` in the right directory.
    texture = texture_manager.load_texture_from_url(image_url)

    while not pr.window_should_close():
        pr.begin_drawing()
        pr.clear_background(pr.RAYWHITE)

        if texture:
            # Draw the texture centered on the screen
            x = screen_width // 2 - texture.width // 2
            y = screen_height // 2 - texture.height // 2
            pr.draw_texture(texture, x, y, pr.WHITE)
            pr.draw_text(
                object_layer_uri,
                10,
                10,
                20,
                pr.DARKGRAY,
            )
        else:
            pr.draw_text(
                f"Failed to load texture from:\n{image_url}", 10, 10, 20, pr.MAROON
            )
            pr.draw_text(
                "Is a local server running on port 8080?", 10, 60, 20, pr.DARKGRAY
            )

        pr.end_drawing()

    # Cleanup
    texture_manager.unload_all_textures()
    pr.close_window()


if __name__ == "__main__":
    # Note: This test requires the 'requests' library (pip install requests)
    # and a local web server serving the image at the specified URL.
    parser = argparse.ArgumentParser(description="Texture Manager Test")
    parser.add_argument(
        "--uri",
        type=str,
        default="/skin/anon/08/0.png",
        help="The object layer URI to load from the assets server.",
    )
    args = parser.parse_args()
    main_test(args.uri)
