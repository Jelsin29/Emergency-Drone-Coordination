#ifndef DRONE_H
#define DRONE_H

#include "coord.h"
#include <time.h>
#include <pthread.h>
#include "list.h"

typedef enum {
    IDLE,
    ON_MISSION,
    DISCONNECTED
} DroneStatus;


typedef struct drone {
    int id;
    pthread_t thread_id;
    int status;             // IDLE, ON_MISSION, DISCONNECTED
    Coord coord;
    Coord target;
    struct tm last_update;
    pthread_mutex_t lock;   // Per-drone mutex
    int socket;             // Socket for client communication
} Drone;

// Global drone list (extern)
extern List *drones;
extern int num_drones;    // Number of drones in the fleet

// Functions
void* drone_server(void *arg);
void *handle_drone_client(void *arg);
void initialize_drones();
void cleanup_drones();
void update_drone_status(Drone *drone, Coord *target);

#endif