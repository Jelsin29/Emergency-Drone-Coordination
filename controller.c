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
#include <sys/socket.h>

// Global lists defined in globals.h
List *survivors = NULL;
List *helpedsurvivors = NULL;
List *drones = NULL;

// Thread IDs for cleanup
pthread_t survivor_thread;
pthread_t drone_server_thread;
pthread_t ai_thread;

// Graceful shutdown flag
volatile int running = 1;

// Statistics variables - made global for view.c to access
int waiting_count = 0;
int helped_count = 0;
int rescued_count = 0;
int idle_drones = 0;
int mission_drones = 0;

/**
 * Signal handler for graceful shutdown
 * @param sig Signal number
 */
void handle_signal(int sig)
{
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;
}

/**
 * Initialize global synchronized lists
 */
void initialize_lists() {
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
    
    // DEBUG PRINT - ADD THIS
    printf("Lists initialized. Drone list capacity: %d\n", drones->capacity);
}

/**
 * Clean up all system resources
 */
void cleanup_resources()
{
    // Free map resources
    freemap();

    // Destroy lists
    if (survivors)
        survivors->destroy(survivors);
    if (helpedsurvivors)
        helpedsurvivors->destroy(helpedsurvivors);
    if (drones)
        drones->destroy(drones);

    // Cleanup SDL
    quit_all();
}

/**
 * Update all statistics for the simulation
 * Used by both controller and view
 */
void update_simulation_stats() {
    // Reset counters
    waiting_count = 0;
    helped_count = 0;
    idle_drones = 0;
    mission_drones = 0;
    
    // Count survivors by status
    pthread_mutex_lock(&survivors_mutex);
    for (int i = 0; i < num_survivors; i++) {
        if (survivor_array[i].status == 0) {
            waiting_count++;
        }
        else if (survivor_array[i].status == 1) {
            helped_count++;
        }
        else if (survivor_array[i].status == 2) {
            survivor_array[i].status = 3;
            rescued_count++;
        }
    }
    pthread_mutex_unlock(&survivors_mutex);
    
    // Count drones by status from the list
    pthread_mutex_lock(&drones->lock);
    
    int disconnected_drones = 0; // Track disconnected drones separately
    
    // Iterate through all drones in the list
    Node* current = drones->head;
    while (current != NULL) {
        Drone* d = (Drone*)current->data;
        
        // Lock this specific drone to check its status
        pthread_mutex_lock(&d->lock);
        
        if (d->status == IDLE) {
            idle_drones++;
        }
        else if (d->status == ON_MISSION) {
            mission_drones++;
        }
        else if (d->status == DISCONNECTED) {
            disconnected_drones++;
        }
        
        pthread_mutex_unlock(&d->lock);
        current = current->next;
    }
    
    // Update the global num_drones to reflect only active drones
    num_drones = idle_drones + mission_drones;
    
    // Debug print for disconnected drones
    if (disconnected_drones > 0) {
        printf("Stats: Active drones: %d, Disconnected drones: %d\n", 
               num_drones, disconnected_drones);
    }
    
    pthread_mutex_unlock(&drones->lock);
}

void cleanup_disconnected_drones() {
    printf("Running cleanup_disconnected_drones()\n");
    
    pthread_mutex_lock(&drones->lock);
    
    Node* current = drones->head;
    int removed = 0;
    
    while (current != NULL) {
        Drone* d = (Drone*)current->data;
        Node* next_node = current->next; // Save next pointer before potential removal
        
        pthread_mutex_lock(&d->lock);
        if (d->status == DISCONNECTED) {
            // Calculate time since disconnection
            time_t now = time(NULL);
            struct tm now_tm;
            localtime_r(&now, &now_tm);
            
            // Get seconds since last update
            time_t last_update_time = mktime(&d->last_update);
            double seconds_disconnected = difftime(now, last_update_time);
            
            printf("Drone %d has been disconnected for %.1f seconds\n", d->id, seconds_disconnected);
            
            // If disconnected for more than 5 seconds, remove from list (reduced from 30)
            if (seconds_disconnected > 5.0) {
                pthread_mutex_unlock(&d->lock);
                
                // Close socket if still open
                if (d->socket > 0) {
                    close(d->socket);
                    d->socket = -1;
                }
                
                // Remove from the list
                drones->removenode(drones, current);
                
                printf("Removed disconnected drone %d after timeout\n", d->id);
                removed++;
                
                current = next_node;
                continue;
            }
        }
        pthread_mutex_unlock(&d->lock);
        
        current = next_node;
    }
    
    printf("Cleanup complete, removed %d drones\n", removed);
    pthread_mutex_unlock(&drones->lock);
}

/**
 * Main function - entry point for the drone coordination system
 */
int main() {
    printf("Emergency Drone Coordination System - Phase 2\n");
    printf("---------------------------------------------\n");

    // Set up signal handler for Ctrl+C
    signal(SIGINT, handle_signal);

    // Initialize global lists
    initialize_lists();

    // Initialize map (40x30 grid)
    init_map(30, 40);

    // Initialize survivor array
    initialize_survivors();

    // Initialize SDL window
    if (init_sdl_window() != 0) {
        fprintf(stderr, "Failed to initialize SDL window\n");
        cleanup_resources();
        cleanup_survivors();
        return 1;
    }

    // Draw initial grid
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    draw_grid();
    SDL_RenderPresent(renderer);

    // Start drone server thread
    int result = pthread_create(&drone_server_thread, NULL, drone_server, NULL);
    if (result != 0) {
        fprintf(stderr, "Error creating drone server thread: %d\n", result);
        cleanup_resources();
        cleanup_survivors();
        return 1;
    }

    // Start survivor generator thread
    int survivor_result = pthread_create(&survivor_thread, NULL, survivor_generator, NULL);
    if (survivor_result != 0) {
        fprintf(stderr, "Error creating survivor thread: %d\n", survivor_result);
        cleanup_resources();
        cleanup_survivors();
        return 1;
    }

    // Start AI controller thread
    int ai_result = pthread_create(&ai_thread, NULL, ai_controller, NULL);
    if (ai_result != 0) {
        fprintf(stderr, "Error creating AI controller thread: %d\n", ai_result);
        cleanup_resources();
        cleanup_survivors();
        return 1;
    }

    // Start main rendering loop
    int frame_count = 0;
    running = 1;
    
    // Track last cleanup time
    time_t last_cleanup_time = time(NULL);

    while (running) {
        // Process SDL events
        if (check_events()) {
            running = 0;
            break;
        }

        // Run cleanup every 10 seconds
        time_t current_time = time(NULL);
        if (difftime(current_time, last_cleanup_time) >= 10.0) {
            cleanup_disconnected_drones();
            last_cleanup_time = current_time;
        }

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Draw elements - grid, drones, and survivors
        draw_grid();
        draw_survivors();
        draw_drones();

        // Update simulation statistics (used by both controller and view)
        update_simulation_stats();

        // Draw the info panel (will use the updated stats)
        draw_info_panel();

        // Print statistics every 50 frames
        if (frame_count % 50 == 0) {
            printf("Stats: Waiting: %d, Being Helped: %d, Rescued: %d, Drones: Idle=%d, On Mission=%d, Total=%d\n",
                   waiting_count, helped_count, rescued_count, idle_drones, mission_drones, num_drones);
        }

        // Present the frame
        SDL_RenderPresent(renderer);

        // Delay for frame rate control
        SDL_Delay(100); // 10 FPS

        frame_count++;
    }

    // Cancel threads
    pthread_cancel(ai_thread);
    pthread_join(ai_thread, NULL);

    pthread_cancel(survivor_thread);
    pthread_join(survivor_thread, NULL);

    pthread_cancel(drone_server_thread);
    pthread_join(drone_server_thread, NULL);

    // Cleanup
    cleanup_resources();
    cleanup_survivors();

    return 0;
}