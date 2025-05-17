#include "headers/ai.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

void assign_mission(Drone *drone, Coord target) {
    printf("Assigning mission to drone %d: target (%d,%d)\n", 
           drone->id, target.x, target.y);
           
    pthread_mutex_lock(&drone->lock);
    drone->target = target;
    drone->status = ON_MISSION;
    time_t t;
    time(&t);
    localtime_r(&t, &drone->last_update);
    pthread_mutex_unlock(&drone->lock);
    
    printf("Mission assigned to drone %d\n", drone->id);
}

// Calculate Manhattan distance between two coordinates
int calculate_distance(Coord a, Coord b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}

Drone *find_closest_idle_drone(Coord target) {
    Drone *closest = NULL;
    int min_distance = INT_MAX;
    
    // Iterate through drone_fleet directly instead of using the list
    for (int i = 0; i < num_drones; i++) {
        pthread_mutex_lock(&drone_fleet[i].lock);
        
        if (drone_fleet[i].status == IDLE) {
            int dist = calculate_distance(drone_fleet[i].coord, target);
            if (dist < min_distance) {
                min_distance = dist;
                closest = &drone_fleet[i];
            }
        }
        
        pthread_mutex_unlock(&drone_fleet[i].lock);
    }
    
    return closest;
}

void *ai_controller(void *arg) {
    (void)arg; // Unused parameter
    
    printf("AI Controller started\n");
    
    // Give the system a moment to finish initialization
    sleep(2);
    
    while (1) {
        // Check if there are any survivors waiting
        pthread_mutex_lock(&survivors->lock);
        int survivor_count = survivors->number_of_elements;
        
        if (survivor_count > 0) {
            // Get the first survivor from the list
            Survivor *s = NULL;
            if (survivors->head != NULL) {
                s = malloc(sizeof(Survivor));  // Allocate new memory for popped survivor
                if (s == NULL) {
                    pthread_mutex_unlock(&survivors->lock);
                    fprintf(stderr, "Failed to allocate memory for survivor\n");
                    sleep(1);
                    continue;
                }
                
                // Pop the survivor from the list into our new memory
                if (survivors->pop(survivors, s) == NULL) {
                    pthread_mutex_unlock(&survivors->lock);
                    free(s);
                    fprintf(stderr, "Failed to pop survivor from list\n");
                    sleep(1);
                    continue;
                }
                
                pthread_mutex_unlock(&survivors->lock);
                
                printf("Processing survivor %s at (%d,%d)\n", 
                       s->info, s->coord.x, s->coord.y);
                
                // Find closest idle drone
                Drone *closest = find_closest_idle_drone(s->coord);
                
                if (closest != NULL) {
                    // Assign mission
                    assign_mission(closest, s->coord);
                    
                    // Mark as being helped
                    s->status = 1;
                    time_t t;
                    time(&t);
                    localtime_r(&t, &s->helped_time);
                    
                    // Add to helped survivors list
                    pthread_mutex_lock(&helpedsurvivors->lock);
                    helpedsurvivors->add(helpedsurvivors, s);
                    pthread_mutex_unlock(&helpedsurvivors->lock);
                    
                    printf("Survivor %s being helped by Drone %d\n",
                           s->info, closest->id);
                } else {
                    // No drones available, put the survivor back in the list
                    printf("No idle drones available for survivor %s\n", s->info);
                    pthread_mutex_lock(&survivors->lock);
                    survivors->add(survivors, s);
                    pthread_mutex_unlock(&survivors->lock);
                }
            } else {
                pthread_mutex_unlock(&survivors->lock);
            }
        } else {
            pthread_mutex_unlock(&survivors->lock);
        }
        
        // Sleep to avoid excessive CPU usage
        usleep(200000); // 200ms
    }
    
    return NULL;
}