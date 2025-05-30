/**
 * @file ai.c
 * @brief Implementation of intelligent mission assignment algorithms
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @version 0.1
 * @date 2025-05-22
 * 
 * This module implements the core artificial intelligence algorithms for
 * the emergency drone coordination system. It provides multiple strategies
 * for optimizing drone-survivor assignments, including distance-based
 * optimization, real-time mission tracking, and performance monitoring.
 * 
 * **Key Algorithms:**
 * - Survivor-centric assignment: Optimize wait times for people in need
 * - Drone-centric assignment: Maximize drone utilization efficiency
 * - Manhattan distance calculations for grid-based pathfinding
 * - Real-time mission completion detection and status management
 * 
 * **Performance Features:**
 * - Sub-millisecond response times for mission assignments
 * - Comprehensive throughput monitoring and statistics
 * - Thread-safe coordination with all system components
 * - JSON-based network protocol for remote drone communication
 * 
 * @copyright Copyright (c) 2025
 * 
 * @ingroup ai_algorithms
 * @ingroup core_modules
 */

#define _POSIX_C_SOURCE 199309L
#include "headers/ai.h"
#include "headers/server_throughput.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <json-c/json.h>
#include <sys/socket.h>

/**
 * @brief Calculate Manhattan distance between two coordinates
 * 
 * Computes the sum of absolute differences in x and y coordinates
 * 
 * @param a First coordinate
 * @param b Second coordinate
 * @return Manhattan distance (|x1-x2| + |y1-y2|)
 */
int calculate_distance(Coord a, Coord b)
{
    return abs(a.x - b.x) + abs(a.y - b.y);
}

/**
 * @brief Assign a mission to a drone to rescue a specific survivor
 * 
 * Updates drone and survivor status and sends mission assignment
 * to networked drone clients
 * 
 * @param drone Pointer to the drone to assign
 * @param survivor_index Index of the survivor to rescue
 */
// clang-format off
void assign_mission(Drone *drone, int survivor_index)
// clang-format on
{
    if (!drone || survivor_index < 0 || survivor_index >= num_survivors)
    {
        fprintf(stderr, "Invalid drone or survivor index in assign_mission\n");
        perf_record_error();
        return;
    }

    // Measure mission assignment response time
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

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
            // clang-format off
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
            size_t mission_size = strlen(mission_str);
            ssize_t bytes_sent = send(drone->socket, mission_str, mission_size, 0);
            // clang-format on

            if (bytes_sent > 0)
            {
                perf_record_mission_assigned(bytes_sent);

                // Record mission assignment response time
                clock_gettime(CLOCK_MONOTONIC, &end_time);
                double response_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                                       (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
                perf_record_response_time(response_time);

                printf("Mission assigned to drone %d for survivor %d (%zd bytes, %.2fms)\n",
                       drone->id,
                       survivor_index,
                       bytes_sent,
                       response_time);
            }
            else
            {
                perror("Failed to send mission assignment");
                perf_record_error();

                // Rollback the status changes if sending failed
                drone->status = IDLE;
                survivor_array[survivor_index].status = 0;
            }

            // Free the JSON object
            json_object_put(mission);
        }
        else
        {
            // For local drones (not networked), just record the assignment
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            double response_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                                   (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
            perf_record_response_time(response_time);

            printf("Local mission assigned to drone %d for survivor %d (%.2fms)\n",
                   drone->id,
                   survivor_index,
                   response_time);
        }
    }
    else
    {
        // Mission assignment failed - record as error
        perf_record_error();
        printf("Failed to assign mission: drone %d status=%d, survivor %d status=%d\n",
               drone->id,
               drone->status,
               survivor_index,
               survivor_index < num_survivors ? survivor_array[survivor_index].status : -1);
    }

    // Unlock mutexes
    pthread_mutex_unlock(&survivors_mutex);
    pthread_mutex_unlock(&drone->lock);
}

/**
 * @brief Find the closest idle drone to a specific survivor
 * 
 * Searches through all drones to find the closest one that is idle
 * 
 * @param survivor_index Index of the survivor
 * @return Pointer to the closest idle drone, or NULL if none available
 */
// clang-format off
Drone *find_closest_idle_drone(int survivor_index)
// clang-format on
{
    if (survivor_index < 0 || survivor_index >= num_survivors)
    {
        fprintf(stderr, "Invalid survivor index in find_closest_idle_drone\n");
        perf_record_error();
        return NULL;
    }
    //clang-format on
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
    // clang-format on
    {
        //clang-format off
        Drone *d = (Drone *)current->data;
        //clang-format on
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
 * @brief Find the closest waiting survivor to a specific drone
 * 
 * Searches through all survivors to find the closest one waiting for help
 * 
 * @param drone Pointer to the drone
 * @return Index of the closest waiting survivor, or -1 if none available
 */
// clang-format off
int find_closest_waiting_survivor(Drone *drone)
// clang-format on
{
    if (!drone)
    {
        fprintf(stderr, "Invalid drone pointer in find_closest_waiting_survivor\n");
        perf_record_error();
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
 * @brief Alternative AI controller function - loops through drones instead of survivors
 * 
 * For each idle drone, finds the closest survivor and assigns a mission
 * More efficient than the survivor-centric approach for large numbers of survivors
 * 
 * @param args Unused parameter
 * @return NULL when thread terminates
 */
// clang-format off
void *drone_centric_ai_controller(void *args)
// clang-format on
{
    (void)args; // Unused parameter

    // Give the system time to initialize
    sleep(3);

    printf("Starting drone-centric AI controller with throughput monitoring...\n");

    // Debug: Count how many survivors and drones we have at the start
    pthread_mutex_lock(&drones->lock);
    int initial_drone_count = drones->number_of_elements;
    pthread_mutex_unlock(&drones->lock);

    pthread_mutex_lock(&survivors_mutex);
    int initial_survivor_count = num_survivors;
    pthread_mutex_unlock(&survivors_mutex);

    printf("AI Controller: Initial count - Drones: %d, Survivors: %d\n", initial_drone_count, initial_survivor_count);

    int ai_cycle_count = 0;

    while (1)
    {
        ai_cycle_count++;
        int missions_assigned = 0;

        // Measure AI processing time every 10 cycles
        struct timespec ai_start, ai_end;
        if (ai_cycle_count % 10 == 0)
        {
            clock_gettime(CLOCK_MONOTONIC, &ai_start);
        }

        // Debug: Count survivors that are waiting for help
        int waiting_survivors = 0;
        pthread_mutex_lock(&survivors_mutex);
        for (int i = 0; i < num_survivors; i++)
        {
            if (survivor_array[i].status == 0)
            {
                waiting_survivors++;
            }
        }
        pthread_mutex_unlock(&survivors_mutex);

        // Debug: Count idle drones
        int idle_drone_count = 0;
        pthread_mutex_lock(&drones->lock);
        Node *count_current = drones->head;
        while (count_current != NULL)
        {
            Drone *d = (Drone *)count_current->data;
            pthread_mutex_lock(&d->lock);
            if (d->status == IDLE)
            {
                idle_drone_count++;
            }
            pthread_mutex_unlock(&d->lock);
            count_current = count_current->next;
        }

        // Only print debug info if there are both idle drones and waiting survivors
        if (idle_drone_count > 0 && waiting_survivors > 0)
        {
            printf(
                "AI Controller: Found %d idle drones and %d waiting survivors\n", idle_drone_count, waiting_survivors);
        }
        pthread_mutex_unlock(&drones->lock);

        // First phase: For each idle drone, find the closest survivor and assign a mission
        pthread_mutex_lock(&drones->lock);
        // clang-format off
        Node *current = drones->head;
        // clang-format on
        while (current != NULL)
        {
            // clang-format off
            Drone *d = (Drone *)current->data;
            // // clang-format on
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

        // Record AI processing performance every 10 cycles
        if (ai_cycle_count % 10 == 0) {
            clock_gettime(CLOCK_MONOTONIC, &ai_end);
            double ai_processing_time = (ai_end.tv_sec - ai_start.tv_sec) * 1000.0 + 
                                       (ai_end.tv_nsec - ai_start.tv_nsec) / 1000000.0;
            perf_record_response_time(ai_processing_time);
            
            if (missions_assigned > 0 || (idle_drone_count > 0 && waiting_survivors > 0)) {
                printf("AI cycle %d: Assigned %d missions in %.2fms\n", 
                       ai_cycle_count, missions_assigned, ai_processing_time);
            }
        }

        // Sleep to avoid excessive CPU usage
        sleep(1);
    }

    return NULL;
}

/**
 * @brief Main AI controller function - loops through survivors instead of drones
 * 
 * For each waiting survivor, finds the closest idle drone and assigns a mission
 * Also checks for mission completions
 * 
 * @param args Unused parameter
 * @return NULL when thread terminates
 */
// clang-format off
void *ai_controller(void *args)
// clang-format on
{
    (void)args; // Unused parameter

    // Give the system time to initialize
    sleep(3);

    printf("Starting survivor-centric AI controller with throughput monitoring...\n");

    int ai_cycle_count = 0;

    while (1)
    {
        ai_cycle_count++;

        // Measure AI processing time
        struct timespec ai_start, ai_end;
        clock_gettime(CLOCK_MONOTONIC, &ai_start);

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
            // clang-format off
            Drone *drone = find_closest_idle_drone(i);
            // clang-format on
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
        // clang-format off
        Node *current = drones->head;
        // clang-format on
        while (current != NULL)
        {
            // clang-format off
            Drone *d = (Drone *)current->data;
            // clang-format on

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
                        if (survivor_array[j].status == 1 && survivor_array[j].coord.x == d->target.x &&
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

                            printf("AI detected mission completion: Drone %d rescued survivor %d\n", d->id, j);

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

        // Record AI processing time
        clock_gettime(CLOCK_MONOTONIC, &ai_end);
        double ai_processing_time = (ai_end.tv_sec - ai_start.tv_sec) * 1000.0 +
                                    (ai_end.tv_nsec - ai_start.tv_nsec) / 1000000.0;
        perf_record_response_time(ai_processing_time);

        // Log AI performance every 10 cycles
        if (ai_cycle_count % 10 == 0)
        {
            printf("AI cycle %d: Assigned %d missions, completed %d missions in %.2fms\n",
                   ai_cycle_count,
                   missions_assigned,
                   missions_completed,
                   ai_processing_time);
        }

        // Sleep to avoid excessive CPU usage
        sleep(1);
    }

    return NULL;
}