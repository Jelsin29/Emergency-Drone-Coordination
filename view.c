#include <SDL2/SDL.h>
#include <stdio.h>

#include "headers/drone.h"
#include "headers/map.h"
#include "headers/survivor.h"

#define CELL_SIZE 20  // Pixels per map cell

// SDL globals
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Event event;
int window_width, window_height;

// Colors
const SDL_Color BLACK = {0, 0, 0, 255};
const SDL_Color RED = {255, 0, 0, 255};
const SDL_Color BLUE = {0, 0, 255, 255};
const SDL_Color GREEN = {0, 255, 0, 255};
const SDL_Color WHITE = {255, 255, 255, 255};

/**
 * Initialize SDL window and renderer based on map dimensions
 * @return 0 on success, 1 on failure
 */
int init_sdl_window() {
    window_width = map.width * CELL_SIZE;
    window_height = map.height * CELL_SIZE;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    window =
        SDL_CreateWindow("Drone Simulator", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, window_width,
                         window_height, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n",
                SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_DestroyWindow(window);
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n",
                SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Update window title with initial stats
    char title[100];
    snprintf(title, sizeof(title), "Drone Simulator | Survivors: 0 | Helped: 0 | Drones: %d", num_drones);
    SDL_SetWindowTitle(window, title);
    
    return 0;
}

/**
 * Draw a colored cell at the specified map coordinates
 * @param x Map x-coordinate
 * @param y Map y-coordinate
 * @param color Color to fill the cell with
 */
void draw_cell(int x, int y, SDL_Color color) {
    // Boundary check to prevent invalid memory access
    if (x < 0 || x >= map.height || y < 0 || y >= map.width) {
        return;
    }
    
    // Create a rectangle for the cell
    SDL_Rect rect = {
        y * CELL_SIZE,     // Note: x and y are transposed for SDL
        x * CELL_SIZE, 
        CELL_SIZE - 1,     // Make slightly smaller than cell
        CELL_SIZE - 1      // to ensure grid lines remain visible
    };
    
    // Set the color
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    // Draw the cell
    SDL_RenderFillRect(renderer, &rect);
}

/**
 * Draw all drones with colors indicating their status
 * Blue = IDLE, Green = ON_MISSION
 * Also draws lines between drones and their targets when on mission
 */
void draw_drones() {
    if (drone_fleet == NULL) {
        return;
    }
    
    for (int i = 0; i < num_drones; i++) {
        pthread_mutex_lock(&drone_fleet[i].lock);
        
        // Choose color based on drone status
        SDL_Color color = (drone_fleet[i].status == IDLE) ? BLUE : GREEN;
        
        // Draw the drone
        draw_cell(drone_fleet[i].coord.x, drone_fleet[i].coord.y, color);
        
        // Draw mission line if on mission
        if (drone_fleet[i].status == ON_MISSION) {
            SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, GREEN.a);
            SDL_RenderDrawLine(
                renderer,
                drone_fleet[i].coord.y * CELL_SIZE + CELL_SIZE / 2,
                drone_fleet[i].coord.x * CELL_SIZE + CELL_SIZE / 2,
                drone_fleet[i].target.y * CELL_SIZE + CELL_SIZE / 2,
                drone_fleet[i].target.x * CELL_SIZE + CELL_SIZE / 2);
        }
        
        pthread_mutex_unlock(&drone_fleet[i].lock);
    }
}

/**
 * Draw all active survivors (status 0 or 1) on the map in red
 * Status 0 = Waiting for help, Status 1 = Being helped
 */
void draw_survivors() {
    // Lock the mutex before accessing the survivor array
    pthread_mutex_lock(&survivors_mutex);
    
    // Draw each survivor in the array
    for (int i = 0; i < num_survivors; i++) {
        // Only draw survivors that are waiting for help (status 0) or being helped (status 1)
        // Don't draw rescued survivors (status 2)
        if (survivor_array[i].status == 0 || survivor_array[i].status == 1) {
            draw_cell(survivor_array[i].coord.x, survivor_array[i].coord.y, RED);
        }
    }
    
    // Unlock after reading the array
    pthread_mutex_unlock(&survivors_mutex);
}

/**
 * Draw the grid lines that represent the map
 */
void draw_grid() {
    SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b,
                           WHITE.a);
    for (int i = 0; i <= map.height; i++) {
        SDL_RenderDrawLine(renderer, 0, i * CELL_SIZE, window_width,
                           i * CELL_SIZE);
    }
    for (int j = 0; j <= map.width; j++) {
        SDL_RenderDrawLine(renderer, j * CELL_SIZE, 0, j * CELL_SIZE,
                           window_height);
    }
}

/**
 * Draw a test pattern for debugging visualization
 */
void draw_test_pattern() {
    for (int i = 0; i < map.height; i++) {
        for (int j = 0; j < map.width; j++) {
            if ((i + j) % 2 == 0) {
                draw_cell(i, j, BLUE);
            } else {
                draw_cell(i, j, RED);
            }
        }
    }
}

/**
 * Draw the entire map including grid, survivors, and drones
 * @return 0 on success, 1 on failure
 */
int draw_map() {
    if (!renderer) {
        fprintf(stderr, "Error: Renderer not initialized in draw_map()\n");
        return 1;
    }
    
    // Clear the screen with black
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b,
                           BLACK.a);
    SDL_RenderClear(renderer);

    // Draw all elements
    draw_survivors();
    draw_drones();
    draw_grid();
    
    // Update the window title with current statistics
    pthread_mutex_lock(&survivors_mutex);
    int survivor_count = 0;
    int helped_count = 0;
    int rescued_count = 0;
    
    for (int i = 0; i < num_survivors; i++) {
        if (survivor_array[i].status == 0) {
            survivor_count++;
        } else if (survivor_array[i].status == 1) {
            helped_count++;
        } else if (survivor_array[i].status == 2) {
            rescued_count++;
        }
    }
    pthread_mutex_unlock(&survivors_mutex);
    
    char title[100];
    snprintf(title, sizeof(title), 
             "Drone Simulator | Waiting: %d | Being Helped: %d | Rescued: %d | Drones: %d",
             survivor_count, helped_count, rescued_count, num_drones);
    SDL_SetWindowTitle(window, title);

    // Present the rendered frame
    SDL_RenderPresent(renderer);
    return 0;
}

/**
 * Draw diagnostic graphics for troubleshooting
 */
void draw_diagnostic() {
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
    for (int i = 0; i < 5; i++) {
        int x, y;
        switch (i) {
            case 0: x = 5; y = 5; break;    // Top left
            case 1: x = 5; y = 25; break;   // Top right
            case 2: x = 20; y = 15; break;  // Center
            case 3: x = 35; y = 5; break;   // Bottom left
            case 4: x = 35; y = 25; break;  // Bottom right
        }
        
        draw_cell(x, y, RED);
    }
}

/**
 * Check SDL events for user input
 * @return 1 if quit requested, 0 otherwise
 */
int check_events() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) return 1;
        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.sym == SDLK_ESCAPE)
            return 1;
    }
    return 0;
}

/**
 * Clean up SDL resources
 */
void quit_all() {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}