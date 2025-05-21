#include "headers/drone.h"
#include "headers/globals.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

int num_drones = 20; // Default fleet size

/**
 * Initialize the drone fleet
 * Creates drone objects and starts their threads
 */
void initialize_drones() {
    // Seed random number generator
    srand(time(NULL));

    // Initialize each drone and add it to the list
    for(int i = 0; i < num_drones; i++) {
        // Create a temporary drone object
        Drone drone;
        
        // Set basic properties
        drone.id = i;
        drone.status = IDLE;
        
        // Random starting position within map boundaries
        drone.coord.x = rand() % map.height;
        drone.coord.y = rand() % map.width;
        
        // Initial target is current position
        drone.target = drone.coord;
        
        // Initialize mutex
        pthread_mutex_init(&drone.lock, NULL);
        
        // Add the drone to the list - this copies the drone data into the list's memory
        // The add function handles its own locking
        Node *node = drones->add(drones, &drone);
        
        if (!node) {
            fprintf(stderr, "Failed to add drone %d to list\n", i);
            continue;
        }
        
        // Get a pointer to the actual drone in the list
        Drone *d = (Drone*)node->data;
        
        // Create thread for this drone
        int result = pthread_create(&d->thread_id, NULL, drone_behavior, d);
        if (result != 0) {
            fprintf(stderr, "Error creating thread for drone %d: %s\n", 
                   i, strerror(result));
        }
        
        // Small sleep to avoid overwhelming the system with thread creation
        usleep(10000); // 10ms delay between thread creation
    }
}

/**
 * Drone behavior function - runs in a separate thread for each drone
 * @param arg Pointer to the drone object this thread controls
 * @return NULL
 */
void* drone_behavior(void *arg) {
    Drone *d = (Drone*)arg;
    
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
            }
            
            // Update timestamp
            time_t t;
            time(&t);
            localtime_r(&t, &d->last_update);
        }
        
        pthread_mutex_unlock(&d->lock);
        
        // Sleep to control drone movement speed
        usleep(300000); // 300ms
    }
    
    return NULL;
}

/**
 * Clean up drone resources
 * Cancels threads, destroys mutexes, and frees memory
 */
void cleanup_drones() {
    // Traverse the list and clean up each drone
    // Note: We don't lock the list here since we're only reading
    //       and any modifications to the list should be done through
    //       the list's thread-safe interface
    Node *current = drones->head;
    
    while (current != NULL) {
        Drone *d = (Drone*)current->data;
        
        // Cancel the drone's thread
        pthread_cancel(d->thread_id);
        
        // Destroy the drone's mutex - must be careful with this!
        // Lock it first to ensure no other thread is using it
        pthread_mutex_lock(&d->lock);
        pthread_mutex_unlock(&d->lock);
        pthread_mutex_destroy(&d->lock);
        
        // Move to the next node
        current = current->next;
    }
    
    // Drones list itself will be destroyed in controller.c cleanup_resources()
}