/**
 * @file map.h
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @brief 2D spatial grid system for drone coordination and survivor tracking
 * @version 0.1
 * @date 2025-05-22
 * 
 * This header defines the spatial organization system for the emergency
 * drone coordination application. It provides a 2D grid structure where
 * each cell can contain survivors and supports efficient spatial queries
 * for mission planning and visualization.
 * 
 * **Key Features:**
 * - Dynamic 2D grid allocation with configurable dimensions
 * - Per-cell survivor lists for efficient spatial queries
 * - Thread-safe access to map data structures
 * - Memory-efficient organization with contiguous allocation
 * - Coordinate validation and bounds checking
 * 
 * **Spatial Organization:**
 * The map uses a row-major layout where:
 * - X-axis represents rows (0 to height-1)
 * - Y-axis represents columns (0 to width-1)
 * - Origin (0,0) is at the top-left corner
 * - Each cell can contain multiple survivors
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup core_modules
 * @ingroup spatial_system
 */

#ifndef MAP_H
#define MAP_H

#include "survivor.h"
#include "list.h"
#include "coord.h"

/**
 * @defgroup spatial_system Spatial Grid Management
 * @brief 2D grid system for spatial organization and queries
 * @ingroup core_modules
 * @{
 */

/**
 * @defgroup map_structures Map Data Structures
 * @brief Core data types for spatial grid representation
 * @{
 */

/**
 * @struct mapcell
 * @brief Structure representing a single cell in the spatial grid
 * 
 * Each map cell represents a discrete location in the 2D coordinate system
 * and maintains a list of survivors currently located within that cell.
 * This design enables efficient spatial queries by organizing entities
 * according to their geographic location.
 * 
 * **Cell Organization:**
 * - Each cell has fixed coordinate boundaries
 * - Survivors are tracked in a thread-safe list
 * - Multiple survivors can occupy the same cell
 * - Empty cells consume minimal memory overhead
 * 
 * **Thread Safety:**
 * Each cell's survivor list has its own mutex protection, allowing
 * concurrent access to different cells while maintaining data integrity.
 * 
 * @note Cell coordinates are immutable once initialized
 * @warning Always validate coordinates before cell access
 */
typedef struct mapcell {
    Coord coord;      /**< Immutable coordinates of this cell (x=row, y=column) */
    List *survivors;  /**< Thread-safe list of survivors currently in this cell */
} MapCell;

/**
 * @struct map
 * @brief Structure representing the complete 2D spatial grid system
 * 
 * The map structure contains the entire spatial grid with dynamically
 * allocated cells organized in a row-major layout. It provides the
 * foundation for all spatial operations in the drone coordination system.
 * 
 * **Memory Layout:**
 * - cells[i][j] represents the cell at row i, column j
 * - Contiguous allocation for cache efficiency
 * - Each row is a separate allocation for flexibility
 * 
 * **Coordinate System:**
 * - height: Number of rows (X-axis range)
 * - width: Number of columns (Y-axis range)
 * - Valid coordinates: 0 <= x < height, 0 <= y < width
 * 
 * **Initialization:**
 * Must be initialized with init_map() before use. The structure
 * remains valid until freemap() is called during shutdown.
 * 
 * @note Global instance available as extern variable
 * @warning Direct field access should be avoided - use accessor functions
 */
typedef struct map {
    int height;        /**< Number of rows in the grid (X-axis dimension) */
    int width;         /**< Number of columns in the grid (Y-axis dimension) */
    MapCell **cells;   /**< 2D array of map cells [height][width] */
} Map;

/** @} */ // end of map_structures group

/**
 * @defgroup map_globals Global Map Instance
 * @brief Global variables for system-wide spatial access
 * @{
 */

/** 
 * @brief Global map instance for the entire system
 * 
 * Single global map that provides spatial organization for all entities
 * in the drone coordination system. This centralized approach ensures
 * consistent coordinate systems across all modules.
 * 
 * **Usage:**
 * - Initialized once during system startup
 * - Accessed by all modules requiring spatial operations
 * - Remains valid throughout application lifetime
 * - Cleaned up during system shutdown
 * 
 * **Access Pattern:**
 * Use the provided accessor functions rather than direct field access
 * to ensure proper bounds checking and thread safety.
 * 
 * @note Must be initialized with init_map() before use
 * @warning Direct access to cells array should be avoided
 * 
 * @see init_map() for initialization
 * @see freemap() for cleanup
 */
extern Map map;

/** @} */ // end of map_globals group

/**
 * @defgroup map_management Map Lifecycle Management
 * @brief Functions for map initialization and cleanup
 * @{
 */

/**
 * @brief Initialize the spatial grid with specified dimensions
 * 
 * Creates and initializes the complete 2D spatial grid system including
 * all cells and their associated survivor lists. This function performs
 * all necessary memory allocation and data structure setup.
 * 
 * **Allocation Process:**
 * 1. Validate input dimensions (must be positive)
 * 2. Allocate array of row pointers (height elements)
 * 3. For each row, allocate array of MapCell structures (width elements)
 * 4. Initialize each cell with coordinates and empty survivor list
 * 5. Handle allocation failures with proper cleanup
 * 
 * **Memory Organization:**
 * - Row-major layout for cache efficiency
 * - Each cell gets thread-safe survivor list (capacity 10)
 * - Contiguous allocation within rows
 * - Clean failure handling with partial cleanup
 * 
 * **Error Handling:**
 * If any allocation fails, the function performs comprehensive cleanup
 * of all previously allocated resources before calling exit(EXIT_FAILURE).
 * 
 * @param height Number of rows in the grid (must be > 0)
 * @param width Number of columns in the grid (must be > 0)
 * 
 * @pre height > 0 && width > 0
 * @post Global map is fully initialized and ready for use
 * @post map.cells[i][j] is valid for all 0 <= i < height, 0 <= j < width
 * @post Each cell has an initialized, empty survivor list
 * 
 * @note Will call exit(EXIT_FAILURE) on allocation failure
 * @warning Must be called before any other map operations
 * 
 * @see freemap() for corresponding cleanup
 * @see MapCell for individual cell structure
 */
void init_map(int height, int width);

/**
 * @brief Free all map resources and clean up memory
 * 
 * Performs comprehensive cleanup of the entire map system including
 * all allocated memory and data structures. This function ensures
 * proper resource deallocation to prevent memory leaks.
 * 
 * **Cleanup Process:**
 * 1. Check for valid map structure
 * 2. For each row:
 *    a. Destroy survivor list in each cell
 *    b. Free the row array
 * 3. Free the main pointer array
 * 4. Reset dimensions to zero
 * 5. Set pointers to NULL for safety
 * 
 * **Thread Safety:**
 * This function should only be called during system shutdown when
 * no other threads are accessing map data. It does not acquire
 * locks as it assumes exclusive access during cleanup.
 * 
 * @pre Map may or may not be initialized (function handles both cases)
 * @post All map memory is freed and pointers are invalid
 * @post Global map dimensions are reset to zero
 * @post All survivor lists are properly destroyed
 * 
 * @note Safe to call multiple times or on uninitialized map
 * @note Handles NULL pointers gracefully
 * @warning Map becomes invalid after this call
 * 
 * @see init_map() for initialization
 */
void freemap();

/** @} */ // end of map_management group

/**
 * @defgroup map_utilities Map Utility Functions
 * @brief Helper functions for map operations and validation
 * @{
 */

/**
 * @brief Validate coordinates against map boundaries
 * 
 * Utility function to check if given coordinates fall within the
 * valid range of the current map. Essential for preventing buffer
 * overflows and segmentation faults when accessing map cells.
 * 
 * **Validation Rules:**
 * - x must be >= 0 and < map.height
 * - y must be >= 0 and < map.width
 * - Map must be properly initialized
 * 
 * @param x X coordinate (row index) to validate
 * @param y Y coordinate (column index) to validate
 * @return 1 if coordinates are valid, 0 otherwise
 * 
 * @pre Map should be initialized for meaningful results
 * @post No side effects - pure validation function
 * 
 * @note Returns 0 for uninitialized map (height/width = 0)
 * @see get_cell() for safe cell access
 */
int is_valid_coordinate(int x, int y);

/**
 * @brief Get pointer to cell at specified coordinates with bounds checking
 * 
 * Provides safe access to map cells with automatic coordinate validation.
 * Returns NULL for invalid coordinates instead of causing memory access
 * violations, making it safe for use with untrusted coordinate values.
 * 
 * **Safety Features:**
 * - Automatic bounds checking using is_valid_coordinate()
 * - NULL return for invalid coordinates
 * - No memory access violations possible
 * 
 * @param x X coordinate (row index)
 * @param y Y coordinate (column index)
 * @return Pointer to MapCell if coordinates are valid, NULL otherwise
 * 
 * @pre Map should be initialized
 * @post Returns valid MapCell pointer or NULL
 * 
 * @warning Always check return value before dereferencing
 * @note Preferred method for accessing map cells safely
 * 
 * @see is_valid_coordinate() for validation logic
 */
MapCell* get_cell(int x, int y);

/**
 * @brief Calculate total number of survivors across all map cells
 * 
 * Iterates through the entire map and sums the number of survivors
 * in all cell lists. This provides a global count for statistics
 * and monitoring purposes, though it can be expensive for large maps.
 * 
 * **Performance Characteristics:**
 * - O(height * width) complexity
 * - Locks each cell briefly for thread safety
 * - May be slow for large maps with many cells
 * 
 * **Thread Safety:**
 * Locks each cell's survivor list individually during counting,
 * ensuring accurate counts even with concurrent modifications.
 * 
 * @return Total number of survivors currently on the map
 * 
 * @pre Map must be initialized
 * @post Returns accurate survivor count at time of call
 * 
 * @note Thread-safe but potentially expensive operation
 * @note Consider caching result if called frequently
 * @warning Count may change immediately after function returns
 */
int get_total_survivor_count();

/** @} */ // end of map_utilities group

/** @} */ // end of spatial_system group

#endif // MAP_H