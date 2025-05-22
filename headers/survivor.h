/**
 * @file survivor.h
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @brief Survivor management system for emergency response coordination
 * @version 0.1
 * @date 2025-05-22
 * 
 * This header defines the complete survivor management system for tracking
 * people requiring emergency assistance. It provides data structures for
 * survivor information, status tracking, and thread-safe access patterns
 * for use by the AI controller and visualization systems.
 * 
 * **Key Features:**
 * - Thread-safe survivor array with mutex protection
 * - Status-based lifecycle management (waiting → being helped → rescued)
 * - Continuous survivor generation simulation
 * - Spatial organization through map cell integration
 * - Temporal tracking with discovery and rescue timestamps
 * 
 * **Survivor Lifecycle:**
 * ```
 * Status 0 (Waiting) → Status 1 (Being Helped) → Status 2 (Rescued)
 * ```
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup core_modules
 * @ingroup simulation
 */

#ifndef SURVIVOR_H
#define SURVIVOR_H

#include "coord.h"
#include <time.h>
#include "list.h"
#include <pthread.h>

/**
 * @defgroup simulation Emergency Simulation System
 * @brief Survivor generation and lifecycle management
 * @ingroup core_modules
 * @{
 */

/**
 * @defgroup survivor_constants Survivor System Constants
 * @brief Configuration constants for survivor management
 * @{
 */

/** 
 * @def MAX_SURVIVORS
 * @brief Maximum number of survivors that can be tracked simultaneously
 * 
 * This limit prevents unbounded memory growth and ensures predictable
 * performance characteristics. When the limit is reached, rescued
 * survivors may be recycled to make space for new arrivals.
 */
// Removed MAX_SURVIVORS definition to avoid conflict with globals.h

/** @} */ // end of survivor_constants group

/**
 * @defgroup survivor_structures Survivor Data Structures
 * @brief Core data types for survivor representation
 * @{
 */

/**
 * @struct survivor
 * @brief Structure representing a person requiring emergency assistance
 * 
 * Contains all information necessary to track a survivor's status,
 * location, and rescue timeline. The structure supports the complete
 * survivor lifecycle from initial discovery through successful rescue.
 * 
 * **Status Values:**
 * - 0: Waiting for rescue (displayed in red)
 * - 1: Being helped by a drone (displayed in red)
 * - 2: Successfully rescued (not displayed)
 * - 3: Archived rescued (available for recycling)
 * 
 * **Thread Safety:**
 * Access to survivor structures is protected by the global survivors_mutex.
 * Always acquire this mutex before reading or modifying survivor data.
 * 
 * @note Coordinate values must be validated against map boundaries
 * @warning Status changes must be synchronized with drone operations
 */
typedef struct survivor {
    int status;                 /**< Current rescue status (0=waiting, 1=being helped, 2=rescued) */
    Coord coord;                /**< Current location on the map grid */
    struct tm discovery_time;   /**< Timestamp when survivor was first detected */
    struct tm helped_time;      /**< Timestamp when rescue was completed */
    char info[25];              /**< Identifier string for tracking purposes */
} Survivor;

/** @} */ // end of survivor_structures group

/**
 * @defgroup survivor_globals Global Survivor Management
 * @brief Global variables for system-wide survivor tracking
 * @{
 */

/** 
 * @brief Global array containing all tracked survivors
 * 
 * Fixed-size array that holds all survivors currently in the system.
 * Access must be synchronized using survivors_mutex to prevent race
 * conditions between the generator thread, AI controller, and visualization.
 * 
 * **Memory Layout:**
 * - Contiguous allocation for cache efficiency
 * - Fixed size determined by MAX_SURVIVORS
 * - Zero-initialized on system startup
 * 
 * @note Array size is static for performance predictability
 * @warning Always use survivors_mutex when accessing this array
 */
extern Survivor *survivor_array;

/** 
 * @brief Current number of active survivors in the array
 * 
 * Tracks how many array slots are currently occupied by valid survivors.
 * This value increases as new survivors are generated and may decrease
 * during recycling operations when the array becomes full.
 * 
 * **Update Patterns:**
 * - Incremented by survivor_generator thread
 * - Read by AI controller for mission planning
 * - Read by visualization for display purposes
 * 
 * @note Must be accessed under survivors_mutex protection
 */
extern int num_survivors;

/** 
 * @brief Mutex protecting access to the survivor array
 * 
 * Critical synchronization primitive that ensures thread-safe access
 * to all survivor-related data structures. Must be acquired before
 * any read or write operations on survivor data.
 * 
 * **Protected Resources:**
 * - survivor_array contents
 * - num_survivors counter
 * - Individual survivor status changes
 * 
 * **Locking Order:**
 * When multiple mutexes are needed, acquire survivors_mutex before
 * any drone-specific mutexes to prevent deadlock.
 * 
 * @warning Failure to use this mutex can cause data corruption
 */
extern pthread_mutex_t survivors_mutex;

/** 
 * @brief Global list of survivors awaiting rescue assistance
 * 
 * Thread-safe list containing survivors with status 0 (waiting).
 * Used by the AI controller for efficient mission assignment
 * without scanning the entire survivor array.
 * 
 * @note This list has its own internal mutex protection
 * @see List structure for thread-safety details
 */
extern List *survivors;

/** 
 * @brief Global list of survivors who have received help
 * 
 * Thread-safe list containing survivors with status 1 (being helped)
 * or status 2 (rescued). Used for tracking rescue progress and
 * generating completion statistics.
 * 
 * @note Separate from main array for performance optimization
 */
extern List *helpedsurvivors;

/** @} */ // end of survivor_globals group

/**
 * @defgroup survivor_management Survivor Lifecycle Management
 * @brief Functions for survivor system initialization and cleanup
 * @{
 */

/**
 * @brief Initialize the complete survivor management system
 * 
 * Performs all necessary setup for survivor tracking including memory
 * allocation, mutex initialization, and data structure preparation.
 * Must be called before any other survivor-related operations.
 * 
 * **Initialization Steps:**
 * 1. Allocate survivor array memory (MAX_SURVIVORS * sizeof(Survivor))
 * 2. Zero-initialize all array contents
 * 3. Initialize survivors_mutex for thread synchronization
 * 4. Set initial survivor count to zero
 * 
 * @pre None - can be called on uninitialized system
 * @post Survivor system is ready for operation
 * 
 * @note Will call exit(EXIT_FAILURE) if memory allocation fails
 * @warning Must be called before starting any survivor-related threads
 * 
 * @see cleanup_survivors() for corresponding cleanup
 */
void initialize_survivors();

/**
 * @brief Clean up all survivor system resources
 * 
 * Performs comprehensive cleanup of the survivor management system
 * including memory deallocation and mutex destruction. Should be
 * called during system shutdown to prevent resource leaks.
 * 
 * **Cleanup Operations:**
 * 1. Destroy survivors_mutex
 * 2. Free survivor_array memory
 * 3. Reset survivor count to zero
 * 4. Null out pointer references
 * 
 * @pre Survivor system must have been initialized
 * @post All survivor resources are freed
 * 
 * @note Safe to call multiple times
 * @warning Do not access survivor data after calling this function
 */
void cleanup_survivors();

/** @} */ // end of survivor_management group

/**
 * @defgroup survivor_creation Survivor Creation and Initialization
 * @brief Functions for creating new survivor instances
 * @{
 */

/**
 * @brief Create a new survivor with specified attributes
 * 
 * Allocates and initializes a new survivor structure with the provided
 * information. This is a utility function for creating properly formatted
 * survivor objects, though most survivor creation occurs automatically
 * through the generator thread.
 * 
 * **Initialization:**
 * - Copies provided coordinate, info, and discovery time
 * - Sets initial status to 0 (waiting for rescue)
 * - Ensures proper null-termination of info string
 * - Zero-initializes helped_time field
 * 
 * @param coord Pointer to initial location coordinates
 * @param info Descriptive string for survivor identification
 * @param discovery_time Pointer to discovery timestamp
 * @return Pointer to newly allocated survivor, or NULL if allocation fails
 * 
 * @pre coord, info, and discovery_time must be valid pointers
 * @pre info string must be null-terminated
 * @post New survivor is allocated and initialized
 * 
 * @note Caller is responsible for freeing returned pointer
 * @warning Does not add survivor to global arrays or lists
 * 
 * @see survivor_generator() for automatic creation
 */
Survivor* create_survivor(Coord *coord, char *info, struct tm *discovery_time);

/** @} */ // end of survivor_creation group

/**
 * @defgroup survivor_generation Survivor Generation System
 * @brief Continuous survivor spawning simulation
 * @{
 */

/**
 * @brief Main thread function for continuous survivor generation
 * 
 * Implements the survivor generation simulation that continuously adds
 * new survivors to the system at random locations. This thread runs
 * throughout the application lifetime, creating realistic emergency
 * scenarios for the drone coordination system to handle.
 * 
 * **Generation Pattern:**
 * 1. Initial burst: 10 survivors at 0.1 second intervals
 * 2. Continuous generation: 1 survivor every 0.5-1.5 seconds
 * 3. Array full handling: Recycle rescued survivors (status >= 2)
 * 
 * **Thread Safety:**
 * - Acquires survivors_mutex before array modifications
 * - Uses random number generation for location variety
 * - Handles array bounds and capacity limits
 * 
 * **Recycling Logic:**
 * When MAX_SURVIVORS is reached, up to 5 rescued survivors are
 * recycled by moving them to new random locations and resetting
 * their status to 0 (waiting).
 * 
 * @param args Unused thread parameter (required for pthread compatibility)
 * @return NULL when thread terminates
 * 
 * @pre Survivor system must be initialized
 * @pre Global map must be available for coordinate generation
 * @post Continuous survivor generation until thread termination
 * 
 * @note Runs continuously until system shutdown
 * @note Seeds random number generator with current time
 * @warning Thread must be properly cancelled during shutdown
 * 
 * @see initialize_survivors() for system setup
 */
void *survivor_generator(void *args);

/** @} */ // end of survivor_generation group

/**
 * @defgroup survivor_cleanup Survivor Cleanup Operations
 * @brief Functions for removing and cleaning up survivor instances
 * @{
 */

/**
 * @brief Clean up all resources associated with a specific survivor
 * 
 * Performs comprehensive cleanup for an individual survivor including
 * removal from map cells, global lists, and memory deallocation.
 * This function handles the complete cleanup lifecycle for survivors
 * that are being removed from the system.
 * 
 * **Cleanup Operations:**
 * 1. Remove from appropriate map cell survivor list
 * 2. Remove from global survivors list
 * 3. Remove from helpedsurvivors list
 * 4. Free allocated memory
 * 
 * **Thread Safety:**
 * - Performs bounds checking on coordinates
 * - Acquires appropriate mutexes for list operations
 * - Handles cases where survivor may not be in all lists
 * 
 * @param s Pointer to the survivor to clean up
 * 
 * @pre s must be a valid pointer to an allocated survivor
 * @pre Global map and lists must be initialized
 * @post Survivor is removed from all data structures and freed
 * 
 * @note Handles survivors that may not be in all expected lists
 * @warning Survivor pointer becomes invalid after this call
 * @warning Coordinate bounds are checked to prevent segmentation faults
 */
void survivor_cleanup(Survivor *s);

/** @} */ // end of survivor_cleanup group

/** @} */ // end of simulation group

#endif // SURVIVOR_H