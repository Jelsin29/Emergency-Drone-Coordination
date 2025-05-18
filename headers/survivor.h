#ifndef SURVIVOR_H
#define SURVIVOR_H

#include "coord.h"
#include <time.h>
#include "list.h"
#include <pthread.h>

#define MAX_SURVIVORS 100  // Define maximum survivors

typedef struct survivor {
    int status;
    Coord coord;
    struct tm discovery_time;
    struct tm helped_time;
    char info[25];
} Survivor;

// Global survivor array (new implementation)
extern Survivor *survivor_array;
extern int num_survivors;
extern pthread_mutex_t survivors_mutex;

// Global survivor lists (old implementation, kept for compatibility)
extern List *survivors;          // Survivors awaiting help
extern List *helpedsurvivors;    // Helped survivors

// Functions
void initialize_survivors(); 
void cleanup_survivors();
Survivor* create_survivor(Coord *coord, char *info, struct tm *discovery_time);
void *survivor_generator(void *args);
void survivor_cleanup(Survivor *s);

#endif