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
    // Boundary check to prevent invalid memory access
    if (x < 0 || x >= map.height || y < 0 || y >= map.width) {
        printf("WARNING: Attempted to draw cell at invalid position (%d,%d)\n", x, y);
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
    
    printf("Drew cell at (%d,%d) = pixel (%d,%d) with color (%d,%d,%d)\n",
           x, y, y * CELL_SIZE, x * CELL_SIZE, color.r, color.g, color.b);
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
    
    // Lock the mutex before accessing the survivor array
    pthread_mutex_lock(&survivors_mutex);
    
    // Draw each survivor in the array
    for (int i = 0; i < num_survivors; i++) {
        // Only draw survivors that are waiting for help (status 0)
        if (survivor_array[i].status == 0) {
            draw_cell(survivor_array[i].coord.x, survivor_array[i].coord.y, RED);
            survivors_drawn++;
            printf("Drawing survivor at (%d,%d)\n", 
                   survivor_array[i].coord.x, survivor_array[i].coord.y);
        }
    }
    
    // Unlock after reading the array
    pthread_mutex_unlock(&survivors_mutex);
    
    if (survivors_drawn > 0) {
        printf("Drew %d survivors from survivor array\n", survivors_drawn);
    } else {
        printf("No survivors found to draw\n");
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
    pthread_mutex_lock(&survivors_mutex);
    int survivor_count = num_survivors;
    pthread_mutex_unlock(&survivors_mutex);
    
    pthread_mutex_lock(&helpedsurvivors->lock);
    int helped_count = helpedsurvivors->number_of_elements;
    pthread_mutex_unlock(&helpedsurvivors->lock);
    
    char title[100];
    snprintf(title, sizeof(title), 
             "Drone Simulator | Survivors: %d | Helped: %d | Drones: %d",
             survivor_count, helped_count, num_drones);
    SDL_SetWindowTitle(window, title);

    // Present the rendered frame
    SDL_RenderPresent(renderer);
    return 0;
}

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
        printf("Drawing diagnostic survivor at position (%d,%d)\n", x, y);
    }
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