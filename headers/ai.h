/**
 * @file ai.h
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @brief Intelligent mission assignment and optimization system for drone coordination
 * @version 0.1
 * @date 2025-05-22
 * 
 * This header defines the AI controller system responsible for intelligent
 * mission assignment, path optimization, and rescue coordination. The AI
 * system uses various algorithms to efficiently match idle drones with
 * survivors requiring assistance, optimizing for response time and system
 * throughput.
 * 
 * **Key Features:**
 * - Multiple AI algorithms for different optimization strategies
 * - Distance-based mission assignment using Manhattan distance
 * - Real-time mission completion detection and status updates
 * - Thread-safe coordination with drone and survivor systems
 * - Performance monitoring and optimization metrics
 * 
 * **AI Strategies:**
 * - Survivor-centric: Assign closest drone to each waiting survivor
 * - Drone-centric: Assign closest survivor to each idle drone
 * - Distance optimization using Manhattan distance calculations
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup core_modules
 * @ingroup ai_algorithms
 */

#ifndef AI_H
#define AI_H

#include "drone.h"
#include "survivor.h"

/**
 * @defgroup ai_algorithms AI Mission Assignment System
 * @brief Intelligent algorithms for drone-survivor coordination
 * @ingroup core_modules
 * @{
 */

/**
 * @defgroup ai_controllers AI Controller Functions
 * @brief Main AI coordination threads and control loops
 * @{
 */

/**
 * @brief Main AI controller using survivor-centric assignment strategy
 * 
 * Primary AI thread function that continuously monitors the system state
 * and assigns missions using a survivor-focused approach. For each waiting
 * survivor, it finds the closest idle drone and creates an optimal mission
 * assignment.
 * 
 * **Algorithm Strategy:**
 * 1. Scan all survivors for those awaiting rescue (status 0)
 * 2. For each waiting survivor, find the closest idle drone
 * 3. Assign mission to optimize response time
 * 4. Monitor mission completions and update survivor status
 * 5. Repeat continuously with configurable cycle timing
 * 
 * **Performance Characteristics:**
 * - Optimizes for survivor wait times
 * - Scales linearly with number of survivors
 * - Includes mission completion detection
 * - Provides comprehensive performance logging
 * 
 * **Thread Safety:**
 * - Coordinates with drone and survivor mutexes
 * - Uses proper locking order to prevent deadlocks
 * - Handles concurrent modifications safely
 * 
 * @param args Unused thread parameter (required for pthread compatibility)
 * @return NULL when thread terminates
 * 
 * @pre All system components must be initialized
 * @pre Global drone and survivor lists must be available
 * @post Continuous mission assignment until thread termination
 * 
 * @note Runs with 1-second cycle time for balanced performance
 * @note Includes both assignment and completion phases
 * @warning Must be properly cancelled during system shutdown
 * 
 * @see drone_centric_ai_controller() for alternative strategy
 * @see assign_mission() for mission creation details
 */
// clang-format off
void* ai_controller(void *args);

/**
 * @brief Alternative AI controller using drone-centric assignment strategy
 * 
 * Alternative AI implementation that optimizes from the drone perspective.
 * For each idle drone, it finds the closest waiting survivor and assigns
 * an optimal mission. This approach can be more efficient when the number
 * of drones is smaller than the number of survivors.
 * 
 * **Algorithm Strategy:**
 * 1. Scan all drones for those in idle status
 * 2. For each idle drone, find the closest waiting survivor
 * 3. Assign mission to minimize total travel distance
 * 4. Focus on drone utilization optimization
 * 5. Repeat continuously with performance monitoring
 * 
 * **Performance Characteristics:**
 * - Optimizes for drone utilization efficiency
 * - Scales linearly with number of drones
 * - More efficient when drones << survivors
 * - Simplified mission completion handling
 * 
 * **Differences from Survivor-Centric:**
 * - No explicit mission completion detection loop
 * - Relies on external mission completion notifications
 * - Better performance with large survivor populations
 * - Focuses on drone resource optimization
 * 
 * @param args Unused thread parameter (required for pthread compatibility)
 * @return NULL when thread terminates
 * 
 * @pre All system components must be initialized
 * @pre Global drone and survivor lists must be available
 * @post Continuous mission assignment until thread termination
 * 
 * @note Runs with 1-second cycle time for balanced performance
 * @note Does not include explicit completion detection phase
 * @warning Must be properly cancelled during system shutdown
 * 
 * @see ai_controller() for survivor-centric alternative
 * @see find_closest_waiting_survivor() for core algorithm
 */
// clang-format off
void* drone_centric_ai_controller(void *args);
// clang-format on
/** @} */ // end of ai_controllers group

/**
 * @defgroup mission_assignment Mission Assignment Operations
 * @brief Functions for creating and managing drone missions
 * @{
 */

/**
 * @brief Assign a rescue mission to a specific drone for a specific survivor
 * 
 * Creates a complete mission assignment including status updates, target
 * setting, and network communication for remote drones. This function
 * handles both local simulation drones and networked drone clients.
 * 
 * **Assignment Process:**
 * 1. Validate drone and survivor parameters
 * 2. Lock both drone and survivor for atomic updates
 * 3. Verify both entities are in assignable states
 * 4. Update drone target coordinates and status
 * 5. Update survivor status to "being helped"
 * 6. Send mission message to networked drones
 * 7. Record performance metrics and timestamps
 * 
 * **Network Communication:**
 * For networked drone clients, creates a JSON mission message containing:
 * - Mission type and unique ID
 * - Priority level and target coordinates
 * - Expiry time for mission validity
 * - All data formatted according to protocol specification
 * 
 * **Error Handling:**
 * - Validates all input parameters
 * - Rolls back status changes on network failures
 * - Records errors in performance monitoring system
 * - Provides detailed logging for debugging
 * 
 * @param drone Pointer to the drone receiving the mission assignment
 * @param survivor_index Array index of the survivor requiring rescue
 * 
 * @pre drone must be valid pointer to initialized drone
 * @pre survivor_index must be valid array index (0 <= index < num_survivors)
 * @pre Drone should be in IDLE status for successful assignment
 * @pre Survivor should have status 0 (waiting) for assignment
 * @post Drone status is ON_MISSION with target set to survivor location
 * @post Survivor status is 1 (being helped)
 * @post Network message sent to remote drones if applicable
 * 
 * @note Function is thread-safe with proper mutex coordination
 * @note Handles both local and networked drone types automatically
 * @warning Assignment may fail if entities are in wrong states
 * 
 * @see find_closest_idle_drone() for drone selection
 * @see update_drone_status() for mission completion handling
 */
void assign_mission(Drone *drone, int survivor_index);

/** @} */ // end of mission_assignment group

/**
 * @defgroup distance_calculations Distance and Optimization Functions
 * @brief Spatial calculations for mission optimization
 * @{
 */

/**
 * @brief Calculate Manhattan distance between two coordinate points
 * 
 * Computes the Manhattan (taxicab) distance between two points on the
 * 2D grid. This distance metric is appropriate for grid-based movement
 * where diagonal movement is not allowed or has the same cost as
 * orthogonal movement.
 * 
 * **Distance Formula:**
 * ```
 * distance = |x1 - x2| + |y1 - y2|
 * ```
 * 
 * **Use Cases:**
 * - Mission assignment optimization
 * - Drone-survivor pairing decisions
 * - Path length estimation
 * - Performance metric calculations
 * 
 * **Performance:**
 * - O(1) time complexity
 * - Simple integer arithmetic
 * - No floating-point operations required
 * - Cache-friendly for repeated calculations
 * 
 * @param a First coordinate point
 * @param b Second coordinate point
 * @return Manhattan distance as integer (always non-negative)
 * 
 * @pre Coordinate values should be valid integers
 * @post Returns accurate Manhattan distance
 * 
 * @note Result is always non-negative due to absolute value calculation
 * @note More appropriate than Euclidean distance for grid-based systems
 * 
 * @see assign_mission() for usage in mission assignment
 * @see find_closest_idle_drone() for optimization applications
 */
int calculate_distance(Coord a, Coord b);

/** @} */ // end of distance_calculations group

/**
 * @defgroup drone_selection Drone Selection Algorithms
 * @brief Functions for finding optimal drones for mission assignment
 * @{
 */

/**
 * @brief Find the closest idle drone to a specific survivor location
 * 
 * Searches through all available drones to find the one that is both
 * idle and closest to the specified survivor. This function implements
 * the core optimization logic for survivor-centric mission assignment.
 * 
 * **Search Algorithm:**
 * 1. Validate survivor index and get survivor coordinates
 * 2. Initialize minimum distance tracker
 * 3. Iterate through all drones in the global list
 * 4. For each drone, check if status is IDLE
 * 5. Calculate distance to survivor location
 * 6. Track drone with minimum distance
 * 7. Return closest idle drone or NULL if none available
 * 
 * **Thread Safety:**
 * - Locks global drone list during iteration
 * - Locks individual drone mutexes for status checking
 * - Uses survivor mutex for coordinate access
 * - Prevents race conditions with concurrent operations
 * 
 * **Performance Characteristics:**
 * - O(n) time complexity where n = number of drones
 * - Linear search through drone list
 * - Minimal memory allocation
 * - Efficient for typical drone fleet sizes
 * 
 * @param survivor_index Index of survivor in global survivor array
 * @return Pointer to closest idle drone, or NULL if none available
 * 
 * @pre survivor_index must be valid (0 <= index < num_survivors)
 * @pre Global drone list must be initialized
 * @pre Survivor array must be accessible
 * @post Returns optimal drone or NULL without side effects
 * 
 * @note Thread-safe through careful mutex coordination
 * @note May return NULL if no drones are idle
 * @warning Returned pointer may become invalid if drone disconnects
 * 
 * @see calculate_distance() for distance calculations
 * @see assign_mission() for mission creation using result
 */
Drone *find_closest_idle_drone(int survivor_index);

/** @} */ // end of drone_selection group

/**
 * @defgroup survivor_selection Survivor Selection Algorithms
 * @brief Functions for finding optimal survivors for drone assignment
 * @{
 */

/**
 * @brief Find the closest waiting survivor to a specific drone location
 * 
 * Searches through all survivors to find the one that is waiting for
 * rescue and closest to the specified drone. This function implements
 * the core optimization logic for drone-centric mission assignment.
 * 
 * **Search Algorithm:**
 * 1. Validate drone pointer and get drone coordinates
 * 2. Initialize minimum distance tracker
 * 3. Iterate through all survivors in the global array
 * 4. For each survivor, check if status is 0 (waiting)
 * 5. Calculate distance from drone location
 * 6. Track survivor with minimum distance
 * 7. Return index of closest waiting survivor
 * 
 * **Thread Safety:**
 * - Locks drone mutex for coordinate access
 * - Locks survivor mutex for array iteration
 * - Prevents race conditions during search
 * - Safe concurrent execution with other AI operations
 * 
 * **Performance Characteristics:**
 * - O(n) time complexity where n = number of survivors
 * - Linear search through survivor array
 * - Efficient for typical survivor populations
 * - Minimal computational overhead
 * 
 * **Return Value:**
 * Returns array index rather than pointer for consistency with
 * survivor array access patterns and to avoid pointer invalidation
 * issues during concurrent modifications.
 * 
 * @param drone Pointer to drone seeking mission assignment
 * @return Index of closest waiting survivor, or -1 if none available
 * 
 * @pre drone must be valid pointer to initialized drone
 * @pre Global survivor array must be initialized
 * @post Returns optimal survivor index or -1 without side effects
 * 
 * @note Thread-safe through careful mutex coordination
 * @note Returns -1 if no survivors are waiting for rescue
 * @note Index remains valid until survivor array is modified
 * 
 * @see calculate_distance() for distance calculations
 * @see assign_mission() for mission creation using result
 */
int find_closest_waiting_survivor(Drone *drone);

/** @} */ // end of survivor_selection group

/** @} */ // end of ai_algorithms group
// clang-format on

#endif // AI_H