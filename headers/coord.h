/**
 * @file coord.h
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @brief Coordinate system definitions for 2D spatial operations
 * @version 0.1
 * @date 2025-05-22
 * 
 * This header defines the fundamental coordinate structure used throughout
 * the drone coordination system for representing positions on the 2D map grid.
 * All spatial operations, including drone positions, survivor locations,
 * and mission targets, use this coordinate system.
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup core_modules
 */

#ifndef COORD_H
#define COORD_H

/**
 * @struct coord
 * @brief Structure representing 2D coordinates in the map grid
 * 
 * This structure defines a point in 2D space using integer coordinates.
 * The coordinate system uses:
 * - Origin (0,0) at the top-left corner of the map
 * - X-axis extends downward (increasing row numbers)
 * - Y-axis extends rightward (increasing column numbers)
 * 
 * **Usage Examples:**
 * ```c
 * Coord drone_pos = {5, 10};        // Drone at row 5, column 10
 * Coord target = {.x = 15, .y = 20}; // Target at row 15, column 20
 * ```
 * 
 * @note All coordinates should be validated against map boundaries before use
 * @see Map for boundary definitions
 */
typedef struct coord {
    int x; /**< X coordinate (row index, 0-based from top) */
    int y; /**< Y coordinate (column index, 0-based from left) */
} Coord;

/**
 * @brief Check if two coordinates are equal
 * @param a First coordinate
 * @param b Second coordinate
 * @return 1 if coordinates are equal, 0 otherwise
 */
#define COORD_EQUAL(a, b) ((a).x == (b).x && (a).y == (b).y)

/**
 * @brief Create a coordinate structure with given values
 * @param x_val X coordinate value
 * @param y_val Y coordinate value
 * @return Coord structure with specified values
 */
#define MAKE_COORD(x_val, y_val) ((Coord){ .x = (x_val), .y = (y_val) })

/**
 * @brief Calculate Manhattan distance between two coordinates
 * @param a First coordinate
 * @param b Second coordinate
 * @return Manhattan distance as integer
 */
#define MANHATTAN_DISTANCE(a, b) (abs((a).x - (b).x) + abs((a).y - (b).y))

#endif // COORD_H