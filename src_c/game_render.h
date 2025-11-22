#ifndef GAME_RENDER_H
#define GAME_RENDER_H

#include "game_state.h"
#include <raylib.h>
#include <stdbool.h>

/**
 * @file game_render.h
 * @brief Enhanced game rendering system
 * 
 * This module handles all game-specific rendering operations, building on
 * the basic render.c foundation. It includes entity rendering, world objects,
 * UI elements, effects, and all visual game elements.
 * Migrated from Python RenderCore and related rendering classes.
 */

/**
 * @brief Texture cache structure for optimized rendering
 */
typedef struct {
    Texture2D* textures;
    char** texture_names;
    int texture_count;
    int capacity;
} TextureCache;

/**
 * @brief Floating text effect structure
 */
typedef struct {
    Vector2 position;
    Vector2 velocity;
    char text[64];
    Color color;
    float life_time;
    float max_life_time;
    float font_size;
    bool active;
} FloatingText;

/**
 * @brief Click effect structure
 */
typedef struct {
    Vector2 position;
    float radius;
    float max_radius;
    float life_time;
    float max_life_time;
    Color color;
    bool active;
} ClickEffect;

/**
 * @brief Game renderer state
 */
typedef struct {
    // Texture management
    TextureCache texture_cache;
    
    // Screen dimensions
    int screen_width;
    int screen_height;
    
    // Effects
    FloatingText floating_texts[100];
    int floating_text_count;
    
    ClickEffect click_effects[20];
    int click_effect_count;
    
    // Font
    Font game_font;
    bool font_loaded;
    
    // Render flags
    bool show_debug_info;
    bool show_grid;
    bool show_aoi;
    bool show_paths;
    
    // Performance tracking
    int frames_rendered;
    double last_fps_update;
    float current_fps;
    
    // UI state
    bool hud_visible;
    Rectangle hud_area;
    
} GameRenderer;

// Global renderer instance
extern GameRenderer g_renderer;

/**
 * @brief Initialize the game renderer
 * @param screen_width Initial screen width
 * @param screen_height Initial screen height
 * @return 0 on success, -1 on failure
 */
int game_render_init(int screen_width, int screen_height);

/**
 * @brief Update screen dimensions (for window resize)
 * @param width New screen width
 * @param height New screen height
 */
void game_render_set_screen_size(int width, int height);

/**
 * @brief Main game rendering function (called each frame)
 * 
 * This is the main entry point for rendering the entire game.
 * Called from the render_update() function in render.c.
 */
void game_render_frame(void);

/**
 * @brief Cleanup game renderer resources
 */
void game_render_cleanup(void);

// ============================================================================
// World Rendering
// ============================================================================

/**
 * @brief Render the game world (grid, objects, entities)
 * 
 * This function handles all world-space rendering that should be
 * affected by camera transforms.
 */
void game_render_world(void);

/**
 * @brief Render grid background and lines
 */
void game_render_grid(void);

/**
 * @brief Render floor tiles
 */
void game_render_floors(void);

/**
 * @brief Render world objects (obstacles, portals, foregrounds)
 */
void game_render_world_objects(void);

/**
 * @brief Render all entities (player, other players, bots)
 */
void game_render_entities(void);

/**
 * @brief Render a single entity (player or bot)
 * @param entity Pointer to EntityState
 * @param color Entity color
 */
void game_render_entity(const EntityState* entity, Color color);

/**
 * @brief Render player's movement path
 */
void game_render_player_path(void);

/**
 * @brief Render Area of Interest (AOI) circle
 */
void game_render_aoi_circle(void);

/**
 * @brief Render debug information overlay
 */
void game_render_debug_overlay(void);

// ============================================================================
// Effects Rendering
// ============================================================================

/**
 * @brief Render all active click effects
 */
void game_render_click_effects(void);

/**
 * @brief Render all active floating text
 */
void game_render_floating_texts(void);

/**
 * @brief Add a new click effect at world position
 * @param world_pos Position in world coordinates
 * @param color Effect color
 */
void game_render_add_click_effect(Vector2 world_pos, Color color);

/**
 * @brief Add floating text at world position
 * @param world_pos Position in world coordinates
 * @param text Text to display
 * @param color Text color
 * @param font_size Font size
 * @param life_time How long the text should be visible
 */
void game_render_add_floating_text(Vector2 world_pos, const char* text, 
                                   Color color, float font_size, float life_time);

/**
 * @brief Update all active effects (called each frame)
 * @param delta_time Time elapsed since last update
 */
void game_render_update_effects(float delta_time);

// ============================================================================
// UI Rendering
// ============================================================================

/**
 * @brief Render the user interface (HUD, menus, etc.)
 * 
 * This function handles all screen-space UI rendering that should
 * NOT be affected by camera transforms.
 */
void game_render_ui(void);

/**
 * @brief Render main HUD (health, minimap, etc.)
 */
void game_render_hud(void);

/**
 * @brief Render player location info (map ID and coordinates)
 */
void game_render_location_info(void);

/**
 * @brief Render developer UI (if enabled)
 */
void game_render_dev_ui(void);

/**
 * @brief Render connection status indicator
 */
void game_render_connection_status(void);

/**
 * @brief Render performance information (FPS, etc.)
 */
void game_render_performance_info(void);

/**
 * @brief Render error messages
 */
void game_render_error_messages(void);

// ============================================================================
// Texture Management
// ============================================================================

/**
 * @brief Load a texture and add to cache
 * @param name Texture name/identifier
 * @param filename Path to texture file
 * @return 0 on success, -1 on failure
 */
int game_render_load_texture(const char* name, const char* filename);

/**
 * @brief Get texture from cache by name
 * @param name Texture name
 * @return Pointer to texture, or NULL if not found
 */
Texture2D* game_render_get_texture(const char* name);

/**
 * @brief Unload texture from cache
 * @param name Texture name
 */
void game_render_unload_texture(const char* name);

/**
 * @brief Clear all textures from cache
 */
void game_render_clear_texture_cache(void);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Convert world coordinates to screen coordinates
 * @param world_pos Position in world space
 * @return Position in screen space
 */
Vector2 game_render_world_to_screen(Vector2 world_pos);

/**
 * @brief Convert screen coordinates to world coordinates
 * @param screen_pos Position in screen space
 * @return Position in world space
 */
Vector2 game_render_screen_to_world(Vector2 screen_pos);

/**
 * @brief Check if a world position is visible on screen
 * @param world_pos Position in world space
 * @param margin Extra margin around screen edges
 * @return true if visible, false otherwise
 */
bool game_render_is_visible(Vector2 world_pos, float margin);

/**
 * @brief Get current camera view rectangle in world coordinates
 * @return Rectangle representing camera view bounds
 */
Rectangle game_render_get_camera_bounds(void);

/**
 * @brief Draw text with shadow effect
 * @param text Text to draw
 * @param x X position
 * @param y Y position
 * @param font_size Font size
 * @param color Main text color
 * @param shadow_color Shadow color
 * @param shadow_offset Shadow offset
 */
void game_render_text_with_shadow(const char* text, int x, int y, int font_size,
                                  Color color, Color shadow_color, Vector2 shadow_offset);

/**
 * @brief Draw a rectangle with outline
 * @param rect Rectangle to draw
 * @param fill_color Fill color
 * @param outline_color Outline color
 * @param outline_width Outline thickness
 */
void game_render_rectangle_outlined(Rectangle rect, Color fill_color, 
                                    Color outline_color, float outline_width);

/**
 * @brief Calculate entity sort order for depth rendering
 * @param entity Entity to calculate sort order for
 * @return Sort order value (higher = rendered later/on top)
 */
float game_render_calculate_entity_depth(const EntityState* entity);

/**
 * @brief Get interpolated position for smooth animation
 * @param start_pos Starting position
 * @param end_pos Ending position
 * @param factor Interpolation factor (0.0 to 1.0)
 * @return Interpolated position
 */
Vector2 game_render_interpolate_position(Vector2 start_pos, Vector2 end_pos, float factor);

/**
 * @brief Get entity animation frame based on direction and mode
 * @param entity Entity to get frame for
 * @param time Current time for animation timing
 * @return Animation frame index
 */
int game_render_get_entity_frame(const EntityState* entity, double time);

/**
 * @brief Set render flags for debugging/development
 * @param show_debug Show debug overlays
 * @param show_grid Show grid lines
 * @param show_aoi Show AOI circle
 * @param show_paths Show movement paths
 */
void game_render_set_debug_flags(bool show_debug, bool show_grid, 
                                 bool show_aoi, bool show_paths);

#endif // GAME_RENDER_H