/**
 * @file view.c
 * @brief SDL-based real-time visualization system for drone coordination display
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @version 0.1
 * @date 2025-05-22
 * 
 * This module implements the complete visualization system for the emergency
 * drone coordination application using SDL2 graphics library. It provides
 * real-time display of system state, interactive monitoring, and comprehensive
 * information panels for system operators.
 * 
 * **Visual Components:**
 * - Real-time map display with color-coded entities
 * - Grid-based layout showing drone and survivor positions
 * - Mission path visualization with dynamic line drawing
 * - Comprehensive information panel with live statistics
 * - Interactive window with event handling capabilities
 * 
 * **Color Coding System:**
 * - Red: Survivors awaiting rescue or being helped
 * - Blue: Idle drones available for mission assignment
 * - Green: Active drones on rescue missions
 * - Green lines: Mission paths from drones to targets
 * - White: Grid lines and text for clarity
 * 
 * **Information Display:**
 * - Real-time survivor counts by status (waiting, helped, rescued)
 * - Drone status breakdown (idle, on mission, total count)
 * - Visual legend explaining color coding system
 * - Dynamic window title with key statistics
 * - Performance-optimized rendering at 10 FPS
 * 
 * **Technical Features:**
 * - SDL2 hardware-accelerated rendering
 * - TTF font support with multiple fallback options
 * - Thread-safe data access with proper synchronization
 * - Scalable display system based on map dimensions
 * - Efficient redraw cycles with minimal overhead
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup visualization
 * @ingroup core_modules
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "headers/drone.h"
#include "headers/map.h"
#include "headers/survivor.h"
#include "headers/globals.h"

/** @brief Pixels per map cell */
#define CELL_SIZE 20

/** @brief Width of the right info panel */
#define PANEL_WIDTH 200

/** @brief Height for each line of text */
#define TEXT_HEIGHT 35

/** @brief Default font size */
#define FONT_SIZE 16

/** @brief Bold font size */
#define BOLD_FONT_SIZE 9

// SDL globals
// clang-format off
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Event event;
int window_width, window_height;

// Font
TTF_Font *font = NULL;
TTF_Font *font_bold = NULL;

// Colors
const SDL_Color BLACK = {0, 0, 0, 255};
const SDL_Color RED = {255, 0, 0, 255};
const SDL_Color BLUE = {0, 0, 255, 255};
const SDL_Color GREEN = {0, 255, 0, 255};
const SDL_Color WHITE = {255, 255, 255, 255};
const SDL_Color LIGHT_GRAY = {200, 200, 200, 255};
const SDL_Color DARK_GRAY = {50, 50, 50, 255};
const SDL_Color YELLOW = {255, 255, 0, 255};

// External references to controller's stats
extern int waiting_count;
extern int helped_count;
extern int rescued_count;
extern int idle_drones;
extern int mission_drones;

//format on
/**
 * @brief Initialize SDL window and renderer based on map dimensions plus info panel
 * 
 * Creates the main simulation window and loads fonts for text rendering
 * 
 * @return 0 on success, 1 on failure
 */
int init_sdl_window()
{
    // Calculate window dimensions to include both map and info panel
    window_width = (map.width * CELL_SIZE) + PANEL_WIDTH;
    window_height = map.height * CELL_SIZE;

    printf("Creating window with dimensions: %d x %d (including panel width: %d)\n",
           window_width, window_height, PANEL_WIDTH);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    // Initialize SDL_ttf
    if (TTF_Init() != 0)
    {
        fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    // Create window
    window = SDL_CreateWindow("Drone Simulator",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              window_width,
                              window_height,
                              SDL_WINDOW_SHOWN);
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        SDL_DestroyWindow(window);
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Load font (use your local fonts)
    font = TTF_OpenFont("fonts/bytebounce/ByteBounce.ttf", FONT_SIZE);
    if (!font)
    {
        // Try alternative font paths
        font = TTF_OpenFont("/usr/share/fonts/TTF/DejaVuSans.ttf", FONT_SIZE);
    }

    if (!font)
    {
        // Try another common font as fallback
        font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeSans.ttf", FONT_SIZE);
    }

    if (!font)
    {
        fprintf(stderr, "TTF_OpenFont Error: %s\n", TTF_GetError());
        fprintf(stderr, "Could not load font, continuing with simplified text\n");
    }

    // Load bold font
    font_bold = TTF_OpenFont("fonts/Press_Start_2P/PressStart2P-Regular.ttf", BOLD_FONT_SIZE);
    if (!font_bold && font)
    {
        // If we have a regular font, set it to bold
        TTF_SetFontStyle(font, TTF_STYLE_BOLD);
        font_bold = font;
    }

    // Set window title
    SDL_SetWindowTitle(window, "Drone Simulator");

    return 0;
}

/**
 * @brief Render text with SDL_ttf
 * 
 * Draws text on the screen at the specified position and color
 * 
 * @param text Text to render
 * @param x X position on screen
 * @param y Y position on screen
 * @param color Text color
 * @param use_bold Whether to use bold font
 */
void render_text(const char *text, int x, int y, SDL_Color color, bool use_bold)
{
    if (!text || strlen(text) == 0)
    {
        return;
    }

    // clang-format off
    TTF_Font *current_font = use_bold ? font_bold : font;
    // clang-format on
    if (!current_font)
    {
        // Fallback if font is not available - draw a simple rectangle
        // clang-format off
        SDL_Rect text_rect = {x, y, strlen(text) * 8, 3};
        // clang-format on
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &text_rect);
        return;
    }

    // clang-format off
    SDL_Surface *surface = TTF_RenderText_Blended(current_font, text, color);
    // clang-format on
    if (!surface)
    {
        fprintf(stderr, "TTF_RenderText_Blended Error: %s\n", TTF_GetError());
        return;
    }
    // clang-format off
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    // clang-format on
    if (!texture)
    {
        fprintf(stderr, "SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect rect = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, NULL, &rect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

/**
 * @brief Render a text line in the info panel with a label and value
 * 
 * Creates a standardized text line with background and value display
 * 
 * @param text Text label to render
 * @param y Y position in the panel
 * @param color Color for the label indicator
 * @param value Numeric value to display
 */
// clang-format off
void render_text_line(const char *text, int y, SDL_Color color, int value)
// clang-format on
{
    // Calculate the starting position for text
    int text_x = map.width * CELL_SIZE + 10; // 10px padding from panel start
    int text_y = y;

    // Background rectangle for label
    SDL_Rect text_bg = {
        text_x - 5,       // 5px padding before text
        text_y - 5,       // 5px padding above text
        PANEL_WIDTH - 10, // Panel width minus padding
        TEXT_HEIGHT       // Height of text area
    };

    // Draw background for text
    SDL_SetRenderDrawColor(renderer, DARK_GRAY.r, DARK_GRAY.g, DARK_GRAY.b, DARK_GRAY.a);
    SDL_RenderFillRect(renderer, &text_bg);

    // Draw a small colored square as a label indicator
    SDL_Rect indicator = { text_x, text_y + 5, 10, 10 };

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &indicator);

    // Render the text label
    render_text(text, text_x + 15, text_y + 5, WHITE, false);

    // Render the numeric value
    char value_str[16];
    sprintf(value_str, "%d", value);
    render_text(value_str, text_x + 120, text_y + 5, WHITE, true);
}

/**
 * @brief Update the window title with current statistics
 * 
 * Shows system statistics in the window title bar
 */
void update_window_title()
{
    // Update window title with stats from controller
    char title[100];
    snprintf(title,
             sizeof(title),
             "Drone Simulator | Waiting: %d | Being Helped: %d | Rescued: %d | Drones: %d",
             waiting_count,
             helped_count,
             rescued_count,
             num_drones);
    SDL_SetWindowTitle(window, title);
}

/**
 * @brief Draw the info panel on the right side of the window
 * 
 * Creates a detailed panel showing statistics and a legend
 */
void draw_info_panel()
{
    // First, draw the panel background
    SDL_Rect panel_rect = {
        map.width * CELL_SIZE, // X position (right after the map)
        0,                     // Y position (top of window)
        PANEL_WIDTH,           // Width of panel
        window_height          // Full height of window
    };

    // Set the panel background color
    SDL_SetRenderDrawColor(renderer, LIGHT_GRAY.r, LIGHT_GRAY.g, LIGHT_GRAY.b, LIGHT_GRAY.a);
    SDL_RenderFillRect(renderer, &panel_rect);

    // Draw panel border
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, BLACK.a);
    SDL_RenderDrawRect(renderer, &panel_rect);

    // Draw vertical separator line
    SDL_RenderDrawLine(renderer, map.width * CELL_SIZE, 0, map.width * CELL_SIZE, window_height);

    // Draw title
    SDL_Rect title_rect = {
        map.width * CELL_SIZE + 10, // X position with padding
        10,                         // Y position with padding
        PANEL_WIDTH - 20,           // Width minus padding
        40                          // Height
    };

    SDL_SetRenderDrawColor(renderer, BLUE.r, BLUE.g, BLUE.b, BLUE.a);
    SDL_RenderFillRect(renderer, &title_rect);

    // Draw title text
    render_text("DRONE SIMULATION", map.width * CELL_SIZE + 30, 20, WHITE, true);

    // Render stats with their actual values - using controller's stats
    int y_pos = 70; // Starting Y position for stats

    // Survivors waiting - RED
    render_text_line("Survivors Waiting:", y_pos, RED, waiting_count);

    // Survivors being helped - GREEN
    y_pos += TEXT_HEIGHT + 10; // Add spacing between lines
    render_text_line("Being Helped:", y_pos, GREEN, helped_count);

    // Survivors rescued - BLUE
    y_pos += TEXT_HEIGHT + 10;
    render_text_line("Rescued:", y_pos, BLUE, rescued_count);

    // Drones section
    y_pos += TEXT_HEIGHT + 30; // Add more spacing for section break

    // Draw section separator
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, BLACK.a);
    SDL_RenderDrawLine(
        renderer, map.width * CELL_SIZE + 10, y_pos - 15, map.width * CELL_SIZE + PANEL_WIDTH - 10, y_pos - 15);

    // Idle drones - BLUE
    render_text_line("Idle Drones:", y_pos, BLUE, idle_drones);

    // On mission drones - GREEN
    y_pos += TEXT_HEIGHT + 10;
    render_text_line("On Mission:", y_pos, GREEN, mission_drones);

    // Total drones count
    y_pos += TEXT_HEIGHT + 10;
    render_text_line("Total Drones:", y_pos, WHITE, drones->number_of_elements);

    // Legend section
    y_pos += TEXT_HEIGHT + 30; // Add more spacing for section break

    // Draw section separator
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, BLACK.a);
    SDL_RenderDrawLine(
        renderer, map.width * CELL_SIZE + 10, y_pos - 15, map.width * CELL_SIZE + PANEL_WIDTH - 10, y_pos - 15);

    // Legend title
    SDL_Rect legend_title = {
        map.width * CELL_SIZE + 10, // X position with padding
        y_pos,                      // Current Y position
        PANEL_WIDTH - 20,           // Width minus padding
        30                          // Height
    };

    SDL_SetRenderDrawColor(renderer, DARK_GRAY.r, DARK_GRAY.g, DARK_GRAY.b, DARK_GRAY.a);
    SDL_RenderFillRect(renderer, &legend_title);

    // Draw legend title text
    render_text("LEGEND", map.width * CELL_SIZE + 75, y_pos + 5, WHITE, true);

    // Legend items
    y_pos += 40;

    // Survivor - RED
    SDL_Rect survivor_icon = { map.width * CELL_SIZE + 20, y_pos + 5, 15, 15 };
    SDL_SetRenderDrawColor(renderer, RED.r, RED.g, RED.b, RED.a);
    SDL_RenderFillRect(renderer, &survivor_icon);

    // Legend text
    render_text("Survivor", map.width * CELL_SIZE + 45, y_pos + 5, WHITE, false);

    // Idle drone - BLUE
    y_pos += 25;
    SDL_Rect idle_icon = { map.width * CELL_SIZE + 20, y_pos + 5, 15, 15 };
    SDL_SetRenderDrawColor(renderer, BLUE.r, BLUE.g, BLUE.b, BLUE.a);
    SDL_RenderFillRect(renderer, &idle_icon);

    // Legend text
    render_text("Idle Drone", map.width * CELL_SIZE + 45, y_pos + 5, WHITE, false);

    // Active drone - GREEN
    y_pos += 25;
    SDL_Rect active_icon = { map.width * CELL_SIZE + 20, y_pos + 5, 15, 15 };
    SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, GREEN.a);
    SDL_RenderFillRect(renderer, &active_icon);

    // Legend text
    render_text("Active Drone", map.width * CELL_SIZE + 45, y_pos + 5, WHITE, false);

    // Mission line - GREEN
    y_pos += 25;
    SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, GREEN.a);
    SDL_RenderDrawLine(renderer, map.width * CELL_SIZE + 20, y_pos + 12, map.width * CELL_SIZE + 40, y_pos + 12);

    // Legend text
    render_text("Mission Path", map.width * CELL_SIZE + 45, y_pos + 5, WHITE, false);
}

/**
 * @brief Draw a colored cell at the specified map coordinates
 * 
 * Fills a single grid cell with the specified color
 * 
 * @param x Map x-coordinate
 * @param y Map y-coordinate
 * @param color Color to fill the cell with
 */
void draw_cell(int x, int y, SDL_Color color)
{
    // Boundary check to prevent invalid memory access
    if (x < 0 || x >= map.height || y < 0 || y >= map.width)
    {
        return;
    }

    // Create a rectangle for the cell
    SDL_Rect rect = {
        y * CELL_SIZE, // Note: x and y are transposed for SDL
        x * CELL_SIZE,
        CELL_SIZE - 1, // Make slightly smaller than cell
        CELL_SIZE - 1  // to ensure grid lines remain visible
    };

    // Set the color
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    // Draw the cell
    SDL_RenderFillRect(renderer, &rect);
}

/**
 * @brief Draw all drones with colors indicating their status
 * 
 * Blue for IDLE drones, Green for ON_MISSION drones
 * Also draws lines between drones and their targets when on mission
 */
void draw_drones()
{
    // Lock the drones list for iteration
    pthread_mutex_lock(&drones->lock);

    // Iterate through all drones in the list
    // clang-format off
    Node *current = drones->head;
    // clang-format on
    while (current != NULL)
    {
        // clang-format off
        Drone *d = (Drone *)current->data;
        // clang-format on
        // Lock this specific drone while drawing it
        pthread_mutex_lock(&d->lock);

        if (d->status != DISCONNECTED)
        {

            // Choose color based on drone status
            SDL_Color color = (d->status == IDLE) ? BLUE : GREEN;

            // Draw the drone
            draw_cell(d->coord.x, d->coord.y, color);

            // Draw mission line if on mission
            if (d->status == ON_MISSION)
            {
                SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, GREEN.a);
                SDL_RenderDrawLine(renderer,
                                   d->coord.y * CELL_SIZE + CELL_SIZE / 2,
                                   d->coord.x * CELL_SIZE + CELL_SIZE / 2,
                                   d->target.y * CELL_SIZE + CELL_SIZE / 2,
                                   d->target.x * CELL_SIZE + CELL_SIZE / 2);
            }
        }

        pthread_mutex_unlock(&d->lock);
        current = current->next;
    }

    pthread_mutex_unlock(&drones->lock);
}

/**
 * @brief Draw all active survivors on the map in red
 * 
 * Only draws survivors with status 0 (waiting) or 1 (being helped)
 */
void draw_survivors()
{
    // Lock the mutex before accessing the survivor array
    pthread_mutex_lock(&survivors_mutex);

    // Draw each survivor in the array
    for (int i = 0; i < num_survivors; i++)
    {
        // Only draw survivors that are waiting for help (status 0) or being helped (status 1)
        // Don't draw rescued survivors (status 2 or 3)
        if (survivor_array[i].status == 0 || survivor_array[i].status == 1)
        {
            draw_cell(survivor_array[i].coord.x, survivor_array[i].coord.y, RED);
        }
    }

    // Unlock after reading the array
    pthread_mutex_unlock(&survivors_mutex);
}

/**
 * @brief Draw the grid lines that represent the map
 * 
 * Creates a visual grid for the map area
 */
void draw_grid()
{
    SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b, WHITE.a);

    // Draw horizontal grid lines
    for (int i = 0; i <= map.height; i++)
    {
        SDL_RenderDrawLine(renderer, 0, i * CELL_SIZE, map.width * CELL_SIZE, i * CELL_SIZE);
    }

    // Draw vertical grid lines
    for (int j = 0; j <= map.width; j++)
    {
        SDL_RenderDrawLine(renderer, j * CELL_SIZE, 0, j * CELL_SIZE, window_height);
    }
}

/**
 * @brief Draw a test pattern for debugging visualization
 * 
 * Creates a checkerboard pattern to verify rendering is working
 */
void draw_test_pattern()
{
    for (int i = 0; i < map.height; i++)
    {
        for (int j = 0; j < map.width; j++)
        {
            if ((i + j) % 2 == 0)
            {
                draw_cell(i, j, BLUE);
            }
            else
            {
                draw_cell(i, j, RED);
            }
        }
    }
}

/**
 * @brief Draw the entire map including grid, survivors, drones, and info panel
 * 
 * Main rendering function that composes the complete view
 * 
 * @return 0 on success, 1 on failure
 */
int draw_map()
{
    if (!renderer)
    {
        fprintf(stderr, "Error: Renderer not initialized in draw_map()\n");
        return 1;
    }

    // Clear the screen with black
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, BLACK.a);
    SDL_RenderClear(renderer);

    // Draw all elements
    draw_survivors();
    draw_drones();
    draw_grid();

    // Update window title and draw the info panel
    update_window_title();
    draw_info_panel();

    // Present the rendered frame
    SDL_RenderPresent(renderer);
    return 0;
}

/**
 * @brief Draw diagnostic graphics for troubleshooting
 * 
 * Creates visual markers to help identify rendering issues
 */
void draw_diagnostic()
{
    // Draw a large red X across the entire window
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderDrawLine(renderer, 0, 0, window_width, window_height);
    SDL_RenderDrawLine(renderer, 0, window_height, window_width, 0);

    // Draw a bright green border around the entire window
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderDrawLine(renderer, 0, 0, window_width, 0);
    SDL_RenderDrawLine(renderer, window_width, 0, window_width, window_height);
    SDL_RenderDrawLine(renderer, window_width, window_height, 0, window_height);
    SDL_RenderDrawLine(renderer, 0, window_height, 0, 0);

    // Draw diagnostic survivors
    for (int i = 0; i < 5; i++)
    {
        int x, y;
        switch (i)
        {
            case 0:
                x = 5;
                y = 5;
                break; // Top left
            case 1:
                x = 5;
                y = 25;
                break; // Top right
            case 2:
                x = 20;
                y = 15;
                break; // Center
            case 3:
                x = 35;
                y = 5;
                break; // Bottom left
            case 4:
                x = 35;
                y = 25;
                break; // Bottom right
        }

        draw_cell(x, y, RED);
    }
}

/**
 * @brief Check SDL events for user input
 * 
 * Processes SDL events like window close or escape key
 * 
 * @return 1 if quit requested, 0 otherwise
 */
int check_events()
{
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
            return 1;
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            return 1;
    }
    return 0;
}

/**
 * @brief Clean up SDL resources
 * 
 * Frees all SDL resources before program exit
 */
void quit_all()
{
    if (font && font != font_bold)
    {
        TTF_CloseFont(font);
    }

    if (font_bold)
    {
        TTF_CloseFont(font_bold);
    }

    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
    }

    if (window)
    {
        SDL_DestroyWindow(window);
    }

    TTF_Quit();
    SDL_Quit();
}