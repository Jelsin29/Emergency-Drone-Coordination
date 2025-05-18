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
        exit(EXIT_FAILURE);
    }

    helpedsurvivors = create_list(sizeof(Survivor), 1000);
    if (!helpedsurvivors)
    {
        fprintf(stderr, "Failed to create helpedsurvivors list\n");
        exit(EXIT_FAILURE);
    }

    drones = create_list(sizeof(Drone), 100);
    if (!drones)
    {
        fprintf(stderr, "Failed to create drones list\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * Clean up all system resources
 */
void cleanup_resources()
{
    // Cancel threads
    if (drone_fleet != NULL)
    {
        for (int i = 0; i < num_drones; i++)
        {
            pthread_cancel(drone_fleet[i].thread_id);
            pthread_mutex_destroy(&drone_fleet[i].lock);
        }
        free(drone_fleet);
        drone_fleet = NULL;
    }

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
 * Main function - entry point for the drone coordination system
 */
int main()
{
    printf("Emergency Drone Coordination System - Phase 1\n");
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
    if (init_sdl_window() != 0)
    {
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

    // Initialize drones
    initialize_drones();

    // Start survivor generator thread
    int result = pthread_create(&survivor_thread, NULL, survivor_generator, NULL);
    if (result != 0)
    {
        fprintf(stderr, "Error creating survivor thread: %d\n", result);
        cleanup_resources();
        cleanup_survivors();
        return 1;
    }

    // Start AI controller thread
    int ai_result = pthread_create(&ai_thread, NULL, ai_controller, NULL);
    if (ai_result != 0)
    {
        fprintf(stderr, "Error creating AI controller thread: %d\n", ai_result);
        cleanup_resources();
        cleanup_survivors();
        return 1;
    }

    // Start main rendering loop
    int frame_count = 0;
    running = 1;
    int rescued_count = 0;

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

        if (drone_fleet != NULL)
        {
            draw_drones();
        }

        pthread_mutex_lock(&survivors_mutex);

        // Count survivors by status
        int waiting_count = 0;
        int helped_count = 0;

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

        char title[100];
        snprintf(title, sizeof(title),
                 "Drone Simulator | Waiting: %d | Being Helped: %d | Rescued: %d | Drones: %d",
                 waiting_count, helped_count, rescued_count, num_drones);
        SDL_SetWindowTitle(window, title);

        // Print statistics every 50 frames
        if (frame_count % 50 == 0){

            printf("Stats: Waiting: %d, Being Helped: %d, Rescued: %d, Drones: %d\n",
                waiting_count, helped_count, rescued_count, num_drones);

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

    // Cleanup
    cleanup_resources();
    cleanup_survivors();

    return 0;
}