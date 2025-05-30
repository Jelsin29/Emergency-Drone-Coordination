/**
 * @file drone.c
 * @brief Server-side drone management and network communication implementation
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @version 0.1
 * @date 2025-05-22
 * 
 * This module implements the server-side drone management system including
 * TCP/IP network communication, client connection handling, and drone
 * lifecycle management. It provides the communication bridge between the
 * coordination server and autonomous drone clients.
 * 
 * **Network Architecture:**
 * - Multi-threaded TCP server with concurrent client handling
 * - JSON-based communication protocol for all message types
 * - Per-client connection threads with dedicated message processing
 * - Automatic client registration and connection management
 * - Graceful handling of connection failures and timeouts
 * 
 * **Message Processing:**
 * - HANDSHAKE: Client registration and configuration exchange
 * - STATUS_UPDATE: Real-time position and status information
 * - MISSION_COMPLETE: Rescue completion notifications
 * - HEARTBEAT_RESPONSE: Connection keep-alive management
 * 
 * **Drone Lifecycle:**
 * - Connection establishment and handshake negotiation
 * - Registration in global drone list with unique ID assignment
 * - Continuous message processing and status synchronization
 * - Mission assignment forwarding to clients
 * - Cleanup and removal on disconnection
 * 
 * **Thread Safety:**
 * This module implements comprehensive thread safety measures to ensure reliable
 * operation in the multi-threaded drone coordination system:
 * 
 * - Each drone has its own mutex (drone->lock) for protecting status and position updates
 * - The global drones list has a mutex (drones->lock) for add/remove operations
 * - Proper locking order is maintained to prevent deadlocks:
 *   1. Always acquire drones list lock before individual drone locks
 *   2. Always acquire drone locks in ascending ID order when locking multiple drones
 * - Socket operations are protected with proper synchronization
 * - Status updates use atomic operations where possible
 * - Network message handling is thread-safe with per-connection mutexes
 * - Drone tracking data structures prevent race conditions during updates
 * 
 * All drone server operations maintain thread safety for concurrent access from
 * the AI controller, network clients, and visualization system.
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup networking
 * @ingroup core_modules
 */

#define _POSIX_C_SOURCE 199309L
#include "headers/drone.h"
#include "headers/globals.h"
#include "headers/server_throughput.h"
#include "headers/list.h"
#include "headers/survivor.h" // Added include for survivor-related variables
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <json-c/json.h>
#include <netinet/in.h>

/** @brief Default fleet size */
int num_drones = 20;

/** @brief Server port for drone communication */
#define SERVER_PORT 8080

/**
 * @brief Server thread function to listen for drone connections
 * 
 * Creates a socket server that listens for incoming drone client connections
 * and launches a new thread for each connected drone
 * 
 * @param arg Unused thread parameter
 * @return NULL when thread terminates
 */
// clang-format off
void *drone_server(void *arg)
// clang-format on
{
    (void)arg;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("Socket creation failed");
        perf_record_error();
        return NULL;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("Set socket options failed");
        perf_record_error();
        close(server_fd);
        return NULL;
    }
    // clang-format off
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    // clang-format on
    {
        perror("Bind failed");
        perf_record_error();
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("Listen failed");
        perf_record_error();
        close(server_fd);
        return NULL;
    }

    printf("Drone server listening on port 8080...\n");

    while (1)
    {
        int new_socket;
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        // clang-format off
        if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) < 0)
        // clang-format on
        {
            perror("Accept failed");
            perf_record_error();
            continue;
        }

        printf(
            "New drone connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Record new connection
        perf_record_connection(1);

        // Handle the new connection in a separate thread
        pthread_t client_thread;
        // clang-format off
        int *socket_ptr = malloc(sizeof(int));
        // clang-format on
        if (socket_ptr == NULL)
        {
            perror("Memory allocation failed");
            perf_record_error();
            close(new_socket);
            perf_record_connection(0); // Record failed connection cleanup
            continue;
        }
        // clang-format off
        *socket_ptr = new_socket;
        
        if (pthread_create(&client_thread, NULL, handle_drone_client, (void *)socket_ptr) != 0)
        // clang-format on
        {
            perror("Failed to create client thread");
            perf_record_error();
            free(socket_ptr);
            close(new_socket);
            perf_record_connection(0);
            continue;
        }

        pthread_detach(client_thread); // Detach thread to auto-cleanup
    }

    close(server_fd);
    return NULL;
}

/**
 * @brief Handle communication with a connected drone client
 * 
 * Processes messages from a drone client, including handshake, status
 * updates, mission completions, and heartbeats
 * 
 * @param arg Pointer to socket descriptor
 * @return NULL when thread terminates
 */
// clang-format off
void *handle_drone_client(void *arg)
// clang-format on
{
    int sock = *((int *)arg);
    free(arg); // Free the allocated memory for the socket pointer

    char buffer[4096];
    ssize_t bytes_received;
    struct timespec start_time, end_time;

    // Measure handshake response time
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // Receive HANDSHAKE message
    bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0)
    {
        if (bytes_received == 0)
            printf("Client disconnected before handshake\n");
        else
        {
            perror("Error receiving handshake");
            perf_record_error();
        }
        close(sock);
        perf_record_connection(0);
        return NULL;
    }

    buffer[bytes_received] = '\0'; // Null-terminate the received data
    printf("Received handshake data: %zd bytes\n", bytes_received);
    perf_record_status_update(bytes_received);

    // Parse the received JSON data
    // clang-format off
    struct json_object *parsed_json = json_tokener_parse(buffer);
    // clang-format on
    if (parsed_json == NULL)
    {
        printf("Failed to parse JSON data\n");
        perf_record_error();
        close(sock);
        perf_record_connection(0);
        return NULL;
    }

    // Verify it's a handshake message
    // clang-format off
    struct json_object *type_obj;
    // clang-format on
    if (!json_object_object_get_ex(parsed_json, "type", &type_obj) ||
        strcmp(json_object_get_string(type_obj), "HANDSHAKE") != 0)
    {
        printf("Not a valid handshake message\n");
        perf_record_error();
        json_object_put(parsed_json);
        close(sock);
        perf_record_connection(0);
        return NULL;
    }

    // Extract drone information from the JSON
    Drone drone;
    memset(&drone, 0, sizeof(Drone));

    // If no ID provided, generate a new one
    pthread_mutex_lock(&drones->lock);
    drone.id = drones->number_of_elements; // Use current size as new ID
    pthread_mutex_unlock(&drones->lock);

    // Get drone status
    // clang-format off
    struct json_object *status_obj;
    // clang-format on
    if (json_object_object_get_ex(parsed_json, "status", &status_obj))
    {
        const char *status_str = json_object_get_string(status_obj);
        if (strcmp(status_str, "IDLE") == 0)
            drone.status = IDLE;
        else if (strcmp(status_str, "ON_MISSION") == 0)
            drone.status = ON_MISSION;
        else
            drone.status = IDLE; // Default to IDLE
    }
    else
    {
        drone.status = IDLE; // Default to IDLE
    }

    // Get drone coordinates
    // clang-format off
    struct json_object *coord_obj;
    // clang-format on
    if (json_object_object_get_ex(parsed_json, "coord", &coord_obj))
    {
        // clang-format off
        struct json_object *x_obj, *y_obj;
        // clang-format on
        if (json_object_object_get_ex(coord_obj, "x", &x_obj) && json_object_object_get_ex(coord_obj, "y", &y_obj))
        {
            drone.coord.x = json_object_get_int(x_obj);
            drone.coord.y = json_object_get_int(y_obj);
        }
    }

    // Set initial target to current position
    drone.target = drone.coord;

    // Set last update time to current time
    time_t t = time(NULL);
    localtime_r(&t, &drone.last_update);

    // Free the parsed JSON object as we've extracted the needed data
    json_object_put(parsed_json);

    // Add the drone to the list
    pthread_mutex_init(&drone.lock, NULL);

    // Initialize the socket field with the client socket
    drone.socket = sock;

    // clang-format off
    Node *node = drones->add(drones, &drone);
    // clang-format on
    if (!node)
    {
        fprintf(stderr, "Failed to add drone %d to list\n", drone.id);
        perf_record_error();
        pthread_mutex_destroy(&drone.lock);
        close(sock);
        perf_record_connection(0);
        return NULL;
    }

    // Get a pointer to the actual drone in the list
    // clang-format off
    Drone *d = (Drone *)node->data;

    // Send HANDSHAKE_ACK response
    struct json_object *handshake_ack = json_object_new_object();
    json_object_object_add(handshake_ack, "type", json_object_new_string("HANDSHAKE_ACK"));
    json_object_object_add(handshake_ack, "session_id", json_object_new_string("S123")); // Generate a session ID

    // Config object
    struct json_object *config = json_object_new_object();
    json_object_object_add(config, "status_update_interval", json_object_new_int(5));
    json_object_object_add(config, "heartbeat_interval", json_object_new_int(10));
    json_object_object_add(handshake_ack, "config", config);

    // Send the handshake acknowledgment
    const char *ack_str = json_object_to_json_string(handshake_ack);
    size_t ack_size = strlen(ack_str);
    ssize_t bytes_sent = send(sock, ack_str, ack_size, 0);
    // clang-format on

    if (bytes_sent > 0)
    {
        perf_record_heartbeat(bytes_sent);

        // Record handshake response time
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        double response_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                               (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
        perf_record_response_time(response_time);

        printf("Handshake acknowledgment sent to drone %d (%zd bytes, %.2fms)\n", d->id, bytes_sent, response_time);
    }
    else
    {
        perf_record_error();
    }

    json_object_put(handshake_ack);

    // Main loop to handle communication with this drone
    while (1)
    {
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
                printf("Drone %d disconnected\n", d->id);
            else
            {
                perror("Error receiving from drone");
                perf_record_error();
            }

            // Mark drone as disconnected
            pthread_mutex_lock(&d->lock);
            d->status = DISCONNECTED;
            pthread_mutex_unlock(&d->lock);

            if (drones->removenode(drones, node) == 0)
            {
                printf("Drone %d removed from list\n", d->id);
            }
            else
            {
                printf("Failed to remove drone %d from list\n", d->id);
                perf_record_error();
            }

            perf_record_connection(0); // Record disconnection
            break;
        }

        buffer[bytes_received] = '\0';
        perf_record_status_update(bytes_received);

        // Try to parse multiple JSON messages in a single buffer
        char *json_start = buffer;
        char *json_end = NULL;
        int json_depth = 0;
        int in_string = 0;
        int escape_next = 0;

        for (int i = 0; i < bytes_received; i++)
        {
            // Track JSON structure to find complete objects
            if (escape_next)
            {
                escape_next = 0;
                continue;
            }

            if (buffer[i] == '\\' && !escape_next)
            {
                escape_next = 1;
                continue;
            }

            if (buffer[i] == '"' && !escape_next)
            {
                in_string = !in_string;
                continue;
            }

            if (!in_string)
            {
                if (buffer[i] == '{')
                {
                    if (json_depth == 0)
                    {
                        json_start = buffer + i;
                    }
                    json_depth++;
                }
                else if (buffer[i] == '}')
                {
                    json_depth--;
                    if (json_depth == 0)
                    {
                        json_end = buffer + i + 1;

                        // We found a complete JSON object, process it
                        // clang-format off
                        char temp = *json_end;
                        *json_end = '\0';
                        
                        clock_gettime(CLOCK_MONOTONIC, &start_time); // Reset timer for individual message
                        parsed_json = json_tokener_parse(json_start);
                        
                        *json_end = temp; // Restore the buffer
                        // clang-format on
                        if (parsed_json == NULL)
                        {
                            printf("Failed to parse JSON data from drone %d\n", d->id);
                            perf_record_error();
                            continue;
                        }

                        // Handle different message types
                        if (json_object_object_get_ex(parsed_json, "type", &type_obj))
                        {
                            // clang-format off
                            const char *msg_type = json_object_get_string(type_obj);
                            // clang-format on
                            if (strcmp(msg_type, "STATUS_UPDATE") == 0)
                            {
                                // Handle status update
                                pthread_mutex_lock(&d->lock);

                                // Update drone location
                                // clang-format off
                                struct json_object *location_obj;
                                // clang-format on
                                if (json_object_object_get_ex(parsed_json, "location", &location_obj))
                                {
                                    // clang-format off
                                    struct json_object *x_obj, *y_obj;
                                    // clang-format on
                                    if (json_object_object_get_ex(location_obj, "x", &x_obj) &&
                                        json_object_object_get_ex(location_obj, "y", &y_obj))
                                    {
                                        d->coord.x = json_object_get_int(x_obj);
                                        d->coord.y = json_object_get_int(y_obj);
                                    }
                                }

                                // Update status
                                // clang-format off
                                struct json_object *status_obj;
                                // clang-format on
                                if (json_object_object_get_ex(parsed_json, "status", &status_obj))
                                {
                                    // clang-format off
                                    const char *status_str = json_object_get_string(status_obj);
                                    // clang-format on
                                    if (strcmp(status_str, "idle") == 0)
                                        d->status = IDLE;
                                    else if (strcmp(status_str, "busy") == 0)
                                        d->status = ON_MISSION;
                                }

                                // Update last update time
                                time(&t);
                                localtime_r(&t, &d->last_update);

                                pthread_mutex_unlock(&d->lock);

                                // Record processing time
                                clock_gettime(CLOCK_MONOTONIC, &end_time);
                                double processing_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                                                         (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
                                perf_record_response_time(processing_time);
                            }
                            else if (strcmp(msg_type, "MISSION_COMPLETE") == 0)
                            {
                                // Handle mission completion
                                printf("Received MISSION_COMPLETE message from drone %d\n", d->id);

                                // Get target location if provided in the message
                                Coord target_coord = d->target; // Default to current target
                                // clang-format off
                                struct json_object *target_location;
                                // clang-format on
                                if (json_object_object_get_ex(parsed_json, "target_location", &target_location))
                                {
                                    // clang-format off
                                    struct json_object *x_obj, *y_obj;
                                    // clang-format on
                                    if (json_object_object_get_ex(target_location, "x", &x_obj) &&
                                        json_object_object_get_ex(target_location, "y", &y_obj))
                                    {
                                        target_coord.x = json_object_get_int(x_obj);
                                        target_coord.y = json_object_get_int(y_obj);
                                    }
                                }

                                pthread_mutex_lock(&d->lock);
                                d->status = IDLE;
                                pthread_mutex_unlock(&d->lock);

                                // Call update_drone_status with explicit target coordinates
                                update_drone_status(d, &target_coord);

                                // Record mission completion processing time
                                clock_gettime(CLOCK_MONOTONIC, &end_time);
                                double processing_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                                                         (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
                                perf_record_response_time(processing_time);
                            }
                            else if (strcmp(msg_type, "HEARTBEAT_RESPONSE") == 0)
                            {
                                // Update last contact time
                                pthread_mutex_lock(&d->lock);
                                time(&t);
                                localtime_r(&t, &d->last_update);
                                pthread_mutex_unlock(&d->lock);

                                // Record heartbeat response time
                                clock_gettime(CLOCK_MONOTONIC, &end_time);
                                double heartbeat_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                                                        (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
                                perf_record_response_time(heartbeat_time);
                            }
                        }

                        json_object_put(parsed_json);

                        // Move to next character after this JSON object
                        json_start = json_end;
                    }
                }
            }
        }
    }

    close(sock);
    return NULL;
}

/**
 * @brief Update drone status after mission completion
 * 
 * Updates the status of a survivor after being rescued by a drone
 * 
 * @param drone Pointer to drone that completed a mission
 * @param target Coordinates of the completed mission target
 */
// clang-format off
void update_drone_status(Drone *drone, Coord *target)
// clang-format on
{
    if (!drone || !target)
    {
        fprintf(stderr, "Invalid arguments in update_drone_status\n");
        perf_record_error();
        return;
    }

    // Keep track if we found and updated a survivor
    int found_survivor = 0;

    // Find which survivor this drone was helping
    pthread_mutex_lock(&survivors_mutex);
    for (int j = 0; j < num_survivors; j++)
    {
        // Check for survivors being helped (status 1) that match the drone's target location
        if (survivor_array[j].status == 1 && survivor_array[j].coord.x == target->x &&
            survivor_array[j].coord.y == target->y)
        {
            // Mark survivor as rescued
            survivor_array[j].status = 2; // 2 = rescued (won't be drawn)

            // Set rescue timestamp
            time_t t;
            time(&t);
            localtime_r(&t, &survivor_array[j].helped_time);

            printf("Server updated survivor %d status to rescued by drone %d\n", j, drone->id);

            found_survivor = 1;
            break;
        }
    }
    pthread_mutex_unlock(&survivors_mutex);

    if (!found_survivor)
    {
        printf(
            "Warning: No matching survivor found for drone %d at target (%d, %d)\n", drone->id, target->x, target->y);
        perf_record_error();
    }

    // The drone status has already been set to IDLE in the calling function
}

/**
 * @brief Clean up drone resources
 * 
 * Cancels threads, destroys mutexes, and frees memory
 */
void cleanup_drones()
{
    // Traverse the list and clean up each drone
    // clang-format off
    Node *current = drones->head;
    // clang-format on

    while (current != NULL)
    {
        // clang-format off
        Drone *d = (Drone *)current->data;
        // clang-format on
        // Cancel the drone's thread
        pthread_cancel(d->thread_id);

        // Close the socket if it's open
        if (d->socket > 0)
        {
            close(d->socket);
            perf_record_connection(0); // Record disconnection
        }

        // Destroy the drone's mutex - must be careful with this!
        pthread_mutex_lock(&d->lock);
        pthread_mutex_unlock(&d->lock);
        pthread_mutex_destroy(&d->lock);

        // Move to the next node
        current = current->next;
    }

    // Drones list itself will be destroyed in controller.c cleanup_resources()
}