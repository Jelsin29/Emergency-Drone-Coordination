/**
 * @file map.c
 * @brief Implementation of 2D spatial grid system for emergency drone coordination
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @version 0.1
 * @date 2025-05-22
 * 
 * This module implements the core spatial organization system for the emergency
 * drone coordination application. It provides a 2D grid structure where each
 * cell can contain survivors, enabling efficient spatial queries and optimal
 * mission assignment algorithms.
 * 
 * **Spatial Organization:**
 * - Dynamic 2D grid allocation with configurable dimensions
 * - Row-major memory layout for cache efficiency
 * - Per-cell survivor lists with thread-safe access
 * - Coordinate validation and bounds checking
 * - Memory-efficient design with proper cleanup
 * 
 * **Grid Structure:**
 * - Each cell maintains its own survivor list (capacity 10)
 * - Thread-safe operations through per-cell list mutexes
 * - Coordinates use (x,y) = (row,column) convention
 * - Origin (0,0) at top-left corner of grid
 * - Supports arbitrary grid dimensions within memory limits
 * 
 * **Performance Features:**
 * - Contiguous memory allocation for spatial locality
 * - Efficient bounds checking for all operations
 * - Minimal overhead for empty cells
 * - Fast spatial queries through direct cell access
 * 
 * **Thread Safety:**
 * - Individual cell lists have independent mutex protection
 * - Concurrent access to different cells is fully supported
 * - Proper initialization and cleanup with error handling
 * - Safe multi-threaded access patterns throughout
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup spatial_system
 * @ingroup core_modules
 */

#include "headers/map.h"
#include "headers/list.h"
#include <stdlib.h>
#include <stdio.h>

/** @brief Global map instance (defined here, declared extern in map.h) */
Map map;

/**
 * @brief Initialize the map with given dimensions
 * 
 * Creates a 2D grid of MapCell structures, each containing coordinates
 * and a thread-safe list for tracking survivors in that cell. This allows
 * for efficient spatial queries and reduces lock contention by providing
 * per-cell synchronization.
 * 
 * Memory layout:
 * - Allocates height * width MapCell structures
 * - Each cell gets its own survivor list with capacity of 10
 * - Uses row-major ordering for memory locality
 * 
 * @param height Map height (number of rows) - must be > 0
 * @param width Map width (number of columns) - must be > 0
 * 
 * @pre height > 0 && width > 0
 * @post map.cells[i][j] is valid for 0 <= i < height, 0 <= j < width
 * @post Each cell has an initialized survivor list
 * 
 * @warning This function will call exit(EXIT_FAILURE) if memory allocation fails
 * 
 * @see freemap() for cleanup
 * @see MapCell for cell structure details
 */
void init_map(int height, int width) {
    // Validate input parameters
    if (height <= 0 || width <= 0) {
        fprintf(stderr, "Error: Invalid map dimensions: %dx%d\n", height, width);
        exit(EXIT_FAILURE);
    }
    
    printf("Initializing map with dimensions: %dx%d cells\n", height, width);
    
    map.height = height;
    map.width = width;

    // Allocate rows (height) - array of pointers to MapCell arrays
    map.cells = (MapCell**)malloc(sizeof(MapCell*) * height);
    if (!map.cells) {
        perror("Failed to allocate map rows");
        exit(EXIT_FAILURE);
    }

    // Allocate columns (width) for each row and initialize cells
    for (int i = 0; i < height; i++) {
        map.cells[i] = (MapCell*)malloc(sizeof(MapCell) * width);
        if (!map.cells[i]) {
            // Clean up any previously allocated rows before exiting
            for (int cleanup_row = 0; cleanup_row < i; cleanup_row++) {
                for (int j = 0; j < width; j++) {
                    if (map.cells[cleanup_row][j].survivors) {
                        map.cells[cleanup_row][j].survivors->destroy(map.cells[cleanup_row][j].survivors);
                    }
                }
                free(map.cells[cleanup_row]);
            }
            free(map.cells);
            perror("Failed to allocate map columns");
            exit(EXIT_FAILURE);
        }

        // Initialize each cell in this row
        for (int j = 0; j < width; j++) {
            // Set cell coordinates
            map.cells[i][j].coord.x = i;
            map.cells[i][j].coord.y = j;
            
            // Create a thread-safe survivor list for this cell
            // Capacity of 10 should handle typical survivor density
            map.cells[i][j].survivors = create_list(sizeof(Survivor), 10);
            
            if (!map.cells[i][j].survivors) {
                fprintf(stderr, "Failed to create survivor list for cell (%d, %d)\n", i, j);
                // Clean up all previously created structures
                for (int cleanup_row = 0; cleanup_row <= i; cleanup_row++) {
                    int cleanup_cols = (cleanup_row == i) ? j : width;
                    for (int cleanup_col = 0; cleanup_col < cleanup_cols; cleanup_col++) {
                        if (map.cells[cleanup_row][cleanup_col].survivors) {
                            map.cells[cleanup_row][cleanup_col].survivors->destroy(map.cells[cleanup_row][cleanup_col].survivors);
                        }
                    }
                    free(map.cells[cleanup_row]);
                }
                free(map.cells);
                exit(EXIT_FAILURE);
            }
        }
    }
    
    printf("Map initialization completed successfully\n");
}

/**
 * @brief Free all map resources and clean up memory
 * 
 * Performs a complete cleanup of the map structure, including:
 * - Destroying all survivor lists in each cell
 * - Freeing all row arrays
 * - Freeing the main cell pointer array
 * 
 * This function ensures no memory leaks by carefully destroying
 * all thread-safe lists before freeing their container structures.
 * 
 * @pre map must have been initialized with init_map()
 * @post All map memory is freed and pointers are invalid
 * @post All survivor lists are properly destroyed
 * 
 * @note This function is safe to call multiple times
 * @note After calling this function, map.cells becomes invalid
 * 
 * @see init_map() for initialization
 */
void freemap() {
    if (!map.cells) {
        printf("Warning: Attempting to free uninitialized map\n");
        return;
    }
    
    printf("Freeing map resources...\n");
    
    // Free each row and its cells
    for (int i = 0; i < map.height; i++) {
        if (map.cells[i]) {
            // Destroy survivor lists in each cell of this row
            for (int j = 0; j < map.width; j++) {
                if (map.cells[i][j].survivors) {
                    // Properly destroy the thread-safe list
                    map.cells[i][j].survivors->destroy(map.cells[i][j].survivors);
                    map.cells[i][j].survivors = NULL;
                }
            }
            
            // Free the row array
            free(map.cells[i]);
            map.cells[i] = NULL;
        }
    }
    
    // Free the main pointer array
    free(map.cells);
    map.cells = NULL;
    
    // Reset dimensions for safety
    map.height = 0;
    map.width = 0;
    
    printf("Map cleanup completed\n");
}

/**
 * @brief Check if given coordinates are within map bounds
 * 
 * Utility function to validate coordinates before accessing map cells.
 * Prevents buffer overflows and segmentation faults.
 * 
 * @param x X coordinate to check
 * @param y Y coordinate to check
 * @return 1 if coordinates are valid, 0 otherwise
 * 
 * @note This is an inline check - consider making it a macro for performance
 */
int is_valid_coordinate(int x, int y) {
    return (x >= 0 && x < map.height && y >= 0 && y < map.width);
}

/**
 * @brief Get a pointer to the cell at specified coordinates
 * 
 * Provides safe access to map cells with bounds checking.
 * 
 * @param x X coordinate (row)
 * @param y Y coordinate (column)
 * @return Pointer to MapCell if coordinates are valid, NULL otherwise
 * 
 * @warning Always check return value before dereferencing
 */
MapCell* get_cell(int x, int y) {
    if (!is_valid_coordinate(x, y)) {
        return NULL;
    }
    return &map.cells[x][y];
}

/**
 * @brief Get the total number of survivors across all map cells
 * 
 * Iterates through all cells and sums up the survivor counts.
 * This is a relatively expensive operation for large maps.
 * 
 * @return Total number of survivors on the map
 * 
 * @note This function locks each cell briefly, so it's thread-safe
 * @note Consider caching this value if called frequently
 */
int get_total_survivor_count() {
    int total = 0;
    
    for (int i = 0; i < map.height; i++) {
        for (int j = 0; j < map.width; j++) {
            if (map.cells[i][j].survivors) {
                pthread_mutex_lock(&map.cells[i][j].survivors->lock);
                total += map.cells[i][j].survivors->number_of_elements;
                pthread_mutex_unlock(&map.cells[i][j].survivors->lock);
            }
        }
    }
    
    return total;
}