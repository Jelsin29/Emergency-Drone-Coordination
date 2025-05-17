// controller.c
#include "headers/globals.h"
#include "headers/map.h"
#include "headers/drone.h"
#include "headers/survivor.h"
#include "headers/ai.h"
#include "headers/list.h"
#include "headers/view.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

// Global lists defined in globals.h
List *survivors = NULL;
List *helpedsurvivors = NULL;
List *drones = NULL;

// Thread IDs for cleanup
pthread_t survivor_thread;
pthread_t ai_thread;

// Graceful shutdown flag
volatile int running = 1;

// Signal handler for graceful shutdown
void handle_signal(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;
}

void initialize_lists() {
    printf("Initializing global lists...\n");
    
    // Create lists with appropriate capacities
    survivors = create_list(sizeof(Survivor), 1000);
    if (!survivors) {
        fprintf(stderr, "Failed to create survivors list\n");
        exit(EXIT_FAILURE);
    }
    
    helpedsurvivors = create_list(sizeof(Survivor), 1000);
    if (!helpedsurvivors) {
        fprintf(stderr, "Failed to create helpedsurvivors list\n");
        exit(EXIT_FAILURE);
    }
    
    drones = create_list(sizeof(Drone), 100);
    if (!drones) {
        fprintf(stderr, "Failed to create drones list\n");
        exit(EXIT_FAILURE);
    }
    
    printf("Global lists initialized\n");
}

void cleanup_resources() {
    printf("Cleaning up resources...\n");
    
    // Cancel threads
    if (drone_fleet != NULL) {
        printf("Canceling drone threads...\n");
        for (int i = 0; i < num_drones; i++) {
            pthread_cancel(drone_fleet[i].thread_id);
            pthread_mutex_destroy(&drone_fleet[i].lock);
        }
        printf("Freeing drone fleet memory...\n");
        free(drone_fleet);
        drone_fleet = NULL;
    }
    
    // Free map resources
    printf("Freeing map...\n");
    freemap();
    
    // Destroy lists
    printf("Destroying lists...\n");
    if (survivors) survivors->destroy(survivors);
    if (helpedsurvivors) helpedsurvivors->destroy(helpedsurvivors);
    if (drones) drones->destroy(drones);
    
    // Cleanup SDL
    printf("Cleaning up SDL...\n");
    quit_all();
    
    printf("All resources cleaned up\n");
}

int main() {
    printf("Emergency Drone Coordination System - Phase 1\n");
    printf("---------------------------------------------\n");
    
    // Set up signal handler for Ctrl+C
    signal(SIGINT, handle_signal);
    
    // Initialize global lists
    printf("Initializing global lists...\n");
    initialize_lists();
    
    // Initialize map (40x30 grid)
    printf("Initializing map...\n");
    init_map(40, 30);
    printf("Map initialized: %dx%d\n", map.width, map.height);

    // Initialize SDL window
    printf("Initializing SDL window...\n");
    if (init_sdl_window() != 0) {
        fprintf(stderr, "Failed to initialize SDL window\n");
        // Cleanup what we've created so far
        if (survivors) survivors->destroy(survivors);
        if (helpedsurvivors) helpedsurvivors->destroy(helpedsurvivors);
        if (drones) drones->destroy(drones);
        freemap();
        return 1;
    }
    printf("SDL window initialized successfully\n");
    
    // Test grid drawing
    printf("Phase 1: Testing grid drawing...\n");
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    draw_grid();
    SDL_RenderPresent(renderer);
    printf("Grid drawn, pausing for 2 seconds...\n");
    sleep(2);
    
    // Initialize drones
    printf("Initializing drones...\n");
    initialize_drones();
    printf("Drones initialized successfully\n");
    
    // Start a simplified main loop
    printf("Starting simplified main loop...\n");
    int frame_count = 0;
    running = 1;
    
    // Create a fixed test survivor for display testing
    printf("Creating fixed test survivor at (15,15)\n");
    SDL_RenderClear(renderer);
    draw_grid();
    
    // Draw a fixed red square at (15,15) - use the color constant RED now that it's exposed
    draw_cell(15, 15, RED);
    SDL_RenderPresent(renderer);
    printf("Test survivor drawn, pausing for 2 seconds...\n");
    sleep(2);
    
    while (running) {
        // Process SDL events
        if (check_events()) {
            printf("Exit event detected\n");
            running = 0;
            break;
        }
        
        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        // Draw elements - grid and drones first
        draw_grid();
        if (drone_fleet != NULL) {
            draw_drones();
        }
        
        // Draw a fixed test survivor at (15,15)
        draw_cell(15, 15, RED);
        
        // Print a heartbeat message every 50 frames
        if (frame_count % 50 == 0) {
            printf("Frame %d: Drawing grid, drones, and test survivor\n", frame_count);
        }
        
        // Update title
        char title[100];
        snprintf(title, sizeof(title), 
                "Drone Simulator | Test Survivor at (15,15) | Drones: %d", 
                num_drones);
        SDL_SetWindowTitle(window, title);
        
        // Present the frame
        SDL_RenderPresent(renderer);
        
        // Delay for frame rate control
        SDL_Delay(100); // 10 FPS
        
        frame_count++;
    }
    
    printf("Exiting main loop...\n");
    
    // Cleanup
    cleanup_resources();
    
    return 0;
}