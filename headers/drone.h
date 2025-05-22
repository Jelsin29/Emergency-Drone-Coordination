/**
 * @file drone.h
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @brief Drone management and network communication definitions
 * @version 0.1
 * @date 2025-05-22
 * 
 * This header defines the core drone functionality for the emergency
 * coordination system. It includes drone data structures, status management,
 * network communication protocols, and server operations for handling
 * multiple concurrent drone clients.
 * 
 * **Key Components:**
 * - Drone state management and lifecycle
 * - TCP/IP server for drone client connections
 * - JSON-based communication protocol
 * - Thread-safe operations for concurrent access
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup core_modules
 * @ingroup networking
 */

#ifndef DRONE_H
#define DRONE_H

#include "coord.h"
#include <time.h>
#include <pthread.h>

// Forward declaration to avoid circular dependency
struct list;

/**
 * @enum DroneStatus
 * @brief Enumeration of possible drone operational states
 * 
 * Defines the lifecycle states of a drone from connection through
 * mission execution to disconnection. State transitions are managed
 * by the AI controller and network handlers.
 * 
 * **State Transitions:**
 * ```
 * IDLE → ON_MISSION (when mission assigned)
 * ON_MISSION → IDLE (when mission completed)
 * Any State → DISCONNECTED (on network failure)
 * ```
 */
typedef enum {
    IDLE = 0,        /**< Drone is connected and available for missions */
    ON_MISSION = 1,  /**< Drone is executing an assigned rescue mission */
    DISCONNECTED = 2 /**< Drone has lost connection or been removed */
} DroneStatus;

/**
 * @struct drone
 * @brief Structure representing a rescue drone in the system
 * 
 * Contains all information needed to track and control a drone,
 * including position, mission status, network connection details,
 * and synchronization primitives for thread-safe access.
 * 
 * **Thread Safety:**
 * - Each drone has its own mutex for property protection
 * - Network operations use separate socket mutex
 * - Status updates are atomic with proper locking
 * 
 * **Network Integration:**
 * - Local drones: socket = -1, managed by simulation
 * - Remote drones: socket > 0, connected via TCP/IP
 * 
 * @note All coordinate updates must be validated against map boundaries
 * @warning Always lock the mutex before accessing/modifying drone properties
 */
typedef struct drone {
    int id;                     /**< Unique identifier for this drone (auto-assigned) */
    pthread_t thread_id;        /**< Thread ID for drone's operation handler */
    DroneStatus status;         /**< Current operational status */
    Coord coord;                /**< Current position on the map grid */
    Coord target;               /**< Target coordinates for current mission */
    struct tm last_update;      /**< Timestamp of last communication or status update */
    pthread_mutex_t lock;       /**< Mutex for thread-safe property access */
    int socket;                 /**< Network socket for client communication (-1 for local drones) */
} Drone;

/**
 * @brief Global list of all active drones in the system
 * 
 * Thread-safe list containing both local simulation drones and
 * networked client drones. Managed by the drone server and
 * accessed by the AI controller for mission assignment.
 */
extern struct list *drones;

/**
 * @brief Total number of drones in the fleet
 * 
 * Tracks the current count of active drones for statistics
 * and resource management. Updated when drones connect/disconnect.
 */
extern int num_drones;

/**
 * @defgroup drone_server Drone Server Functions
 * @brief TCP/IP server for handling drone client connections
 * @{
 */

/**
 * @brief Main server thread function for drone client connections
 * 
 * Creates a TCP socket server that listens on the configured port
 * for incoming drone client connections. For each connection:
 * 1. Accepts the client socket
 * 2. Spawns a new thread to handle the client
 * 3. Continues listening for more connections
 * 
 * **Protocol Handling:**
 * - Handshake negotiation and validation
 * - Client registration and ID assignment
 * - Thread creation for ongoing communication
 * 
 * **Error Handling:**
 * - Network failures are logged and connection retried
 * - Invalid clients are rejected gracefully
 * - Resource cleanup on thread termination
 * 
 * @param arg Unused thread parameter (required for pthread compatibility)
 * @return NULL when server thread terminates
 * 
 * @note This function runs continuously until system shutdown
 * @warning Server socket must be properly closed during cleanup
 * 
 * @see handle_drone_client() for per-client communication
 */
void* drone_server(void *arg);

/**
 * @brief Handle communication with a connected drone client
 * 
 * Manages the complete lifecycle of a drone client connection:
 * 1. **Handshake Processing**: Validates client and exchanges configuration
 * 2. **Registration**: Adds drone to global list with assigned ID
 * 3. **Message Loop**: Processes ongoing communication until disconnect
 * 4. **Cleanup**: Removes drone and frees resources
 * 
 * **Message Types Processed:**
 * - STATUS_UPDATE: Drone position and status changes
 * - MISSION_COMPLETE: Notification of successful rescue
 * - HEARTBEAT_RESPONSE: Keep-alive acknowledgments
 * 
 * **Thread Safety:**
 * - Each client has its own handler thread
 * - Proper mutex usage for shared data access
 * - Graceful handling of concurrent operations
 * 
 * @param arg Pointer to client socket descriptor (dynamically allocated)
 * @return NULL when client handler terminates
 * 
 * @note The socket pointer is freed by this function
 * @warning Client disconnection must be handled gracefully
 * 
 * @see drone_server() for server setup
 * @see update_drone_status() for mission completion handling
 */
void *handle_drone_client(void *arg);

/** @} */ // end of drone_server group

/**
 * @defgroup drone_management Drone Lifecycle Management
 * @brief Functions for drone initialization, update, and cleanup
 * @{
 */

/**
 * @brief Update drone status after mission completion
 * 
 * Called when a drone reports successful mission completion.
 * Updates the corresponding survivor's status to "rescued" and
 * sets appropriate timestamps for tracking purposes.
 * 
 * **Operations Performed:**
 * 1. Locate survivor at mission target coordinates
 * 2. Verify survivor is in "being helped" state
 * 3. Update survivor status to "rescued"
 * 4. Record rescue timestamp
 * 5. Update mission statistics
 * 
 * **Thread Safety:**
 * - Uses survivor array mutex for data protection
 * - Atomic status updates to prevent race conditions
 * 
 * @param drone Pointer to drone that completed the mission
 * @param target Coordinates where the mission was completed
 * 
 * @pre drone != NULL && target != NULL
 * @pre drone status has been set to IDLE by caller
 * @post Survivor at target location is marked as rescued
 * 
 * @note If no matching survivor is found, a warning is logged
 * @see assign_mission() for mission initiation
 */
void update_drone_status(Drone *drone, Coord *target);

/**
 * @brief Clean up all drone resources during system shutdown
 * 
 * Performs comprehensive cleanup of the drone system:
 * - Cancels all drone operation threads
 * - Closes network sockets for connected clients
 * - Destroys drone-specific mutexes
 * - Records final statistics
 * 
 * **Cleanup Order:**
 * 1. Signal all threads to terminate
 * 2. Wait for thread completion (with timeout)
 * 3. Close network connections
 * 4. Destroy synchronization objects
 * 5. Free allocated memory
 * 
 * @note Should be called during system shutdown sequence
 * @warning This function will force-terminate unresponsive threads
 * 
 * @see initialize_drones() for system initialization
 */
void cleanup_drones(void);

/** @} */ // end of drone_management group

/**
 * @defgroup drone_constants Drone System Constants
 * @brief Configuration constants for drone operations
 * @{
 */

/** @brief Default TCP port for drone server */
#define DRONE_SERVER_PORT 8080

/** @brief Maximum size for JSON messages */
#define DRONE_MESSAGE_SIZE 4096

/** @brief Maximum number of pending connections */
#define MAX_PENDING_CONNECTIONS 10

/** @brief Timeout for socket operations (seconds) */
#define SOCKET_TIMEOUT_SEC 30

/** @brief Heartbeat interval for connected drones (seconds) */
#define DRONE_HEARTBEAT_INTERVAL 10

/** @} */ // end of drone_constants group

#endif // DRONE_H