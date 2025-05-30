/**
 * @file globals.h
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @brief Global variable declarations and system-wide definitions
 * @version 0.1
 * @date 2025-05-22
 * 
 * This header contains declarations for global variables that are shared
 * across multiple modules in the emergency drone coordination system.
 * It provides a centralized location for system-wide data structures
 * and ensures consistent access patterns.
 * 
 * **Global Data Structures:**
 * - Map grid for spatial operations
 * - Synchronized lists for drones and survivors
 * - System-wide constants and configurations
 * 
 * @warning All global variables declared here must be properly initialized
 *          before use and cleaned up during system shutdown
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup core_modules
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include "coord.h"
#include "map.h"
#include "drone.h"
#include "survivor.h"
#include "list.h"

// Forward declarations to avoid circular dependencies
struct map;
struct list;
struct drone;
struct survivor;

/**
 * @brief Global map instance for the simulation area
 * 
 * The map represents the 2D grid where drones operate and survivors
 * are located. It provides spatial organization and efficient lookup
 * of entities by location.
 * 
 * @note Must be initialized with init_map() before use
 * @see init_map() for initialization
 * @see freemap() for cleanup
 */
extern struct map map;

/**
 * @brief Global list of survivors waiting for rescue
 * 
 * Thread-safe list containing all survivors that have been detected
 * and are awaiting assistance. This list is continuously updated
 * by the survivor generator thread and accessed by the AI controller.
 * 
 * **Access Pattern:**
 * - Read access: AI controller for mission planning
 * - Write access: Survivor generator for new arrivals
 * - Update access: Drones for status changes
 * 
 * @note Thread-safe with internal mutex protection
 * @see create_list() for initialization
 * @see Survivor for element structure
 */
// clang-format off
extern struct list *survivors;

/**
 * @brief Global list of survivors who have received help
 * 
 * Contains survivors who are either currently being assisted by drones
 * or have been successfully rescued. Used for tracking rescue progress
 * and generating statistics.
 * 
 * @note Elements may be moved between survivors and helpedsurvivors lists
 */
extern struct list *helpedsurvivors;

/**
 * @brief Global list of active drones in the system
 * 
 * Thread-safe list containing all drones currently participating
 * in rescue operations. Includes both local simulation drones and
 * networked drone clients.
 * 
 * **Drone Types:**
 * - Local drones: Simulated entities for testing
 * - Network drones: Real client connections via TCP/IP
 * 
 * **Synchronization:**
 * - List-level mutex for add/remove operations
 * - Individual drone mutexes for status updates
 * 
 * @note Each drone has its own mutex for thread-safe property access
 * @see Drone for element structure and synchronization details
 */
extern struct list *drones;

// clang-format on

/**
 * @defgroup system_constants System Constants
 * @brief Configuration constants for system operation
 * @{
 */

/** @brief Maximum number of concurrent drone connections */
#define MAX_DRONES 100

/** @brief Maximum number of survivors that can be tracked */
#define MAX_SURVIVORS 1000

/** @brief Default TCP port for drone-server communication */
#define DEFAULT_SERVER_PORT 8080

/** @brief Maximum size for network message buffers */
#define MAX_MESSAGE_SIZE 4096

/** @brief Default survivor generation interval (milliseconds) */
#define SURVIVOR_SPAWN_INTERVAL_MS 1000

/** @brief Default heartbeat interval for drone clients (seconds) */
#define HEARTBEAT_INTERVAL_SEC 10

/** @brief Maximum time before considering a drone disconnected (seconds) */
#define DRONE_TIMEOUT_SEC 30

/** @} */ // end of system_constants group

/**
 * @defgroup thread_globals Thread Management Globals
 * @brief Global variables for thread coordination
 * @{
 */

/** 
 * @brief Global flag controlling main system execution
 * 
 * When set to 0, signals all threads to terminate gracefully.
 * Modified by signal handlers and checked by all main loops.
 * 
 * @note Declared as volatile to prevent compiler optimizations
 */
extern volatile int running;

/** @} */ // end of thread_globals group

/**
 * @defgroup statistics_globals Statistics and Monitoring
 * @brief Global variables for system statistics
 * @{
 */

/** @brief Number of survivors currently waiting for rescue */
extern int waiting_count;

/** @brief Number of survivors currently being helped */
extern int helped_count;

/** @brief Total number of survivors successfully rescued */
extern int rescued_count;

/** @brief Number of drones currently idle and available */
extern int idle_drones;

/** @brief Number of drones currently on active missions */
extern int mission_drones;

/** @} */ // end of statistics_globals group

#endif // GLOBALS_H