/**
 * @file view.h
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @brief SDL-based visualization system for drone coordination display
 * @version 0.1
 * @date 2025-05-22
 * 
 * This header defines the complete visualization system for the emergency
 * drone coordination application. It provides real-time graphical display
 * of drone positions, survivor locations, mission assignments, and system
 * statistics using SDL2 graphics library.
 * 
 * **Key Features:**
 * - Real-time map visualization with color-coded entities
 * - Information panel with live statistics
 * - Mission path visualization
 * - Interactive window with event handling
 * - Scalable grid-based display system
 * 
 * **Color Coding:**
 * - Red: Survivors waiting for rescue
 * - Blue: Idle drones available for missions
 * - Green: Drones on active missions
 * - Green lines: Mission paths from drone to target
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup core_modules
 * @ingroup visualization
 */

#ifndef VIEW_H
#define VIEW_H

#include <SDL2/SDL.h>
#include <stdbool.h>

/**
 * @defgroup visualization Visualization System
 * @brief SDL-based real-time graphics and user interface
 * @ingroup core_modules
 * @{
 */

/**
 * @defgroup sdl_globals SDL Global Variables
 * @brief Core SDL objects for window and rendering
 * @{
 */

//clang-format off
/** @brief Main SDL window instance for the application */
extern SDL_Window *window;

/** @brief SDL renderer for all drawing operations */
extern SDL_Renderer *renderer;

/** @brief Current window width in pixels */
extern int window_width;

/** @brief Current window height in pixels */
extern int window_height;

/** @} */ // end of sdl_globals group

/**
 * @defgroup color_constants Display Colors
 * @brief Predefined color constants for consistent rendering
 * @{
 */

/** @brief Black color (0,0,0,255) - used for backgrounds and borders */
extern const SDL_Color BLACK;

/** @brief Red color (255,0,0,255) - used for survivors */
extern const SDL_Color RED;

/** @brief Blue color (0,0,255,255) - used for idle drones */
extern const SDL_Color BLUE;

/** @brief Green color (0,255,0,255) - used for active drones and mission paths */
extern const SDL_Color GREEN;

/** @brief White color (255,255,255,255) - used for text and grid lines */
extern const SDL_Color WHITE;

/** @brief Light gray color (200,200,200,255) - used for panel backgrounds */
extern const SDL_Color LIGHT_GRAY;

/** @brief Dark gray color (50,50,50,255) - used for text backgrounds */
extern const SDL_Color DARK_GRAY;

/** @} */ // end of color_constants group

/**
 * @defgroup initialization System Initialization
 * @brief Functions for setting up the visualization system
 * @{
 */

/**
 * @brief Initialize SDL window and renderer based on map dimensions
 * 
 * Creates the main application window with appropriate size for the map
 * plus space for the information panel. Initializes SDL subsystems,
 * creates the window and renderer, and loads fonts for text display.
 * 
 * **Window Layout:**
 * - Left side: Map visualization (map_width * CELL_SIZE pixels)
 * - Right side: Information panel (PANEL_WIDTH pixels)
 * - Height: map_height * CELL_SIZE pixels
 * 
 * **Initialization Steps:**
 * 1. Calculate window dimensions from map size
 * 2. Initialize SDL video subsystem
 * 3. Initialize SDL_ttf for font rendering
 * 4. Create window and renderer
 * 5. Load fonts with fallback options
 * 
 * @return 0 on success, 1 on failure
 * 
 * @pre Global map structure must be initialized with valid dimensions
 * @post SDL window and renderer are ready for drawing operations
 * 
 * @note Window title is set to "Drone Simulator"
 * @warning Will attempt multiple font paths for compatibility
 * 
 * @see quit_all() for cleanup
 */
extern int init_sdl_window();

/** @} */ // end of initialization group

/**
 * @defgroup drawing_primitives Drawing Primitives
 * @brief Basic drawing functions for map elements
 * @{
 */

/**
 * @brief Draw a colored cell at specified map coordinates
 * 
 * Fills a single grid cell with the specified color. Performs bounds
 * checking to prevent invalid memory access and maintains grid line
 * visibility by making cells slightly smaller than their allocated space.
 * 
 * **Coordinate System:**
 * - x: Map row (0 to map.height-1)
 * - y: Map column (0 to map.width-1)
 * - Origin (0,0) at top-left of map
 * 
 * @param x X coordinate on map (row index)
 * @param y Y coordinate on map (column index)
 * @param color SDL color to fill the cell with
 * 
 * @pre SDL renderer must be initialized
 * @pre Coordinates must be within map bounds
 * @post Cell is drawn with specified color
 * 
 * @note Cells are drawn 1 pixel smaller than CELL_SIZE to show grid lines
 * @warning Invalid coordinates are silently ignored
 */
extern void draw_cell(int x, int y, SDL_Color color);

/** @} */ // end of drawing_primitives group

/**
 * @defgroup entity_rendering Entity Rendering
 * @brief Functions for drawing game entities (drones, survivors)
 * @{
 */

/**
 * @brief Draw all drones with status-based color coding
 * 
 * Iterates through the global drones list and renders each drone
 * with appropriate colors based on their current status. Also draws
 * mission paths for drones that are actively pursuing targets.
 * 
 * **Color Scheme:**
 * - Blue: IDLE drones (available for missions)
 * - Green: ON_MISSION drones (actively rescuing)
 * - Green lines: Mission paths from drone to target
 * 
 * **Thread Safety:**
 * - Locks global drones list during iteration
 * - Locks individual drone mutexes for status reading
 * - Prevents race conditions with AI controller
 * 
 * @pre Global drones list must be initialized
 * @pre SDL renderer must be ready
 * @post All active drones are visually represented
 * 
 * @note Disconnected drones are not drawn
 * @warning Concurrent access protected by mutex locking
 */
extern void draw_drones();

/**
 * @brief Draw all active survivors on the map
 * 
 * Renders survivors from the global survivor array using red color.
 * Only draws survivors with status 0 (waiting) or 1 (being helped).
 * Rescued survivors (status 2+) are not displayed.
 * 
 * **Status Mapping:**
 * - Status 0: Waiting for rescue (red)
 * - Status 1: Being helped (red)
 * - Status 2+: Rescued (not drawn)
 * 
 * @pre Global survivor array must be initialized
 * @pre SDL renderer must be ready
 * @post All active survivors are visually represented
 * 
 * @note Thread-safe access to survivor array
 */
extern void draw_survivors();

/** @} */ // end of entity_rendering group

/**
 * @defgroup map_rendering Map Infrastructure
 * @brief Functions for drawing map structure and layout
 * @{
 */

/**
 * @brief Draw grid lines that represent the map structure
 * 
 * Creates a visual grid overlay showing the boundaries of each map cell.
 * Grid lines are drawn in white color to provide clear visual separation
 * between cells while maintaining readability.
 * 
 * **Grid Layout:**
 * - Horizontal lines: map.height + 1 lines
 * - Vertical lines: map.width + 1 lines
 * - Line color: White for maximum contrast
 * 
 * @pre Global map structure must be initialized
 * @pre SDL renderer must be ready
 * @post Grid overlay is drawn on the map area
 * 
 * @note Grid extends exactly to map boundaries
 */
extern void draw_grid();

/** @} */ // end of map_rendering group

/**
 * @defgroup ui_components User Interface Components
 * @brief Functions for drawing UI panels and text
 * @{
 */

/**
 * @brief Update the window title with current system statistics
 * 
 * Dynamically updates the SDL window title to show real-time statistics
 * including survivor counts and drone status. Provides quick overview
 * of system state without requiring detailed panel examination.
 * 
 * **Title Format:**
 * "Drone Simulator | Waiting: X | Being Helped: Y | Rescued: Z | Drones: N"
 * 
 * @pre Global statistics variables must be updated
 * @post Window title reflects current system state
 * 
 * @note Title updates do not affect rendering performance
 */
extern void update_window_title();

/**
 * @brief Draw the comprehensive information panel
 * 
 * Renders a detailed statistics panel on the right side of the window
 * showing real-time system metrics, drone status, and a visual legend.
 * The panel provides complete system oversight for operators.
 * 
 * **Panel Sections:**
 * 1. Title header with system name
 * 2. Survivor statistics (waiting, helped, rescued)
 * 3. Drone statistics (idle, on mission, total)
 * 4. Visual legend explaining color coding
 * 
 * **Layout:**
 * - Background: Light gray with dark borders
 * - Text: White on dark gray backgrounds
 * - Icons: Colored squares matching entity colors
 * 
 * @pre Global statistics must be current
 * @pre SDL renderer and fonts must be initialized
 * @post Complete information panel is rendered
 * 
 * @note Panel width is fixed at PANEL_WIDTH pixels
 */
extern void draw_info_panel();

/**
 * @brief Render text with SDL_ttf at specified position
 * 
 * High-level text rendering function that handles font selection,
 * surface creation, texture conversion, and cleanup. Supports both
 * regular and bold font styles with automatic fallback handling.
 * 
 * **Font Handling:**
 * - Attempts to use loaded TTF fonts
 * - Falls back to simple rectangle if fonts unavailable
 * - Supports bold/regular font selection
 * 
 * @param text Null-terminated string to render
 * @param x X position on screen (pixels from left)
 * @param y Y position on screen (pixels from top)
 * @param color SDL color for text rendering
 * @param use_bold Whether to use bold font variant
 * 
 * @pre SDL renderer must be initialized
 * @pre text must be valid null-terminated string
 * @post Text is rendered at specified position
 * 
 * @note Empty strings are handled gracefully
 * @warning Font fallback may result in simplified rendering
 */
extern void render_text(const char *text, int x, int y, SDL_Color color, bool use_bold);

/**
 * @brief Render a formatted text line in the info panel
 * 
 * Specialized function for creating consistent text lines within the
 * information panel. Each line includes a colored indicator, label text,
 * and numeric value with standardized formatting and spacing.
 * 
 * **Line Components:**
 * - Background rectangle with dark gray color
 * - Colored square indicator (10x10 pixels)
 * - Label text in white
 * - Numeric value in bold white
 * 
 * @param text Label text to display
 * @param y Y position within the panel
 * @param color Color for the indicator square
 * @param value Numeric value to display
 * 
 * @pre Info panel area must be available
 * @pre SDL renderer and fonts must be ready
 * @post Formatted text line is rendered in panel
 * 
 * @note Line height is standardized at TEXT_HEIGHT pixels
 */
extern void render_text_line(const char *text, int y, SDL_Color color, int value);

/** @} */ // end of ui_components group

/**
 * @defgroup main_rendering Main Rendering Pipeline
 * @brief High-level rendering and composition functions
 * @{
 */

/**
 * @brief Draw the complete scene including all elements
 * 
 * Main rendering function that composes the entire application view.
 * Clears the screen and draws all elements in the correct order to
 * create the complete visualization.
 * 
 * **Rendering Order:**
 * 1. Clear screen to black
 * 2. Draw survivors (red cells)
 * 3. Draw drones (blue/green cells and mission lines)
 * 4. Draw grid overlay (white lines)
 * 5. Update window title
 * 6. Draw information panel
 * 7. Present final frame
 * 
 * @return 0 on success, 1 on failure
 * 
 * @pre All subsystems must be initialized
 * @pre Global data structures must be accessible
 * @post Complete frame is rendered and presented
 * 
 * @note This is the main entry point for frame rendering
 * @warning Requires properly initialized SDL renderer
 */
extern int draw_map();

/** @} */ // end of main_rendering group

/**
 * @defgroup debugging Debug and Diagnostic Tools
 * @brief Functions for testing and troubleshooting visualization
 * @{
 */

/**
 * @brief Draw a test pattern for debugging visualization
 * 
 * Creates a checkerboard pattern using alternating blue and red cells
 * to verify that the rendering system is working correctly. Useful
 * for testing cell positioning and color rendering.
 * 
 * **Pattern:**
 * - Alternating blue and red cells
 * - Checkerboard layout based on (i+j) % 2
 * - Covers entire map area
 * 
 * @pre Map dimensions must be valid
 * @pre SDL renderer must be initialized
 * @post Checkerboard pattern fills the map area
 * 
 * @note Used for development and testing only
 */
extern void draw_test_pattern();

/**
 * @brief Draw diagnostic graphics for troubleshooting
 * 
 * Creates highly visible diagnostic markers to help identify rendering
 * issues and verify that the display system is functioning. Includes
 * distinctive patterns that are easy to recognize.
 * 
 * **Diagnostic Elements:**
 * - Large red X across entire window
 * - Bright green border around window perimeter
 * - Five test survivors at known positions
 * 
 * @pre SDL renderer must be initialized
 * @post Diagnostic pattern is drawn over entire window
 * 
 * @note Used for development and debugging only
 * @warning Overwrites normal display content
 */
extern void draw_diagnostic();

/** @} */ // end of debugging group

/**
 * @defgroup event_handling Event Processing
 * @brief Functions for handling user input and system events
 * @{
 */

/**
 * @brief Check for SDL events and handle user input
 * 
 * Processes all pending SDL events including window close requests
 * and keyboard input. Provides graceful shutdown mechanism for
 * user-initiated termination.
 * 
 * **Handled Events:**
 * - SDL_QUIT: Window close button clicked
 * - SDLK_ESCAPE: Escape key pressed
 * 
 * @return 1 if quit requested, 0 to continue execution
 * 
 * @pre SDL event system must be initialized
 * @post All pending events are processed
 * 
 * @note Should be called once per frame
 * @see main rendering loop in controller.c
 */
extern int check_events();

/** @} */ // end of event_handling group

/**
 * @defgroup cleanup Cleanup and Shutdown
 * @brief Functions for proper resource cleanup
 * @{
 */

/**
 * @brief Clean up all SDL resources before exit
 * 
 * Performs comprehensive cleanup of all SDL resources including
 * fonts, renderer, window, and SDL subsystems. Ensures proper
 * resource deallocation to prevent memory leaks.
 * 
 * **Cleanup Order:**
 * 1. Close TTF fonts (regular and bold)
 * 2. Destroy SDL renderer
 * 3. Destroy SDL window
 * 4. Quit SDL_ttf subsystem
 * 5. Quit SDL main subsystem
 * 
 * @pre SDL resources may or may not be initialized
 * @post All SDL resources are properly released
 * 
 * @note Safe to call multiple times
 * @note Handles NULL pointers gracefully
 */
extern void quit_all();
// clang-format on
/** @} */ // end of cleanup group

/** @} */ // end of visualization group

#endif // VIEW_H