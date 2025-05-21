#include "headers/drone.h"
#include "headers/globals.h"
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

int num_drones = 20; // Default fleet size
#define SERVER_PORT 8080

void *drone_server(void *arg)
{
    (void)arg;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("Socket creation failed");
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
        close(server_fd);
        return NULL;
    }

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        return NULL;
    }
    if (listen(server_fd, 3) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        return NULL;
    }
    printf("Drone server listening on port 8080...\n");
    while (1)
    {
        int new_socket;
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) < 0)
        {
            perror("Accept failed");
            continue;
        }
        printf("New drone connection accepted\n");

        // Handle the new connection in a separate thread
        pthread_t client_thread;
        int *socket_ptr = malloc(sizeof(int));
        if (socket_ptr == NULL)
        {
            perror("Memory allocation failed");
            close(new_socket);
            continue;
        }
        *socket_ptr = new_socket;
        pthread_create(&client_thread, NULL, handle_drone_client, (void *)socket_ptr);
        pthread_detach(client_thread); // Detach thread to auto-cleanup
    }
    close(server_fd);
    return NULL;
}

/**
 * Handle communication with a connected drone client
 * @param arg Pointer to socket descriptor
 * @return NULL
 */
void *handle_drone_client(void *arg)
{
    int sock = *((int *)arg);
    free(arg); // Free the allocated memory for the socket pointer

    char buffer[4096];
    ssize_t bytes_received;

    // Receive HANDSHAKE message
    bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0)
    {
        if (bytes_received == 0)
            printf("Client disconnected before handshake\n");
        else
            perror("Error receiving handshake");
        close(sock);
        return NULL;
    }

    buffer[bytes_received] = '\0'; // Null-terminate the received data
    printf("Received data: %s\n", buffer);

    // Parse the received JSON data
    struct json_object *parsed_json = json_tokener_parse(buffer);
    if (parsed_json == NULL)
    {
        printf("Failed to parse JSON data\n");
        close(sock);
        return NULL;
    }

    // Verify it's a handshake message
    struct json_object *type_obj;
    if (!json_object_object_get_ex(parsed_json, "type", &type_obj) ||
        strcmp(json_object_get_string(type_obj), "HANDSHAKE") != 0)
    {
        printf("Not a valid handshake message\n");
        json_object_put(parsed_json);
        close(sock);
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
    struct json_object *status_obj;
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
    struct json_object *coord_obj;
    if (json_object_object_get_ex(parsed_json, "coord", &coord_obj))
    {
        struct json_object *x_obj, *y_obj;
        if (json_object_object_get_ex(coord_obj, "x", &x_obj) &&
            json_object_object_get_ex(coord_obj, "y", &y_obj))
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
    
    Node *node = drones->add(drones, &drone);

    if (!node)
    {
        fprintf(stderr, "Failed to add drone %d to list\n", drone.id);
        pthread_mutex_destroy(&drone.lock);
        close(sock);
        return NULL;
    }

    // Get a pointer to the actual drone in the list
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
    send(sock, ack_str, strlen(ack_str), 0);
    json_object_put(handshake_ack);

    printf("Handshake acknowledgment sent to drone %d\n", d->id);

    // Main loop to handle communication with this drone
    while (1)
    {
        bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
                printf("Drone %d disconnected\n", d->id);
            else
                perror("Error receiving from drone");

            // Mark drone as disconnected
            pthread_mutex_lock(&d->lock);
            d->status = DISCONNECTED;
            pthread_mutex_unlock(&d->lock);

            if(drones->removenode(drones, node) == 0)
            {
                printf("Drone %d removed from list\n", d->id);
            }
            else
            {
                printf("Failed to remove drone %d from list\n", d->id);
            }

            break;
        }

        buffer[bytes_received] = '\0';
        printf("Received from drone %d: %s\n", d->id, buffer);

        // Parse the received message
        parsed_json = json_tokener_parse(buffer);
        if (parsed_json == NULL)
        {
            printf("Failed to parse JSON data from drone %d\n", d->id);
            continue;
        }

        // Handle different message types (STATUS_UPDATE, MISSION_COMPLETE, etc.)
        if (json_object_object_get_ex(parsed_json, "type", &type_obj))
        {
            const char *msg_type = json_object_get_string(type_obj);

            if (strcmp(msg_type, "STATUS_UPDATE") == 0)
            {
                // Handle status update
                pthread_mutex_lock(&d->lock);

                // Update drone location
                struct json_object *location_obj;
                if (json_object_object_get_ex(parsed_json, "location", &location_obj))
                {
                    struct json_object *x_obj, *y_obj;
                    if (json_object_object_get_ex(location_obj, "x", &x_obj) &&
                        json_object_object_get_ex(location_obj, "y", &y_obj))
                    {
                        d->coord.x = json_object_get_int(x_obj);
                        d->coord.y = json_object_get_int(y_obj);
                    }
                }

                // Update status
                struct json_object *status_obj;
                if (json_object_object_get_ex(parsed_json, "status", &status_obj))
                {
                    const char *status_str = json_object_get_string(status_obj);
                    if (strcmp(status_str, "idle") == 0)
                        d->status = IDLE;
                    else if (strcmp(status_str, "busy") == 0)
                        d->status = ON_MISSION;
                }

                // Update last update time
                time(&t);
                localtime_r(&t, &d->last_update);

                pthread_mutex_unlock(&d->lock);
            }
            else if (strcmp(msg_type, "MISSION_COMPLETE") == 0)
            {
                // Handle mission completion
                pthread_mutex_lock(&d->lock);
                d->status = IDLE;
                pthread_mutex_unlock(&d->lock);

                printf("Drone %d completed mission\n", d->id);
            }
            else if (strcmp(msg_type, "HEARTBEAT_RESPONSE") == 0)
            {
                // Update last contact time
                pthread_mutex_lock(&d->lock);
                time(&t);
                localtime_r(&t, &d->last_update);
                pthread_mutex_unlock(&d->lock);
            }
        }

        json_object_put(parsed_json);
    }

    close(sock);
    return NULL;
}

/**
 * Clean up drone resources
 * Cancels threads, destroys mutexes, and frees memory
 */
void cleanup_drones()
{
    // Traverse the list and clean up each drone
    // Note: We don't lock the list here since we're only reading
    //       and any modifications to the list should be done through
    //       the list's thread-safe interface
    Node *current = drones->head;

    while (current != NULL)
    {
        Drone *d = (Drone *)current->data;

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