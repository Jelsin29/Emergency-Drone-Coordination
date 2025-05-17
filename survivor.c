#include "headers/survivor.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "headers/globals.h"
#include "headers/map.h"

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

void *survivor_generator(void *args) {
    (void)args;  // Unused parameter
    
    printf("Survivor generator started\n");
    
    // Wait a moment for the system to stabilize
    sleep(2);
    
    // Seed random number generator
    srand(time(NULL) + 1000);
    
    // Create survivors at several fixed positions first
    int fixed_positions[5][2] = {
        {5, 5},   // Top left
        {5, 25},  // Top right
        {20, 15}, // Center
        {35, 5},  // Bottom left
        {35, 25}  // Bottom right
    };
    
    printf("Creating 5 fixed test survivors\n");
    
    for (int i = 0; i < 5; i++) {
        int x = fixed_positions[i][0];
        int y = fixed_positions[i][1];
        
        // Create survivor
        Survivor *s = malloc(sizeof(Survivor));
        if (!s) {
            fprintf(stderr, "Failed to allocate memory for survivor\n");
            continue;
        }
        
        // Initialize fields
        memset(s, 0, sizeof(Survivor));
        s->coord.x = x;
        s->coord.y = y;
        snprintf(s->info, sizeof(s->info) - 1, "TEST-%d", i);
        s->status = 0;  // Waiting for help
        
        // Set time
        time_t t;
        time(&t);
        localtime_r(&t, &s->discovery_time);
        
        printf("Creating fixed survivor at (%d,%d)\n", x, y);
        
        // Add to global list
        pthread_mutex_lock(&survivors->lock);
        survivors->add(survivors, s);
        printf("Added survivor to global list, count: %d\n", 
               survivors->number_of_elements);
        pthread_mutex_unlock(&survivors->lock);
        
        // Verify coordinates are within map bounds
        if (x < 0 || x >= map.height || y < 0 || y >= map.width) {
            printf("ERROR: Survivor coordinates (%d,%d) are outside map bounds (%d,%d)!\n", 
                  x, y, map.height, map.width);
            continue;
        }
        
        // Add to map cell WITH DEBUGGING ADDED
        printf("CRITICAL DEBUG: About to add survivor to map cell (%d,%d)\n", x, y);
        printf("CRITICAL DEBUG: Map height=%d, width=%d\n", map.height, map.width);
        printf("CRITICAL DEBUG: Map cell survivors list at (%d,%d) is %p\n", 
               x, y, (void*)map.cells[x][y].survivors);
        
        if (map.cells[x][y].survivors == NULL) {
            printf("ERROR: Survivors list at map cell (%d,%d) is NULL!\n", x, y);
        } else {
            pthread_mutex_lock(&map.cells[x][y].survivors->lock);
            Node *node = map.cells[x][y].survivors->add(map.cells[x][y].survivors, s);
            int cell_count = map.cells[x][y].survivors->number_of_elements;
            pthread_mutex_unlock(&map.cells[x][y].survivors->lock);
            
            if (node == NULL) {
                printf("ERROR: Failed to add survivor to map cell (%d,%d)!\n", x, y);
            } else {
                printf("SUCCESS: Added survivor to map cell (%d,%d), count now: %d\n", 
                      x, y, cell_count);
            }
        }
        
        // Short delay between survivors
        sleep(1);
    }
    
    printf("Fixed survivors created, entering random generation mode\n");
    
    // Then generate random survivors periodically
    while (1) {
        // Random coordinates within map bounds
        int x = rand() % map.height;
        int y = rand() % map.width;
        
        // Create survivor
        Survivor *s = malloc(sizeof(Survivor));
        if (!s) {
            fprintf(stderr, "Failed to allocate memory for survivor\n");
            sleep(3);
            continue;
        }
        
        // Initialize fields
        memset(s, 0, sizeof(Survivor));
        s->coord.x = x;
        s->coord.y = y;
        snprintf(s->info, sizeof(s->info) - 1, "SURV-%04d", rand() % 10000);
        s->status = 0;  // Waiting for help
        
        // Set time
        time_t t;
        time(&t);
        localtime_r(&t, &s->discovery_time);
        
        printf("Creating random survivor at (%d,%d)\n", x, y);
        
        // Add to global list
        pthread_mutex_lock(&survivors->lock);
        survivors->add(survivors, s);
        printf("Total survivors: %d\n", survivors->number_of_elements);
        pthread_mutex_unlock(&survivors->lock);
        
        // Add to map cell
        pthread_mutex_lock(&map.cells[x][y].survivors->lock);
        Node *node = map.cells[x][y].survivors->add(map.cells[x][y].survivors, s);
        int cell_count = map.cells[x][y].survivors->number_of_elements;
        pthread_mutex_unlock(&map.cells[x][y].survivors->lock);
        
        if (node == NULL) {
            printf("ERROR: Failed to add random survivor to map cell (%d,%d)!\n", x, y);
        } else {
            printf("SUCCESS: Added random survivor to map cell (%d,%d), count now: %d\n", 
                  x, y, cell_count);
        }
        
        // Longer delay between random survivors
        int delay = rand() % 5 + 5;  // 5-10 second delay
        sleep(delay);
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