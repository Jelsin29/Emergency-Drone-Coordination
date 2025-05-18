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
    (void)args;  // Unused parameter
    
    printf("Survivor generator started\n");
    
    // Wait a moment for the system to stabilize
    sleep(2);
    
    // Create survivors at several fixed positions first
    int fixed_positions[5][2] = {
        {5, 5},   // Top left
        {5, 25},  // Top right
        {20, 15}, // Center
        {35, 5},  // Bottom left
        {35, 25}  // Bottom right
    };
    
    printf("Creating 5 fixed test survivors\n");
    
    // Lock the mutex before modifying the survivor array
    pthread_mutex_lock(&survivors_mutex);
    
    // Create the fixed survivors
    for (int i = 0; i < 5 && num_survivors < MAX_SURVIVORS; i++) {
        int x = fixed_positions[i][0];
        int y = fixed_positions[i][1];
        
        // Initialize new survivor
        survivor_array[num_survivors].coord.x = x;
        survivor_array[num_survivors].coord.y = y;
        sprintf(survivor_array[num_survivors].info, "SURV-%d", num_survivors);
        survivor_array[num_survivors].status = 0; // Waiting for help
        
        // Set time
        time_t t;
        time(&t);
        localtime_r(&t, &survivor_array[num_survivors].discovery_time);
        
        printf("Created fixed survivor at (%d,%d)\n", x, y);
        
        // Move to next array slot
        num_survivors++;
    }
    
    // Unlock after modifying the array
    pthread_mutex_unlock(&survivors_mutex);
    
    printf("Fixed survivors created, entering random generation mode\n");
    
    // Seed random number generator
    srand(time(NULL));
    
    // Now randomly generate survivors periodically
    while (1) {
        // Wait between 5-10 seconds before generating a new survivor
        int delay = (rand() % 6) + 5;
        sleep(delay);
        
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
            
            printf("Created random survivor %d at (%d,%d)\n", 
                   num_survivors, x, y);
            
            // Move to next array slot
            num_survivors++;
        } else {
            printf("Survivor array full (%d survivors)\n", num_survivors);
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