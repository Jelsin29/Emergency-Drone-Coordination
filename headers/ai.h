#ifndef AI_H
#define AI_H

#include "drone.h"
#include "survivor.h"

// AI Mission Assignment
void* ai_controller(void *args);
void assign_mission(Drone *drone, int survivor_index);
int calculate_distance(Coord a, Coord b);
Drone*  find_closest_idle_drone(int survivor_index);

#endif