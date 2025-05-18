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
    printf("Map initialized: height=%d, width=%d\n", map.height, map.width);

    // Initialize survivor array
    initialize_survivors();

    // Initialize SDL window
    printf("Initializing SDL window...\n");
    if (init_sdl_window() != 0) {
        fprintf(stderr, "Failed to initialize SDL window\n");
        cleanup_resources();
        cleanup_survivors();
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
    
    // Start survivor generator thread
    printf("Starting survivor generator thread...\n");
    int result = pthread_create(&survivor_thread, NULL, survivor_generator, NULL);
    if (result != 0) {
        fprintf(stderr, "Error creating survivor thread: %d\n", result);
        cleanup_resources();
        cleanup_survivors();
        return 1;
    }
    printf("Survivor generator thread started\n");
    
    // Wait for survivor generator to create initial survivors
    printf("Waiting for survivors to be generated...\n");
    sleep(3);
    
    // Start main loop
    printf("Starting main loop...\n");
    int frame_count = 0;
    running = 1;
    
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
        
        // Draw elements - grid, drones, and survivors
        draw_grid();
        draw_survivors();
        
        if (drone_fleet != NULL) {
            draw_drones();
        }
        
        // Print statistics every 50 frames
        if (frame_count % 50 == 0) {
            pthread_mutex_lock(&survivors_mutex);
            int current_survivors = num_survivors;
            pthread_mutex_unlock(&survivors_mutex);
            
            printf("Frame %d: Survivors: %d, Drones: %d\n", 
                   frame_count, current_survivors, num_drones);
        }
        
        // Update title
        pthread_mutex_lock(&survivors_mutex);
        int current_survivors = num_survivors;
        pthread_mutex_unlock(&survivors_mutex);
        
        char title[100];
        snprintf(title, sizeof(title), 
                "Drone Simulator | Survivors: %d | Drones: %d", 
                current_survivors, num_drones);
        SDL_SetWindowTitle(window, title);
        
        // Present the frame
        SDL_RenderPresent(renderer);
        
        // Delay for frame rate control
        SDL_Delay(100); // 10 FPS
        
        frame_count++;
    }
    
    printf("Exiting main loop...\n");
    
    // Cancel survivor thread
    printf("Canceling survivor thread...\n");
    pthread_cancel(survivor_thread);
    pthread_join(survivor_thread, NULL);
    
    // Cleanup
    cleanup_resources();
    cleanup_survivors();
    
    return 0;
}