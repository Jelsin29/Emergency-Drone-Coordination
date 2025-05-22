#include "headers/ai.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <json-c/json.h> // Add this include for json_object_put and other JSON functions
#include <sys/socket.h>  // Add this for socket functions like send()

/**
 * Calculate Manhattan distance between two coordinates
 * @param a First coordinate
 * @param b Second coordinate
 * @return Manhattan distance (|x1-x2| + |y1-y2|)
 */
int calculate_distance(Coord a, Coord b)
{
    return abs(a.x - b.x) + abs(a.y - b.y);
}

/**
 * Assign a mission to a drone to rescue a specific survivor
 * @param drone Pointer to the drone to assign
 * @param survivor_index Index of the survivor to rescue
 */
void assign_mission(Drone *drone, int survivor_index)
{
    if (!drone || survivor_index < 0 || survivor_index >= num_survivors)
    {
        fprintf(stderr, "Invalid drone or survivor index in assign_mission\n");
        return;
    }

    // Lock both drone and survivor mutex to prevent race conditions
    pthread_mutex_lock(&drone->lock);
    pthread_mutex_lock(&survivors_mutex);

    // Only proceed if the survivor still needs help and drone is idle
    if (survivor_array[survivor_index].status == 0 && drone->status == IDLE)
    {
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

        // Check if this is a networked drone client
        if (drone->socket > 0)
        {
            // Create a mission assignment message according to the protocol
            struct json_object *mission = json_object_new_object();
            json_object_object_add(mission, "type", json_object_new_string("ASSIGN_MISSION"));

            // Generate a unique mission ID
            char mission_id[16];
            snprintf(mission_id, sizeof(mission_id), "M%d", survivor_index);
            json_object_object_add(mission, "mission_id", json_object_new_string(mission_id));

            // Set priority (based on distance or other factors)
            json_object_object_add(mission, "priority", json_object_new_string("high"));

            // Target coordinates
            struct json_object *target = json_object_new_object();
            json_object_object_add(target, "x", json_object_new_int(survivor_array[survivor_index].coord.x));
            json_object_object_add(target, "y", json_object_new_int(survivor_array[survivor_index].coord.y));
            json_object_object_add(mission, "target", target);

            // Set expiry time (one hour from now)
            json_object_object_add(mission, "expiry", json_object_new_int(time(NULL) + 3600));

            // Send the mission to the drone client
            const char *mission_str = json_object_to_json_string(mission);
            ssize_t bytes_sent = send(drone->socket, mission_str, strlen(mission_str), 0);

            if (bytes_sent < 0)
            {
                perror("Failed to send mission assignment");
            }

            // Free the JSON object
            json_object_put(mission);
        }
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
Drone *find_closest_idle_drone(int survivor_index)
{
    if (survivor_index < 0 || survivor_index >= num_survivors)
    {
        fprintf(stderr, "Invalid survivor index in find_closest_idle_drone\n");
        return NULL;
    }

    Drone *closest_drone = NULL;
    int min_distance = INT_MAX;

    pthread_mutex_lock(&survivors_mutex);
    Coord survivor_pos = survivor_array[survivor_index].coord;
    pthread_mutex_unlock(&survivors_mutex);

    // Lock the drones list for iteration
    pthread_mutex_lock(&drones->lock);

    // Iterate through all drones in the list to find the closest idle one
    Node *current = drones->head;
    while (current != NULL)
    {
        Drone *d = (Drone *)current->data;

        // Lock this specific drone to check its status
        pthread_mutex_lock(&d->lock);

        if (d->status == IDLE)
        {
            int dist = calculate_distance(d->coord, survivor_pos);
            if (dist < min_distance)
            {
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
 * Find the closest waiting survivor to a specific drone
 * @param drone Pointer to the drone
 * @return Index of the closest waiting survivor, or -1 if none available
 */
int find_closest_waiting_survivor(Drone *drone)
{
    if (!drone)
    {
        fprintf(stderr, "Invalid drone pointer in find_closest_waiting_survivor\n");
        return -1;
    }

    int closest_survivor_index = -1;
    int min_distance = INT_MAX;

    // Lock the drone to get its current position
    pthread_mutex_lock(&drone->lock);
    Coord drone_pos = drone->coord;
    pthread_mutex_unlock(&drone->lock);

    // Lock the survivors mutex for iteration
    pthread_mutex_lock(&survivors_mutex);

    // Iterate through all survivors to find the closest waiting one
    for (int i = 0; i < num_survivors; i++)
    {
        // Only consider survivors who are waiting for help (status 0)
        if (survivor_array[i].status == 0)
        {
            Coord survivor_pos = survivor_array[i].coord;
            int dist = calculate_distance(drone_pos, survivor_pos);

            if (dist < min_distance)
            {
                
                min_distance = dist;
                closest_survivor_index = i;
            }
        }
    }

    pthread_mutex_unlock(&survivors_mutex);

    return closest_survivor_index;
}

/**
 * Alternative AI controller function - loops through drones instead of survivors
 * Assigns the closest survivor to each idle drone
 * @param args Unused parameter
 * @return NULL
 */
void *drone_centric_ai_controller(void *args)
{
    (void)args; // Unused parameter

    // Give the system time to initialize
    sleep(3);

    printf("Starting drone-centric AI controller...\n");
    
    // Debug: Count how many survivors and drones we have at the start
    pthread_mutex_lock(&drones->lock);
    int initial_drone_count = drones->number_of_elements;
    pthread_mutex_unlock(&drones->lock);
    
    pthread_mutex_lock(&survivors_mutex);
    int initial_survivor_count = num_survivors;
    pthread_mutex_unlock(&survivors_mutex);
    
    printf("AI Controller: Initial count - Drones: %d, Survivors: %d\n", 
           initial_drone_count, initial_survivor_count);

    while (1)
    {
        int missions_assigned = 0;
        
        // Debug: Count survivors that are waiting for help
        int waiting_survivors = 0;
        pthread_mutex_lock(&survivors_mutex);
        for (int i = 0; i < num_survivors; i++) {
            if (survivor_array[i].status == 0) {
                waiting_survivors++;
            }
        }
        pthread_mutex_unlock(&survivors_mutex);
        
        // Debug: Count idle drones
        int idle_drone_count = 0;
        pthread_mutex_lock(&drones->lock);
        Node *count_current = drones->head;
        while (count_current != NULL) {
            Drone *d = (Drone *)count_current->data;
            pthread_mutex_lock(&d->lock);
            if (d->status == IDLE) {
                idle_drone_count++;
            }
            pthread_mutex_unlock(&d->lock);
            count_current = count_current->next;
        }
        
        // Only print debug info if there are both idle drones and waiting survivors
        if (idle_drone_count > 0 && waiting_survivors > 0) {
            printf("AI Controller: Found %d idle drones and %d waiting survivors\n", 
                  idle_drone_count, waiting_survivors);
        }
        pthread_mutex_unlock(&drones->lock);

        // First phase: For each idle drone, find the closest survivor and assign a mission
        pthread_mutex_lock(&drones->lock);

        Node *current = drones->head;
        while (current != NULL)
        {
            Drone *d = (Drone *)current->data;

            // Lock this specific drone to check its status
            pthread_mutex_lock(&d->lock);

            // Only consider idle drones
            if (d->status == IDLE)
            {
                // Unlock drone before searching for survivor to avoid deadlocks
                pthread_mutex_unlock(&d->lock);

                // Find the closest waiting survivor
                int survivor_index = find_closest_waiting_survivor(d);

                // If a waiting survivor was found, assign the drone to help
                if (survivor_index >= 0)
                {
                    assign_mission(d, survivor_index);
                    missions_assigned++;
                    printf("Drone %d assigned to closest survivor %d\n", d->id, survivor_index);
                }
            }
            else
            {
                pthread_mutex_unlock(&d->lock);
            }

            current = current->next;
        }

        pthread_mutex_unlock(&drones->lock);

        // Sleep to avoid excessive CPU usage
        sleep(1);
    }

    return NULL;
}

/**
 * Main AI controller function - runs in a separate thread
 * Manages mission assignment and completion
 * @param args Unused parameter
 * @return NULL
 */
void *ai_controller(void *args)
{
    (void)args; // Unused parameter

    // Give the system time to initialize
    sleep(3);

    while (1)
    {
        // Scan through all survivors to find those waiting for help
        pthread_mutex_lock(&survivors_mutex);
        int current_num_survivors = num_survivors;
        pthread_mutex_unlock(&survivors_mutex);

        int missions_assigned = 0;

        // First phase: Assign missions to idle drones
        for (int i = 0; i < current_num_survivors; i++)
        {
            pthread_mutex_lock(&survivors_mutex);

            // Skip survivors that are already being helped or rescued
            if (survivor_array[i].status != 0)
            {
                pthread_mutex_unlock(&survivors_mutex);
                continue;
            }
            pthread_mutex_unlock(&survivors_mutex);

            // Find the closest idle drone
            Drone *drone = find_closest_idle_drone(i);

            // If an idle drone was found, assign it to help this survivor
            if (drone != NULL)
            {
                assign_mission(drone, i);
                missions_assigned++;
            }
        }

        // Second phase: Check for mission completions
        int missions_completed = 0;

        // Lock the drones list for iteration
        pthread_mutex_lock(&drones->lock);

        // Iterate through all drones in the list
        Node *current = drones->head;
        while (current != NULL)
        {
            Drone *d = (Drone *)current->data;

            // Lock this specific drone to check its status
            pthread_mutex_lock(&d->lock);

            // If drone is on mission, check if it reached its target
            if (d->status == ON_MISSION)
            {
                if (d->coord.x == d->target.x && d->coord.y == d->target.y)
                {

                    // Find which survivor this drone was helping
                    pthread_mutex_lock(&survivors_mutex);
                    for (int j = 0; j < num_survivors; j++)
                    {
                        if (survivor_array[j].status == 1 &&
                            survivor_array[j].coord.x == d->target.x &&
                            survivor_array[j].coord.y == d->target.y)
                        {

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