/**
 * @file controller.c
 * @brief Main system controller and coordination hub for emergency drone system
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @version 0.1
 * @date 2025-05-22
 * 
 * This module serves as the central coordination hub for the entire emergency
 * drone coordination system. It initializes all subsystems, manages the main
 * simulation loop, coordinates between different components, and provides
 * system-wide statistics and monitoring.
 * 
 * **System Architecture:**
 * - Multi-threaded architecture with dedicated threads for each subsystem
 * - Real-time SDL-based visualization with interactive display
 * - TCP/IP server for drone client connections
 * - Continuous survivor generation for realistic emergency simulation
 * - AI-driven mission assignment and optimization
 * 
 * **Thread Management:**
 * - Main thread: SDL rendering and event processing (10 FPS)
 * - Drone server thread: Network connection handling
 * - Survivor generator thread: Continuous emergency simulation
 * - AI controller thread: Mission assignment and optimization
 * - Performance monitor thread: Metrics collection and logging
 * 
 * **Performance Monitoring:**
 * - Real-time throughput tracking with CSV logging
 * - Comprehensive metrics export in JSON format
 * - Frame-based statistics updates for visualization
 * - Graceful shutdown with final performance reports
 * 
 * **System Lifecycle:**
 * 1. Initialize all subsystems (lists, map, SDL, networking)
 * 2. Start all service threads (server, AI, survivor generation)
 * 3. Run main simulation loop with real-time visualization
 * 4. Handle graceful shutdown with proper resource cleanup
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup core_modules
 * @ingroup main_controller
 */

#include "headers/globals.h"
#include "headers/map.h"
#include "headers/drone.h"
#include "headers/survivor.h"
#include "headers/ai.h"
#include "headers/list.h"
#include "headers/view.h"
#include "headers/server_throughput.h"
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
pthread_t drone_server_thread;
pthread_t ai_thread;

// Throughput monitoring thread
pthread_t throughput_monitor;

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
void initialize_lists()
{
    // Create lists with appropriate capacities
    survivors = create_list(sizeof(Survivor), 1000);
    if (!survivors)
    {
        fprintf(stderr, "Failed to create survivors list\n");
        perf_record_error();
        exit(EXIT_FAILURE);
    }

    helpedsurvivors = create_list(sizeof(Survivor), 1000);
    if (!helpedsurvivors)
    {
        fprintf(stderr, "Failed to create helpedsurvivors list\n");
        perf_record_error();
        exit(EXIT_FAILURE);
    }

    drones = create_list(sizeof(Drone), 100);
    if (!drones)
    {
        fprintf(stderr, "Failed to create drones list\n");
        perf_record_error();
        exit(EXIT_FAILURE);
    }
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
void update_simulation_stats()
{
    // Reset counters
    waiting_count = 0;
    helped_count = 0;
    idle_drones = 0;
    mission_drones = 0;
    
    // Count survivors by status
    pthread_mutex_lock(&survivors_mutex);
    for (int i = 0; i < num_survivors; i++)
    {
        if (survivor_array[i].status == 0)
        {
            waiting_count++;
        }
        else if (survivor_array[i].status == 1)
        {
            helped_count++;
        }
        else if (survivor_array[i].status == 2)
        {
            survivor_array[i].status = 3;
            rescued_count++;
        }
    }
    pthread_mutex_unlock(&survivors_mutex);
    
    // Count drones by status from the list
    pthread_mutex_lock(&drones->lock);
    
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
        
        pthread_mutex_unlock(&d->lock);
        current = current->next;
    }
    
    pthread_mutex_unlock(&drones->lock);
}

/**
 * Main function - entry point for the drone coordination system
 */
int main()
{
    printf("Emergency Drone Coordination System - Phase 1\n");
    printf("---------------------------------------------\n");

    // Initialize performance monitoring with CSV logging
    throughput_monitor = start_perf_monitor("drone_server_metrics.csv");
    if (throughput_monitor == 0) {
        fprintf(stderr, "Failed to start performance monitoring\n");
        return 1;
    }

    // Set up signal handler for Ctrl+C
    signal(SIGINT, handle_signal);

    // Initialize global lists
    initialize_lists();

    // Initialize map (40x30 grid)
    init_map(30, 40);

    // Initialize survivor array
    initialize_survivors();

    // Initialize SDL window
    if (init_sdl_window() != 0)
    {
        fprintf(stderr, "Failed to initialize SDL window\n");
        perf_record_error();
        cleanup_resources();
        cleanup_survivors();
        export_metrics_json("error_final_drone_metrics.json");
        stop_perf_monitor(throughput_monitor);
        return 1;
    }

    // Draw initial grid
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    draw_grid();
    SDL_RenderPresent(renderer);

    // Start drone server thread
    int result = pthread_create(&drone_server_thread, NULL, drone_server, NULL);
    if (result != 0)
    {
        fprintf(stderr, "Error creating drone server thread: %d\n", result);
        perf_record_error();
        cleanup_resources();
        cleanup_survivors();
        export_metrics_json("error_final_drone_metrics.json");
        stop_perf_monitor(throughput_monitor);
        return 1;
    }

    // Start survivor generator thread
    int survivor_result = pthread_create(&survivor_thread, NULL, survivor_generator, NULL);
    if (survivor_result != 0)
    {
        fprintf(stderr, "Error creating survivor thread: %d\n", survivor_result);
        perf_record_error();
        cleanup_resources();
        cleanup_survivors();
        export_metrics_json("error_final_drone_metrics.json");
        stop_perf_monitor(throughput_monitor);
        return 1;
    }

    // Start AI controller thread
    int ai_result = pthread_create(&ai_thread, NULL, drone_centric_ai_controller, NULL);
    if (ai_result != 0)
    {
        fprintf(stderr, "Error creating AI controller thread: %d\n", ai_result);
        perf_record_error();
        cleanup_resources();
        cleanup_survivors();
        export_metrics_json("error_final_drone_metrics.json");
        stop_perf_monitor(throughput_monitor);
        return 1;
    }

    // Start main rendering loop
    int frame_count = 0;
    running = 1;

    printf("Main simulation loop started - monitoring server throughput...\n");

    while (running)
    {
        // Process SDL events
        if (check_events())
        {
            running = 0;
            break;
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

        // Print statistics every 50 frames (with throughput info)
        if (frame_count % 50 == 0) {
            printf("Stats: Waiting: %d, Being Helped: %d, Rescued: %d, Drones: Idle=%d, On Mission=%d\n",
                   waiting_count, helped_count, rescued_count, idle_drones, mission_drones);
            
            // Log current performance metrics every 100 frames
            if (frame_count % 100 == 0) {
                log_perf_metrics();
            }
        }

        // Present the frame
        SDL_RenderPresent(renderer);

        // Delay for frame rate control
        SDL_Delay(100); // 10 FPS

        frame_count++;
    }

    printf("Shutting down system - finalizing performance metrics...\n");

    // Cancel threads
    pthread_cancel(ai_thread);
    pthread_join(ai_thread, NULL);

    pthread_cancel(survivor_thread);
    pthread_join(survivor_thread, NULL);

    // Cleanup
    cleanup_resources();
    cleanup_survivors();

    // Export final performance metrics
    printf("Exporting final performance metrics...\n");
    export_metrics_json("final_drone_metrics.json");
    stop_perf_monitor(throughput_monitor);

    printf("System shutdown complete.\n");
    return 0;
}