#include "headers/drone.h"
#include "headers/globals.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

// Global drone fleet
Drone *drone_fleet = NULL;
int num_drones = 10; // Default fleet size

void initialize_drones() {
    printf("Initializing %d drones...\n", num_drones);
    
    // Allocate memory for drone fleet
    drone_fleet = malloc(sizeof(Drone) * num_drones);
    if (!drone_fleet) {
        perror("Failed to allocate drone fleet");
        exit(EXIT_FAILURE);
    }
    
    // Seed random number generator
    srand(time(NULL));

    // Initialize each drone
    for(int i = 0; i < num_drones; i++) {
        printf("Initializing drone %d...\n", i);
        
        // Set basic properties
        drone_fleet[i].id = i;
        drone_fleet[i].status = IDLE;
        
        // Random starting position within map boundaries
        drone_fleet[i].coord.x = rand() % map.height;
        drone_fleet[i].coord.y = rand() % map.width;
        
        // Initial target is current position
        drone_fleet[i].target = drone_fleet[i].coord;
        
        // Initialize mutex
        pthread_mutex_init(&drone_fleet[i].lock, NULL);
        
        printf("Drone %d initialized at (%d,%d)\n", 
               i, drone_fleet[i].coord.x, drone_fleet[i].coord.y);
    }
    
    printf("All drones initialized, SKIPPING adding to global list\n");
    
    // Start threads
    for(int i = 0; i < num_drones; i++) {
        int result = pthread_create(&drone_fleet[i].thread_id, NULL, drone_behavior, &drone_fleet[i]);
        if (result != 0) {
            fprintf(stderr, "Error creating thread for drone %d: %s\n", 
                   i, strerror(result));
        } else {
            printf("Drone %d: Thread started\n", i);
        }
        
        // Small sleep to avoid overwhelming the system with thread creation
        usleep(10000); // 10ms delay between thread creation
    }
    
    printf("All drone threads started\n");
}

void* drone_behavior(void *arg) {
    Drone *d = (Drone*)arg;
    
    printf("Drone %d: Thread starting behavior with status %d\n", d->id, d->status);
    
    // Make this thread cancelable
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    
    while(1) {
        // Keep the lock for the minimum time possible
        pthread_mutex_lock(&d->lock);
        
        DroneStatus status = d->status;
        Coord current = d->coord;
        Coord target = d->target;
        
        // Only update coordinates if on mission
        if(status == ON_MISSION) {
            // Calculate new position (move one step in each iteration)
            Coord new_pos = current;
            
            // Move in X direction
            if(new_pos.x < target.x) new_pos.x++;
            else if(new_pos.x > target.x) new_pos.x--;
            
            // Move in Y direction
            if(new_pos.y < target.y) new_pos.y++;
            else if(new_pos.y > target.y) new_pos.y--;
            
            // Check if position actually changed
            if(new_pos.x != current.x || new_pos.y != current.y) {
                // Update position
                d->coord = new_pos;
                
                printf("Drone %d: Moving from (%d,%d) to (%d,%d), target: (%d,%d)\n", 
                       d->id, current.x, current.y, 
                       new_pos.x, new_pos.y, 
                       target.x, target.y);
            }
            
            // Update timestamp
            time_t t;
            time(&t);
            localtime_r(&t, &d->last_update);
        }
        
        pthread_mutex_unlock(&d->lock);
        
        // Sleep to control drone movement speed (shorter for more responsive movement)
        usleep(300000); // 300ms
    }
    
    return NULL;
}

void cleanup_drones() {
    printf("Cleaning up drones...\n");
    
    if (!drone_fleet) {
        return; // Nothing to clean up
    }
    
    for(int i = 0; i < num_drones; i++) {
        printf("Cleaning up drone %d...\n", i);
        
        // Cancel thread
        pthread_cancel(drone_fleet[i].thread_id);
        
        // Cleanup mutex
        pthread_mutex_destroy(&drone_fleet[i].lock);
    }
    
    // Free memory
    free(drone_fleet);
    drone_fleet = NULL;
    
    printf("Drone cleanup complete\n");
}