#include "headers/ai.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

/**
 * Calculate Manhattan distance between two coordinates
 * @param a First coordinate
 * @param b Second coordinate
 * @return Manhattan distance (|x1-x2| + |y1-y2|)
 */
int calculate_distance(Coord a, Coord b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}

/**
 * Assign a mission to a drone to rescue a specific survivor
 * @param drone Pointer to the drone to assign
 * @param survivor_index Index of the survivor to rescue
 */
void assign_mission(Drone *drone, int survivor_index) {
    if (!drone || survivor_index < 0 || survivor_index >= num_survivors) {
        fprintf(stderr, "Invalid drone or survivor index in assign_mission\n");
        return;
    }
    
    // Lock both drone and survivor mutex to prevent race conditions
    pthread_mutex_lock(&drone->lock);
    pthread_mutex_lock(&survivors_mutex);
    
    // Only proceed if the survivor still needs help and drone is idle
    if (survivor_array[survivor_index].status == 0 && drone->status == IDLE) {
        // Set drone target to survivor position
        drone->target.x = survivor_array[survivor_index].coord.x;
        drone->target.y = survivor_array[survivor_index].coord.y;
        
        // Update drone status
        drone->status = ON_MISSION;
        
        // Update survivor status to "being helped"
        survivor_array[survivor_index].status = 1;
        
        // Set timestamp
        time_t t;
        time(&t);
        localtime_r(&t, &drone->last_update);
    }
    
    // Unlock mutexes
    pthread_mutex_unlock(&survivors_mutex);
    pthread_mutex_unlock(&drone->lock);
}

/**
 * Find the closest idle drone to a specific survivor
 * @param survivor_index Index of the survivor
 * @return Pointer to the closest idle drone, or NULL if none available
 */
Drone* find_closest_idle_drone(int survivor_index) {
    if (survivor_index < 0 || survivor_index >= num_survivors) {
        fprintf(stderr, "Invalid survivor index in find_closest_idle_drone\n");
        return NULL;
    }
    
    Drone* closest_drone = NULL;
    int min_distance = INT_MAX;
    
    pthread_mutex_lock(&survivors_mutex);
    Coord survivor_pos = survivor_array[survivor_index].coord;
    pthread_mutex_unlock(&survivors_mutex);
    
    // Lock the drones list for iteration
    pthread_mutex_lock(&drones->lock);
    
    // Iterate through all drones in the list to find the closest idle one
    Node* current = drones->head;
    while (current != NULL) {
        Drone* d = (Drone*)current->data;
        
        // Lock this specific drone to check its status
        pthread_mutex_lock(&d->lock);
        
        if (d->status == IDLE) {
            int dist = calculate_distance(d->coord, survivor_pos);
            if (dist < min_distance) {
                min_distance = dist;
                closest_drone = d;
            }
        }
        
        pthread_mutex_unlock(&d->lock);
        current = current->next;
    }
    
    pthread_mutex_unlock(&drones->lock);
    
    return closest_drone;
}

/**
 * Main AI controller function - runs in a separate thread
 * Manages mission assignment and completion
 * @param args Unused parameter
 * @return NULL
 */
void *ai_controller(void *args) {
    (void)args; // Unused parameter
    
    // Give the system time to initialize
    sleep(3);
    
    while (1) {
        // Scan through all survivors to find those waiting for help
        pthread_mutex_lock(&survivors_mutex);
        int current_num_survivors = num_survivors;
        pthread_mutex_unlock(&survivors_mutex);
        
        int missions_assigned = 0;
        
        // First phase: Assign missions to idle drones
        for (int i = 0; i < current_num_survivors; i++) {
            pthread_mutex_lock(&survivors_mutex);
            
            // Skip survivors that are already being helped or rescued
            if (survivor_array[i].status != 0) {
                pthread_mutex_unlock(&survivors_mutex);
                continue;
            }
            pthread_mutex_unlock(&survivors_mutex);
            
            // Find the closest idle drone
            Drone* drone = find_closest_idle_drone(i);
            
            // If an idle drone was found, assign it to help this survivor
            if (drone != NULL) {
                assign_mission(drone, i);
                missions_assigned++;
            }
        }
        
        // Second phase: Check for mission completions
        int missions_completed = 0;
        
        // Lock the drones list for iteration
        pthread_mutex_lock(&drones->lock);
        
        // Iterate through all drones in the list
        Node* current = drones->head;
        while (current != NULL) {
            Drone* d = (Drone*)current->data;
            
            // Lock this specific drone to check its status
            pthread_mutex_lock(&d->lock);
            
            // If drone is on mission, check if it reached its target
            if (d->status == ON_MISSION) {
                if (d->coord.x == d->target.x && d->coord.y == d->target.y) {
                    
                    // Find which survivor this drone was helping
                    pthread_mutex_lock(&survivors_mutex);
                    for (int j = 0; j < num_survivors; j++) {
                        if (survivor_array[j].status == 1 && 
                            survivor_array[j].coord.x == d->target.x &&
                            survivor_array[j].coord.y == d->target.y) {
                            
                            // Mark survivor as rescued
                            survivor_array[j].status = 2; // 2 = rescued (won't be drawn)
                            
                            // Set rescue timestamp
                            time_t t;
                            time(&t);
                            localtime_r(&t, &survivor_array[j].helped_time);
                            
                            missions_completed++;
                            
                            // Reset drone to idle
                            d->status = IDLE;
                            
                            break;
                        }
                    }
                    pthread_mutex_unlock(&survivors_mutex);
                }
            }
            
            pthread_mutex_unlock(&d->lock);
            current = current->next;
        }
        
        pthread_mutex_unlock(&drones->lock);
        
        // Sleep to avoid excessive CPU usage
        sleep(1);
    }
    
    return NULL;
}