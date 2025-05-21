#include <arpa/inet.h>
#include "headers/drone.h"
#include "headers/globals.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <json-c/json.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

Drone my_drone = {0};
pthread_t thread_id;
volatile int running = 1;

void* drone_behavior(void *arg) {
    int sock = *(int *)arg;

    while(running) {
        // Keep the lock for the minimum time possible
        pthread_mutex_lock(&my_drone.lock);
        
        // Only update coordinates if on mission
        if(my_drone.status == ON_MISSION) {
            // Calculate new position (move one step in each iteration)
            Coord new_pos = my_drone.coord;
            
            // Move in X direction
            if(new_pos.x < my_drone.target.x) new_pos.x++;
            else if(new_pos.x > my_drone.target.x) new_pos.x--;
            
            // Move in Y direction
            if(new_pos.y < my_drone.target.y) new_pos.y++;
            else if(new_pos.y > my_drone.target.y) new_pos.y--;
            
            // Check if position actually changed
            if(new_pos.x != my_drone.coord.x || new_pos.y != my_drone.coord.y) {
                // Update position
                my_drone.coord = new_pos;
            }
        }

        // Check if the drone has reached its target
        if (my_drone.status == ON_MISSION && my_drone.coord.x == my_drone.target.x && my_drone.coord.y == my_drone.target.y) {
            // Notify the server about mission completion
            struct json_object *mission_complete = json_object_new_object();
            json_object_object_add(mission_complete, "type", json_object_new_string("MISSION_COMPLETE"));
            json_object_object_add(mission_complete, "drone_id", json_object_new_int(my_drone.id));
            json_object_object_add(mission_complete, "timestamp", json_object_new_int(time(NULL)));
            json_object_object_add(mission_complete, "success", json_object_new_boolean(1));
            json_object_object_add(mission_complete, "details", json_object_new_string("Mission completed successfully."));

            const char *mission_complete_str = json_object_to_json_string(mission_complete);
            send(sock, mission_complete_str, strlen(mission_complete_str), 0);
            json_object_put(mission_complete);

            // Reset the drone's status to IDLE
            my_drone.status = IDLE;
        } else {
            // Update timestamp only if the mission is not yet complete
            time_t t;
            time(&t);
            localtime_r(&t, &my_drone.last_update);
        }
        
        pthread_mutex_unlock(&my_drone.lock);
        
        // Sleep to control drone movement speed
        usleep(300000); // 300ms
    }
    
    return NULL;
}

int main()
{
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to the rescue system server.\n");

    // Seed random number generator
    srand(time(NULL));

    // Initialize the map dimensions before using them
    map.height = 30; // Example height
    map.width = 40;  // Example width

    // Set basic properties
    // my_drone.id = id;
    my_drone.status = IDLE;

    // Random starting position within map boundaries
    my_drone.coord.x = rand() % map.height;
    my_drone.coord.y = rand() % map.width;

    // Initial target is current position
    my_drone.target = my_drone.coord;

    // Initialize mutex
    pthread_mutex_init(&my_drone.lock, NULL);

    // Create JSON object for the drone
    struct json_object *drone_info = json_object_new_object();
    json_object_object_add(drone_info, "type", json_object_new_string("HANDSHAKE"));
    json_object_object_add(drone_info, "drone_id", json_object_new_int(my_drone.id));
    json_object_object_add(drone_info, "status", json_object_new_string("IDLE"));
    struct json_object *coord = json_object_new_object();
    json_object_object_add(coord, "x", json_object_new_int(my_drone.coord.x));
    json_object_object_add(coord, "y", json_object_new_int(my_drone.coord.y));
    json_object_object_add(drone_info, "coord", coord);

    // Convert JSON object to string and send it
    const char *json_str = json_object_to_json_string(drone_info);
    send(sock, json_str, strlen(json_str), 0);
    printf("Drone info sent: %s\n", json_str);

    // Free JSON object
    json_object_put(drone_info);

    // Wait for HANDSHAKE_ACK from the server
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Server response: %s\n", buffer);
        // Parse the response to ensure it's a HANDSHAKE_ACK
        struct json_object *response = json_tokener_parse(buffer);
        struct json_object *type;
        if (json_object_object_get_ex(response, "type", &type) &&
            strcmp(json_object_get_string(type), "HANDSHAKE_ACK") == 0) {
            printf("Handshake acknowledged by server.\n");
        } else {
            fprintf(stderr, "Unexpected response from server. Exiting.\n");
            json_object_put(response);
            close(sock);
            exit(EXIT_FAILURE);
        }
        json_object_put(response);
    } else {
        perror("Failed to receive HANDSHAKE_ACK");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Start the drone behavior thread
    int result = pthread_create(&thread_id, NULL, &drone_behavior, &sock);
    if (result != 0) {
        fprintf(stderr, "Error creating thread %s\n", strerror(result));
    }

    // Main loop to handle server messages
    while (running) {
        bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("Message from server: %s\n", buffer);

            // Parse the server message
            struct json_object *message = json_tokener_parse(buffer);
            struct json_object *type;
            if (json_object_object_get_ex(message, "type", &type)) {
                const char *message_type = json_object_get_string(type);

                if (strcmp(message_type, "HEARTBEAT") == 0) {
                    // Respond to heartbeat
                    struct json_object *heartbeat_response = json_object_new_object();
                    json_object_object_add(heartbeat_response, "type", json_object_new_string("HEARTBEAT_RESPONSE"));
                    json_object_object_add(heartbeat_response, "drone_id", json_object_new_int(my_drone.id));
                    json_object_object_add(heartbeat_response, "timestamp", json_object_new_int(time(NULL)));
                    const char *response_str = json_object_to_json_string(heartbeat_response);
                    send(sock, response_str, strlen(response_str), 0);
                    json_object_put(heartbeat_response);
                } else if (strcmp(message_type, "ASSIGN_MISSION") == 0) {
                    // Handle mission assignment
                    struct json_object *target;
                    if (json_object_object_get_ex(message, "target", &target)) {
                        struct json_object *x, *y;
                        if (json_object_object_get_ex(target, "x", &x) &&
                            json_object_object_get_ex(target, "y", &y)) {
                            pthread_mutex_lock(&my_drone.lock);
                            my_drone.target.x = json_object_get_int(x);
                            my_drone.target.y = json_object_get_int(y);
                            my_drone.status = ON_MISSION;
                            pthread_mutex_unlock(&my_drone.lock);
                            printf("Mission assigned: Target (%d, %d)\n", my_drone.target.x, my_drone.target.y);
                        }
                    }
                }
            }
            json_object_put(message);
        } else if (bytes_received == 0) {
            printf("Server disconnected.\n");
            break;
        } else {
            perror("Error receiving message from server");
            break;
        }
    }

    // Wait for the thread to finish
    running = 0; // Signal the thread to exit
    pthread_join(thread_id, NULL);

    // Cleanup resources
    pthread_mutex_destroy(&my_drone.lock);
    close(sock);

    return 0;
}

