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
    
    printf("Survivor array initialized with capacity for %d survivors\n", MAX_SURVIVORS);
}

void cleanup_survivors() {
    pthread_mutex_destroy(&survivors_mutex);
    if (survivor_array) {
        free(survivor_array);
        survivor_array = NULL;
    }
    num_survivors = 0;
}

void *survivor_generator(void *args) {
    
    printf("Survivor generator started\n");
    
    // Wait a moment for the system to stabilize
    sleep(1);
    
    // Seed random number generator
    srand(time(NULL));
    
    printf("Beginning rapid survivor generation...\n");
    
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
            
            printf("Created initial survivor %d at (%d,%d)\n", 
                   num_survivors, x, y);
            
            // Move to next array slot
            num_survivors++;
        }
        
        pthread_mutex_unlock(&survivors_mutex);
        
        // Very small delay between initial survivors
        usleep(100000); // Just 0.1 seconds between spawns
    }
    
    printf("Initial batch created, continuing with rapid generation\n");
    
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
            
            printf("Created rapid survivor %d at (%d,%d)\n", 
                   num_survivors, x, y);
            
            // Move to next array slot
            num_survivors++;
        } else {
            printf("Survivor array full (%d survivors)\n", num_survivors);
            
            // If the array is full, we can reset some rescued survivors to make space
            // Optional: Recycle some rescued survivors
            int recycled = 0;
            for (int i = 0; i < num_survivors && recycled < 5; i++) {
                if (survivor_array[i].status == 2) { // If rescued
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
                    
                    printf("Recycled survivor %d at new position (%d,%d)\n", i, x, y);
                    recycled++;
                }
            }
        }
        
        // Unlock after modifying the array
        pthread_mutex_unlock(&survivors_mutex);
    }
    
    return NULL;
}

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