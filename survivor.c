/**
 * @file survivor.c
 * @brief Implementation of survivor management system for emergency response simulation
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @version 0.1
 * @date 2025-05-22
 * 
 * This module implements the complete survivor management system for the
 * emergency drone coordination application. It provides continuous survivor
 * generation, status tracking, and lifecycle management to create realistic
 * emergency scenarios for testing and demonstration.
 * 
 * **Survivor Lifecycle Management:**
 * - Continuous generation of new survivors at random locations
 * - Status tracking: waiting → being helped → rescued
 * - Thread-safe array management with mutex protection
 * - Automatic recycling when maximum capacity is reached
 * - Timestamp tracking for response time analysis
 * 
 * **Generation Algorithm:**
 * - Initial burst: 10 survivors at system startup
 * - Continuous generation: 1 survivor every 0.5-1.5 seconds
 * - Random location selection within map boundaries
 * - Intelligent recycling of rescued survivors when array is full
 * - Configurable generation rates and patterns
 * 
 * **Data Management:**
 * - Fixed-size array for predictable memory usage
 * - Thread-safe access through global mutex protection
 * - Integration with spatial map system for location tracking
 * - Efficient status updates and query operations
 * 
 * **Thread Safety:**
 * - Global survivor mutex protects all array operations
 * - Coordination with drone system for mission assignment
 * - Safe concurrent access from AI controller and visualization
 * - Proper cleanup and resource management
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup simulation
 * @ingroup core_modules
 */

#include "headers/survivor.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "headers/globals.h"
#include "headers/map.h"

// Global survivor array
Survivor *survivor_array = NULL;
int num_survivors = 0;
pthread_mutex_t survivors_mutex;

/**
 * @brief Create a new survivor with the given attributes
 * 
 * @param coord Location coordinates
 * @param info Information string
 * @param discovery_time Time of discovery
 * @return Pointer to the new survivor or NULL if allocation failed
 */
Survivor *create_survivor(Coord *coord, char *info,
                          struct tm *discovery_time) {
    Survivor *s = malloc(sizeof(Survivor));
    if (!s) return NULL;

    memset(s, 0, sizeof(Survivor));
    s->coord = *coord;
    memcpy(&s->discovery_time, discovery_time, sizeof(struct tm));
    strncpy(s->info, info, sizeof(s->info) - 1);
    s->info[sizeof(s->info) - 1] = '\0';  // Ensure null-termination
    s->status = 0;  // Initialize status (0 = waiting)
    return s;
}

/**
 * @brief Initialize the survivor system
 * 
 * Allocates survivor array and initializes mutex
 */
void initialize_survivors() {
    // Allocate memory for survivor array
    survivor_array = (Survivor*)malloc(sizeof(Survivor) * MAX_SURVIVORS);
    if (!survivor_array) {
        fprintf(stderr, "Failed to allocate memory for survivor array\n");
        exit(EXIT_FAILURE);
    }
    
    // Initialize values
    memset(survivor_array, 0, sizeof(Survivor) * MAX_SURVIVORS);
    num_survivors = 0;
    
    // Initialize mutex
    pthread_mutex_init(&survivors_mutex, NULL);
}

/**
 * @brief Cleanup survivor resources
 * 
 * Frees memory and destroys the mutex
 */
void cleanup_survivors() {
    pthread_mutex_destroy(&survivors_mutex);
    if (survivor_array) {
        free(survivor_array);
        survivor_array = NULL;
    }
    num_survivors = 0;
}

/**
 * @brief Survivor generator thread function
 * 
 * Continuously generates new survivors at random map locations
 * 
 * @param args Unused thread parameters
 * @return NULL
 */
void *survivor_generator(void *args) {
    (void)args;  // Explicitly mark parameter as unused to suppress warning
    
    // Wait a moment for the system to stabilize
    sleep(1);
    
    // Seed random number generator
    srand(time(NULL));
    
    // Initial batch of survivors (10 at once)
    for (int i = 0; i < 10; i++) {
        pthread_mutex_lock(&survivors_mutex);
        
        if (num_survivors < MAX_SURVIVORS) {
            // Generate random coordinates
            int x = rand() % map.height;
            int y = rand() % map.width;
            
            // Initialize new survivor
            survivor_array[num_survivors].coord.x = x;
            survivor_array[num_survivors].coord.y = y;
            sprintf(survivor_array[num_survivors].info, "SURV-%d", num_survivors);
            survivor_array[num_survivors].status = 0; // Waiting for help
            
            // Set time
            time_t t;
            time(&t);
            localtime_r(&t, &survivor_array[num_survivors].discovery_time);
            
            // Move to next array slot
            num_survivors++;
        }
        
        pthread_mutex_unlock(&survivors_mutex);
        
        // Very small delay between initial survivors
        usleep(100000); // Just 0.1 seconds between spawns
    }
    
    // Constant rapid generation
    while (1) {
        // Very short delay between spawns (0.5-1.5 seconds)
        int delay_ms = (rand() % 1000) + 500;
        usleep(delay_ms * 1000);
        
        // Lock the mutex before checking/modifying the array
        pthread_mutex_lock(&survivors_mutex);
        
        // Only generate a new survivor if there's space in the array
        if (num_survivors < MAX_SURVIVORS) {
            // Generate random coordinates
            int x = rand() % map.height;
            int y = rand() % map.width;
            
            // Initialize new survivor
            survivor_array[num_survivors].coord.x = x;
            survivor_array[num_survivors].coord.y = y;
            sprintf(survivor_array[num_survivors].info, "SURV-%d", num_survivors);
            survivor_array[num_survivors].status = 0; // Waiting for help
            
            // Set time
            time_t t;
            time(&t);
            localtime_r(&t, &survivor_array[num_survivors].discovery_time);
            
            // Move to next array slot
            num_survivors++;
        } else {
            //printf("Survivor array is full, recycling survivors...\n");
            // If the array is full, recycle some rescued survivors to make space
            int recycled = 0;
            for (int i = 0; i < num_survivors && recycled < 5; i++) {
                if (survivor_array[i].status >= 2) { // If rescued
                    // Generate new coordinates
                    int x = rand() % map.height;
                    int y = rand() % map.width;
                    
                    // Reset this survivor to a new location
                    survivor_array[i].coord.x = x;
                    survivor_array[i].coord.y = y;
                    survivor_array[i].status = 0; // Waiting for help again
                    
                    // Update time
                    time_t t;
                    time(&t);
                    localtime_r(&t, &survivor_array[i].discovery_time);
                    
                    recycled++;
                }
            }
        }
        
        // Unlock after modifying the array
        pthread_mutex_unlock(&survivors_mutex);
    }
    
    return NULL;
}

/**
 * @brief Clean up resources associated with a survivor
 * 
 * @param s Pointer to the survivor to clean up
 */
void survivor_cleanup(Survivor *s) {
    // Make sure coordinates are within bounds to avoid segfault
    if (s->coord.x >= 0 && s->coord.x < map.height && 
        s->coord.y >= 0 && s->coord.y < map.width) {
        
        // Remove from map cell
        pthread_mutex_lock(&map.cells[s->coord.x][s->coord.y].survivors->lock);
        map.cells[s->coord.x][s->coord.y].survivors->removedata(
            map.cells[s->coord.x][s->coord.y].survivors, s);
        pthread_mutex_unlock(&map.cells[s->coord.x][s->coord.y].survivors->lock);
    }

    // Remove from global lists
    pthread_mutex_lock(&survivors->lock);
    survivors->removedata(survivors, s);
    pthread_mutex_unlock(&survivors->lock);
    
    pthread_mutex_lock(&helpedsurvivors->lock);
    helpedsurvivors->removedata(helpedsurvivors, s);
    pthread_mutex_unlock(&helpedsurvivors->lock);

    free(s);
}