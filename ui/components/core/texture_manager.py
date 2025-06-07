import logging
from raylibpy import Texture2D, load_texture, unload_texture

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class TextureManager:
    """
    Manages loading, caching, and unloading of textures to prevent duplicate loading
    and ensure proper memory management.
    """

    _instance = None  # Singleton instance
    _textures: dict[str, Texture2D] = {}
    _is_initialized = False

    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def __init__(self):
        if not self._is_initialized:
            logging.info("TextureManager initialized.")
            TextureManager._is_initialized = True

    def load_texture(self, path: str) -> Texture2D | None:
        """
        Loads a texture from the given path. If the texture is already loaded,
        returns the cached instance.
        """
        if path in self._textures:
            return self._textures[path]

        try:
            texture = load_texture(path)
            self._textures[path] = texture
            logging.info(f"Loaded texture: {path}")
            return texture
        except Exception as e:
            logging.error(f"Failed to load texture from {path}: {e}")
            return None

    def get_texture(self, path: str) -> Texture2D | None:
        """
        Retrieves a loaded texture from the cache. Does not attempt to load it
        if it's not already in the cache.
        """
        return self._textures.get(path)

    def unload_texture(self, path: str):
        """
        Unloads a specific texture from memory and removes it from the cache.
        """
        if path in self._textures:
            texture = self._textures.pop(path)
            if texture:
                try:
                    unload_texture(texture)
                    logging.info(f"Unloaded texture: {path}")
                except Exception as e:
                    logging.error(f"Error unloading texture {path}: {e}")

    def unload_all_textures(self):
        """
        Unloads all textures currently managed by the manager.
        """
        logging.info("Unloading all textures...")
        for path in list(self._textures.keys()):
            self.unload_texture(path)
        logging.info("All textures unloaded.")
