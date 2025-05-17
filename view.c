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

int init_sdl_window() {
    window_width = map.width * CELL_SIZE;
    window_height = map.height * CELL_SIZE;
    
    printf("Window dimensions: %dx%d\n", window_width, window_height);

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

void draw_cell(int x, int y, SDL_Color color) {
    // PRINT exact pixel values for debugging
    printf("Drawing cell at (%d,%d) = pixel (%d,%d) with color (%d,%d,%d)\n",
           x, y, y * CELL_SIZE, x * CELL_SIZE, color.r, color.g, color.b);
    
    // Make cells MUCH larger and more visible
    SDL_Rect rect = {
        y * CELL_SIZE,     // Note: x and y are transposed
        x * CELL_SIZE, 
        CELL_SIZE - 1,     // Make slightly smaller than cell
        CELL_SIZE - 1      // to ensure grid lines remain visible
    };
    
    // Set the color
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    // Draw the cell
    SDL_RenderFillRect(renderer, &rect);
}

void draw_drones() {
    int drones_drawn = 0;
    
    if (drone_fleet == NULL) {
        printf("Warning: drone_fleet is NULL in draw_drones()\n");
        return;
    }
    
    for (int i = 0; i < num_drones; i++) {
        pthread_mutex_lock(&drone_fleet[i].lock);
        
        // Choose color based on drone status
        SDL_Color color = (drone_fleet[i].status == IDLE) ? BLUE : GREEN;
        
        // Draw the drone
        draw_cell(drone_fleet[i].coord.x, drone_fleet[i].coord.y, color);
        drones_drawn++;
        
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
    
    if (drones_drawn > 0) {
        printf("Drew %d drones\n", drones_drawn);
    } else {
        printf("No drones drawn\n");
    }
}

void draw_survivors() {
    int survivors_drawn = 0;
    
    // Draw survivors from the map cells
    for (int i = 0; i < map.height; i++) {
        for (int j = 0; j < map.width; j++) {
            pthread_mutex_lock(&map.cells[i][j].survivors->lock);
            
            // Check if there are any survivors in this cell
            if (map.cells[i][j].survivors->number_of_elements > 0) {
                // Draw a red cell for survivors
                draw_cell(i, j, RED);
                survivors_drawn++;
                printf("Drawing survivor at map cell (%d,%d), count in cell: %d\n", 
                       i, j, map.cells[i][j].survivors->number_of_elements);
            }
            
            pthread_mutex_unlock(&map.cells[i][j].survivors->lock);
        }
    }
    
    // Debug: print how many we drew
    if (survivors_drawn > 0) {
        printf("Drew %d survivors from map cells\n", survivors_drawn);
    } else {
        printf("No survivors found in map cells to draw\n");
        
        // Fallback: Draw the fixed test survivors 
        printf("Drawing fallback test survivors\n");
        draw_cell(5, 5, RED);
        draw_cell(5, 25, RED);
        draw_cell(20, 15, RED);
        draw_cell(35, 5, RED);
        draw_cell(35, 25, RED);
    }
}

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

// Add this function to draw a test pattern if needed
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
    char title[100];
    snprintf(title, sizeof(title), 
             "Drone Simulator | Survivors: %d | Helped: %d | Drones: %d",
             survivors ? survivors->number_of_elements : 0,
             helpedsurvivors ? helpedsurvivors->number_of_elements : 0,
             num_drones);
    SDL_SetWindowTitle(window, title);

    // Present the rendered frame
    SDL_RenderPresent(renderer);
    return 0;
}

void draw_diagnostic() {
    // Draw a large red X across the entire window (WORKING)
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderDrawLine(renderer, 0, 0, window_width, window_height);
    SDL_RenderDrawLine(renderer, 0, window_height, window_width, 0);
    
    // Draw a bright green border around the entire window (WORKING)
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderDrawLine(renderer, 0, 0, window_width, 0);
    SDL_RenderDrawLine(renderer, window_width, 0, window_width, window_height);
    SDL_RenderDrawLine(renderer, window_width, window_height, 0, window_height);
    SDL_RenderDrawLine(renderer, 0, window_height, 0, 0);
    
    // Try the most basic rectangle possible
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White
    SDL_Rect rect1 = { 100, 100, 100, 100 }; // Simple square at 100,100
    SDL_RenderFillRect(renderer, &rect1);
    
    // Try another rectangle with different color
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Blue
    SDL_Rect rect2 = { 300, 300, 100, 100 }; // Simple square at 300,300
    SDL_RenderFillRect(renderer, &rect2);
    
    printf("Drawing basic test rectangles at (100,100) and (300,300)\n");
}

int check_events() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) return 1;
        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.sym == SDLK_ESCAPE)
            return 1;
    }
    return 0;
}

void quit_all() {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}