#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include "headers/drone.h"
#include "headers/globals.h"
#include "headers/map.h"
#include "headers/server_throughput.h"
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
pthread_t throughput_monitor;
volatile int running = 1;
int sock; // Global socket for both threads
pthread_mutex_t sock_mutex = PTHREAD_MUTEX_INITIALIZER; // Add socket mutex

void* drone_behavior(void *arg) {
    (void)arg; // Unused parameter

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
            
            // Debug print to track movement calculation
            printf("*** Movement calc: Current (%d,%d) Target (%d,%d) NewPos (%d,%d) Status=%d\n",
                  my_drone.coord.x, my_drone.coord.y, 
                  my_drone.target.x, my_drone.target.y,
                  new_pos.x, new_pos.y, my_drone.status);
            
            // Check if position actually changed
            if(new_pos.x != my_drone.coord.x || new_pos.y != my_drone.coord.y) {
                // Update position
                my_drone.coord = new_pos;
                
                // Measure response time for status update
                struct timespec start_time, end_time;
                clock_gettime(CLOCK_MONOTONIC, &start_time);
                
                // Send a STATUS_UPDATE message to the server
                struct json_object *status_update = json_object_new_object();
                json_object_object_add(status_update, "type", json_object_new_string("STATUS_UPDATE"));
                json_object_object_add(status_update, "drone_id", json_object_new_int(my_drone.id));
                json_object_object_add(status_update, "timestamp", json_object_new_int(time(NULL)));
                
                // Include current location
                struct json_object *location = json_object_new_object();
                json_object_object_add(location, "x", json_object_new_int(my_drone.coord.x));
                json_object_object_add(location, "y", json_object_new_int(my_drone.coord.y));
                json_object_object_add(status_update, "location", location);

                // Include current status
                json_object_object_add(status_update, "status", json_object_new_string(my_drone.status == IDLE ? "idle" : "busy"));
                json_object_object_add(status_update, "battery", json_object_new_int(100)); // Add battery level for better info
                
                // Send the update
                const char *update_str = json_object_to_json_string(status_update);
                size_t message_size = strlen(update_str);
                
                pthread_mutex_lock(&sock_mutex);
                ssize_t bytes_sent = send(sock, update_str, message_size, 0);
                // Send a newline character to separate JSON messages
                send(sock, "\n", 1, 0);
                pthread_mutex_unlock(&sock_mutex);
                
                // Record throughput metrics
                if (bytes_sent > 0) {
                    perf_record_status_update(bytes_sent);
                    
                    // Measure and record response time
                    clock_gettime(CLOCK_MONOTONIC, &end_time);
                    double response_time_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + 
                                             (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
                    perf_record_response_time(response_time_ms);
                } else {
                    perf_record_error();
                }
                
                // Free the JSON object
                json_object_put(status_update);
                
                printf("Status update sent: Position (%d, %d) - %zd bytes\n", my_drone.coord.x, my_drone.coord.y, bytes_sent);
            }
        }

        // Check if the drone has reached its target
        if (my_drone.status == ON_MISSION && my_drone.coord.x == my_drone.target.x && my_drone.coord.y == my_drone.target.y) {
            // Wait a short while to ensure status update is fully processed
            pthread_mutex_unlock(&my_drone.lock);
            usleep(100000); // 100ms delay
            pthread_mutex_lock(&my_drone.lock);
            
            // Only proceed if we're still on mission (status may have changed)
            if (my_drone.status != ON_MISSION) {
                pthread_mutex_unlock(&my_drone.lock);
                continue;
            }
            
            // Notify the server about mission completion
            printf("*** TARGET REACHED! Current=(%d,%d), Target=(%d,%d) - Preparing MISSION_COMPLETE message\n",
                  my_drone.coord.x, my_drone.coord.y, my_drone.target.x, my_drone.target.y);
            
            // Measure response time for mission completion
            struct timespec start_time, end_time;
            clock_gettime(CLOCK_MONOTONIC, &start_time);
                  
            struct json_object *mission_complete = json_object_new_object();
            json_object_object_add(mission_complete, "type", json_object_new_string("MISSION_COMPLETE"));
            json_object_object_add(mission_complete, "drone_id", json_object_new_int(my_drone.id));
            json_object_object_add(mission_complete, "timestamp", json_object_new_int(time(NULL)));
            json_object_object_add(mission_complete, "success", json_object_new_boolean(1));
            json_object_object_add(mission_complete, "details", json_object_new_string("Mission completed successfully."));
            
            // Add target location to help server find the correct survivor
            struct json_object *target_location = json_object_new_object();
            json_object_object_add(target_location, "x", json_object_new_int(my_drone.target.x));
            json_object_object_add(target_location, "y", json_object_new_int(my_drone.target.y));
            json_object_object_add(mission_complete, "target_location", target_location);

            const char *mission_complete_str = json_object_to_json_string(mission_complete);
            size_t message_size = strlen(mission_complete_str);
            
            pthread_mutex_lock(&sock_mutex);
            ssize_t send_result = send(sock, mission_complete_str, message_size, 0);
            // Send a newline character to separate JSON messages
            send(sock, "\n", 1, 0);
            pthread_mutex_unlock(&sock_mutex);
            
            // Record throughput metrics
            if (send_result > 0) {
                perf_record_status_update(send_result); // Count as status update
                
                // Measure and record response time
                clock_gettime(CLOCK_MONOTONIC, &end_time);
                double response_time_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + 
                                         (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
                perf_record_response_time(response_time_ms);
                
                printf("*** MISSION_COMPLETE message sent successfully (%zd bytes, %.2fms)\n", send_result, response_time_ms);
            } else {
                perror("Failed to send MISSION_COMPLETE message");
                perf_record_error();
            }
            
            json_object_put(mission_complete);

            // Reset the drone's status to IDLE
            my_drone.status = IDLE;
            printf("*** Drone status changed to IDLE\n");
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

void* drone_status_monitor(void *arg) {
    (void)arg; // Unused parameter

    while (running) {
        pthread_mutex_lock(&my_drone.lock);
        printf("Drone Status: ID=%d, Status=%s, Position=(%d,%d), Target=(%d,%d)\n",
               my_drone.id,
               my_drone.status == IDLE ? "IDLE" : "ON_MISSION",
               my_drone.coord.x, my_drone.coord.y,
               my_drone.target.x, my_drone.target.y);
        pthread_mutex_unlock(&my_drone.lock);

        sleep(5); // Print status every 5 seconds
    }

    return NULL;
}

int main()
{
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    printf("Drone Client Starting - Initializing Performance Monitoring...\n");
    
    // Start client-side performance monitoring
    throughput_monitor = start_perf_monitor("drone_client_metrics.csv");
    if (throughput_monitor == 0) {
        fprintf(stderr, "Warning: Failed to start client performance monitoring\n");
    }

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        perf_record_error();
        export_metrics_json("client_error_metrics.json");
        stop_perf_monitor(throughput_monitor);
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported");
        perf_record_error();
        close(sock);
        export_metrics_json("client_error_metrics.json");
        stop_perf_monitor(throughput_monitor);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    printf("Connecting to server %s:%d...\n", SERVER_IP, SERVER_PORT);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        perf_record_error();
        close(sock);
        export_metrics_json("client_error_metrics.json");
        stop_perf_monitor(throughput_monitor);
        exit(EXIT_FAILURE);
    }

    printf("Connected to the rescue system server.\n");
    perf_record_connection(1); // Record successful connection

    // Seed random number generator
    srand(time(NULL));

    // Initialize the map dimensions before using them
    map.height = 30; // Example height
    map.width = 40;  // Example width

    // Set basic properties
    my_drone.status = IDLE;

    // Random starting position within map boundaries
    my_drone.coord.x = rand() % map.height;
    my_drone.coord.y = rand() % map.width;

    // Initial target is current position
    my_drone.target = my_drone.coord;

    // Initialize mutex
    pthread_mutex_init(&my_drone.lock, NULL);

    // Measure handshake response time
    struct timespec handshake_start, handshake_end;
    clock_gettime(CLOCK_MONOTONIC, &handshake_start);

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
    size_t handshake_size = strlen(json_str);
    
    pthread_mutex_lock(&sock_mutex);
    ssize_t bytes_sent = send(sock, json_str, handshake_size, 0);
    pthread_mutex_unlock(&sock_mutex);
    
    if (bytes_sent > 0) {
        perf_record_status_update(bytes_sent);
        printf("Drone info sent: %s (%zd bytes)\n", json_str, bytes_sent);
    } else {
        perf_record_error();
    }

    // Free JSON object
    json_object_put(drone_info);

    // Wait for HANDSHAKE_ACK from the server
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Server response: %s (%d bytes)\n", buffer, bytes_received);
        
        // Record handshake response time
        clock_gettime(CLOCK_MONOTONIC, &handshake_end);
        double handshake_time = (handshake_end.tv_sec - handshake_start.tv_sec) * 1000.0 + 
                               (handshake_end.tv_nsec - handshake_start.tv_nsec) / 1000000.0;
        perf_record_response_time(handshake_time);
        perf_record_status_update(bytes_received);
        
        // Parse the response to ensure it's a HANDSHAKE_ACK
        struct json_object *response = json_tokener_parse(buffer);
        struct json_object *type;
        if (json_object_object_get_ex(response, "type", &type) &&
            strcmp(json_object_get_string(type), "HANDSHAKE_ACK") == 0) {
            printf("Handshake acknowledged by server (%.2fms response time).\n", handshake_time);
        } else {
            fprintf(stderr, "Unexpected response from server. Exiting.\n");
            perf_record_error();
            json_object_put(response);
            close(sock);
            export_metrics_json("client_error_metrics.json");
            stop_perf_monitor(throughput_monitor);
            exit(EXIT_FAILURE);
        }
        json_object_put(response);
    } else {
        perror("Failed to receive HANDSHAKE_ACK");
        perf_record_error();
        close(sock);
        export_metrics_json("client_error_metrics.json");
        stop_perf_monitor(throughput_monitor);
        exit(EXIT_FAILURE);
    }

    // Periodically print drone status for debugging
    pthread_t status_monitor_thread;
    pthread_create(&status_monitor_thread, NULL, &drone_status_monitor, NULL);
    pthread_detach(status_monitor_thread);

    printf("Drone %d is ready for missions.\n", my_drone.id);

    // Start the drone behavior thread
    int result = pthread_create(&thread_id, NULL, &drone_behavior, NULL);
    if (result != 0) {
        fprintf(stderr, "Error creating thread %s\n", strerror(result));
        perf_record_error();
    }

    printf("Starting main message loop...\n");
    
    // Main loop to handle server messages
    while (running) {

        printf("Waiting for messages from server...\n");
        fflush(stdout); // Ensure the message is printed immediately

        bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("Message from server: %s (%d bytes)\n", buffer, bytes_received);
            perf_record_status_update(bytes_received);

            // Parse the server message
            struct json_object *message = json_tokener_parse(buffer);
            struct json_object *type;
            if (json_object_object_get_ex(message, "type", &type)) {
                const char *message_type = json_object_get_string(type);

                if (strcmp(message_type, "HEARTBEAT") == 0) {
                    // Measure heartbeat response time
                    struct timespec hb_start, hb_end;
                    clock_gettime(CLOCK_MONOTONIC, &hb_start);
                    
                    // Respond to heartbeat
                    struct json_object *heartbeat_response = json_object_new_object();
                    json_object_object_add(heartbeat_response, "type", json_object_new_string("HEARTBEAT_RESPONSE"));
                    json_object_object_add(heartbeat_response, "drone_id", json_object_new_int(my_drone.id));
                    json_object_object_add(heartbeat_response, "timestamp", json_object_new_int(time(NULL)));
                    const char *response_str = json_object_to_json_string(heartbeat_response);
                    size_t response_size = strlen(response_str);
                    
                    pthread_mutex_lock(&sock_mutex);
                    ssize_t hb_bytes_sent = send(sock, response_str, response_size, 0);
                    pthread_mutex_unlock(&sock_mutex);
                    
                    if (hb_bytes_sent > 0) {
                        perf_record_heartbeat(hb_bytes_sent);
                        
                        // Record heartbeat response time
                        clock_gettime(CLOCK_MONOTONIC, &hb_end);
                        double hb_time = (hb_end.tv_sec - hb_start.tv_sec) * 1000.0 + 
                                        (hb_end.tv_nsec - hb_start.tv_nsec) / 1000000.0;
                        perf_record_response_time(hb_time);
                    } else {
                        perf_record_error();
                    }
                    
                    json_object_put(heartbeat_response);
                } else if (strcmp(message_type, "ASSIGN_MISSION") == 0) {
                    // Handle mission assignment
                    struct json_object *target;
                    if (json_object_object_get_ex(message, "target", &target)) {
                        struct json_object *x, *y;
                        if (json_object_object_get_ex(target, "x", &x) &&
                            json_object_object_get_ex(target, "y", &y)) {
                            int target_x = json_object_get_int(x);
                            int target_y = json_object_get_int(y);
                            
                            pthread_mutex_lock(&my_drone.lock);
                            my_drone.target.x = target_x;
                            my_drone.target.y = target_y;
                            my_drone.status = ON_MISSION;
                            printf("*** MISSION STATUS CHANGE: Drone %d status set to ON_MISSION\n", my_drone.id);
                            pthread_mutex_unlock(&my_drone.lock);
                            
                            printf("Mission assigned: Target (%d, %d) - Current position: (%d, %d)\n", 
                                  target_x, target_y, my_drone.coord.x, my_drone.coord.y);
                        }
                    }
                }
            }
            json_object_put(message);
        } else if (bytes_received == 0) {
            printf("Server disconnected.\n");
            perf_record_connection(0); // Record disconnection
            break;
        } else {
            perror("Error receiving message from server");
            perf_record_error();
            break;
        }
    }

    printf("Client shutting down - finalizing metrics...\n");

    // Wait for the thread to finish
    running = 0; // Signal the thread to exit
    pthread_join(thread_id, NULL);

    // Cleanup resources
    pthread_mutex_destroy(&my_drone.lock);
    close(sock);
    
    // Record final disconnection and export metrics
    perf_record_connection(0);
    export_metrics_json("final_client_metrics.json");
    stop_perf_monitor(throughput_monitor);

    printf("Drone client shutdown complete.\n");
    return 0;
}