/**
 * @file server_throughput.h
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @brief Comprehensive performance monitoring system for drone server operations
 * @version 0.1
 * @date 2025-05-22
 * 
 * This header defines a complete performance monitoring and metrics collection
 * system for the drone coordination server. It provides real-time tracking of
 * message throughput, response times, connection statistics, and system performance
 * with both console output and CSV logging capabilities.
 * 
 * **Key Features:**
 * - Real-time message throughput monitoring
 * - Response time tracking with min/max/average calculations
 * - Connection lifecycle monitoring
 * - Data transfer volume tracking (bytes sent/received)
 * - Peak performance detection and recording
 * - Multi-format output (console, CSV, JSON)
 * - Thread-safe metrics collection
 * 
 * **Use Cases:**
 * - System performance optimization
 * - Bottleneck identification
 * - Capacity planning
 * - Service level monitoring
 * - Debugging network issues
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup core_modules
 * @ingroup monitoring
 */

#ifndef SERVER_THROUGHPUT_H
#define SERVER_THROUGHPUT_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

/**
 * @defgroup monitoring Performance Monitoring System
 * @brief Real-time performance tracking and analysis
 * @ingroup core_modules
 * @{
 */

/**
 * @defgroup metrics_structures Performance Metrics Data Structures
 * @brief Core data types for performance measurement
 * @{
 */

/**
 * @struct PerfMetrics
 * @brief Comprehensive structure for tracking all system performance metrics
 * 
 * This structure serves as the central repository for all performance data
 * collected during system operation. It includes counters for different
 * message types, timing information, connection statistics, and peak
 * performance indicators.
 * 
 * **Thread Safety:**
 * All fields are protected by the internal metrics_lock mutex. Always
 * acquire this lock before accessing or modifying any metric values
 * to prevent race conditions in multi-threaded environments.
 * 
 * **Data Categories:**
 * - Message Counts: Different types of messages processed
 * - Throughput: Bytes transferred in both directions
 * - Connections: Active and historical connection tracking
 * - Response Times: Latency measurements and statistics
 * - Peak Performance: Maximum values achieved
 * 
 * @note All counters use unsigned long for maximum range
 * @warning Direct field access should be avoided - use provided functions
 */
typedef struct {
    /** @name Message Processing Counters
     *  Tracking different types of messages handled by the server
     *  @{
     */
    unsigned long status_updates_received; /**< Number of drone status update messages received */
    unsigned long missions_assigned;       /**< Number of mission assignment messages sent to drones */
    unsigned long heartbeats_sent;         /**< Number of heartbeat messages sent to maintain connections */
    unsigned long messages_processed;      /**< Total number of messages processed (all types combined) */
    unsigned long error_count;             /**< Number of errors encountered during operation */
    /** @} */

    /** @name Data Throughput Tracking
     *  Monitoring network bandwidth utilization
     *  @{
     */
    unsigned long total_bytes_received; /**< Total bytes received from drone clients */
    unsigned long total_bytes_sent;     /**< Total bytes sent to drone clients */
    /** @} */

    /** @name Connection Management
     *  Tracking drone client connections over time
     *  @{
     */
    unsigned long active_connections; /**< Currently active drone connections */
    unsigned long total_connections;  /**< Total number of connections established since startup */
    unsigned long disconnections;     /**< Number of connections that have been closed */
    /** @} */

    /** @name Response Time Analysis
     *  Measuring system responsiveness and latency
     *  @{
     */
    double total_response_time_ms; /**< Cumulative response time for averaging calculations */
    unsigned long response_count;  /**< Number of response time measurements taken */
    double max_response_time_ms;   /**< Maximum response time observed */
    double min_response_time_ms;   /**< Minimum response time observed */
    /** @} */

    /** @name Timing Infrastructure
     *  Time tracking for performance calculations
     *  @{
     */
    struct timespec start_time;   /**< System startup timestamp for elapsed time calculations */
    struct timespec current_time; /**< Most recent timestamp (updated periodically) */
    /** @} */

    /** @name Peak Performance Tracking
     *  Recording maximum performance levels achieved
     *  @{
     */
    unsigned long peak_messages_per_second; /**< Highest message processing rate observed */
    unsigned long peak_connections;         /**< Maximum number of simultaneous connections */
    /** @} */

    /** @name Synchronization and Control
     *  Thread safety and monitoring control
     *  @{
     */
    pthread_mutex_t metrics_lock; /**< Mutex protecting all metric fields from concurrent access */
    int is_monitoring;            /**< Flag controlling whether monitoring thread continues running */
    /** @} */

    /** @name Output and Logging
     *  File handling for persistent metrics storage
     *  @{
     */
    // clang-format off
    FILE* log_file;                         /**< File handle for CSV output logging (NULL if disabled) */
    // clang-format on
    /** @} */
} PerfMetrics;

/** @} */ // end of metrics_structures group

/**
 * @defgroup metrics_globals Global Metrics Management
 * @brief Global variables for system-wide performance tracking
 * @{
 */

/** 
 * @brief Global performance metrics instance
 * 
 * Single global instance that collects all performance data for the
 * entire system. This centralized approach ensures consistent data
 * collection and simplifies access patterns across different modules.
 * 
 * **Initialization:**
 * Must be initialized by calling init_perf_monitor() before use.
 * The structure is zero-initialized at startup.
 * 
 * **Access Pattern:**
 * Always use the provided recording functions rather than direct
 * field access to ensure proper thread synchronization.
 * 
 * @note Global scope allows access from any system module
 * @warning Must be properly initialized before use
 */
extern PerfMetrics metrics;

/** @} */ // end of metrics_globals group

/**
 * @defgroup metrics_initialization Metrics System Initialization
 * @brief Functions for setting up performance monitoring
 * @{
 */

/**
 * @brief Initialize performance monitoring system with optional CSV logging
 * 
 * Sets up the complete performance monitoring infrastructure including
 * mutex initialization, timestamp recording, and optional file logging.
 * This function must be called before any metric recording operations.
 * 
 * **Initialization Steps:**
 * 1. Initialize metrics_lock mutex for thread safety
 * 2. Record system startup timestamp
 * 3. Set monitoring active flag
 * 4. Initialize min_response_time to high value
 * 5. Open CSV log file if filename provided
 * 6. Write CSV header if file opened successfully
 * 
 * **CSV Log Format:**
 * The CSV file includes timestamp, elapsed time, message counts,
 * throughput rates, connection statistics, and response time metrics.
 * 
 * @param log_filename Path to CSV log file (can be NULL to disable file logging)
 * 
 * @pre None - can be called on uninitialized system
 * @post Performance monitoring system is ready for operation
 * 
 * @note File logging is optional - pass NULL to disable
 * @note CSV file includes descriptive header row
 * 
 * @see start_perf_monitor() for automated background monitoring
 */
// clang-format off
void init_perf_monitor(const char* log_filename);

/** @} */ // end of metrics_initialization group

/**
 * @defgroup metrics_recording Metrics Recording Functions
 * @brief Thread-safe functions for recording performance events
 * @{
 */

/**
 * @brief Record a status update message with byte count tracking
 * 
 * Thread-safe function to increment the status update counter and
 * track associated data transfer. Used when receiving position or
 * status information from drone clients.
 * 
 * **Metrics Updated:**
 * - status_updates_received counter
 * - messages_processed counter  
 * - total_bytes_received accumulator
 * 
 * @param bytes_received Size of the received message in bytes
 * 
 * @pre Performance monitoring must be initialized
 * @post Status update metrics are incremented atomically
 * 
 * @note Thread-safe through internal mutex locking
 * @see perf_record_mission_assigned() for outbound messages
 */
void perf_record_status_update(size_t bytes_received);

/**
 * @brief Record a mission assignment with byte count tracking
 * 
 * Thread-safe function to track outbound mission assignment messages
 * sent to drone clients. Includes data transfer volume monitoring
 * for bandwidth analysis.
 * 
 * **Metrics Updated:**
 * - missions_assigned counter
 * - messages_processed counter
 * - total_bytes_sent accumulator
 * 
 * @param bytes_sent Size of the sent message in bytes
 * 
 * @pre Performance monitoring must be initialized
 * @post Mission assignment metrics are incremented atomically
 * 
 * @note Thread-safe through internal mutex locking
 * @see perf_record_status_update() for inbound messages
 */
void perf_record_mission_assigned(size_t bytes_sent);

/**
 * @brief Record a heartbeat message with byte count tracking
 * 
 * Thread-safe function to track heartbeat messages sent to maintain
 * connections with drone clients. Heartbeats are critical for
 * connection management and their frequency affects network utilization.
 * 
 * **Metrics Updated:**
 * - heartbeats_sent counter
 * - messages_processed counter
 * - total_bytes_sent accumulator
 * 
 * @param bytes_sent Size of the sent heartbeat message in bytes
 * 
 * @pre Performance monitoring must be initialized
 * @post Heartbeat metrics are incremented atomically
 * 
 * @note Thread-safe through internal mutex locking
 * @note Heartbeats typically generate consistent, small message sizes
 */
void perf_record_heartbeat(size_t bytes_sent);

/**
 * @brief Record an error occurrence in system operation
 * 
 * Thread-safe function to increment the error counter when system
 * failures, network issues, or other problems are detected. Error
 * tracking is essential for system reliability monitoring.
 * 
 * **Metrics Updated:**
 * - error_count counter
 * 
 * @pre Performance monitoring must be initialized
 * @post Error count is incremented atomically
 * 
 * @note Thread-safe through internal mutex locking
 * @note Should be called for all significant error conditions
 */
void perf_record_error(void);

/**
 * @brief Record connection lifecycle events
 * 
 * Thread-safe function to track drone client connections and
 * disconnections. Maintains counts of active connections and
 * historical connection statistics including peak concurrent usage.
 * 
 * **For New Connections (is_new = 1):**
 * - Increments active_connections
 * - Increments total_connections
 * - Updates peak_connections if new maximum reached
 * 
 * **For Disconnections (is_new = 0):**
 * - Decrements active_connections (with bounds checking)
 * - Increments disconnections counter
 * 
 * @param is_new 1 for new connection establishment, 0 for disconnection
 * 
 * @pre Performance monitoring must be initialized
 * @post Connection metrics are updated atomically
 * 
 * @note Thread-safe through internal mutex locking
 * @note Handles connection count bounds checking automatically
 */
void perf_record_connection(int is_new);

/**
 * @brief Record response time measurement for latency analysis
 * 
 * Thread-safe function to track message response times for system
 * performance analysis. Maintains running statistics including
 * average, minimum, and maximum response times observed.
 * 
 * **Metrics Updated:**
 * - total_response_time_ms accumulator (for average calculation)
 * - response_count (number of measurements)
 * - max_response_time_ms (if new maximum)
 * - min_response_time_ms (if new minimum)
 * 
 * @param response_time_ms Response time measurement in milliseconds
 * 
 * @pre Performance monitoring must be initialized
 * @pre response_time_ms should be positive
 * @post Response time statistics are updated atomically
 * 
 * @note Thread-safe through internal mutex locking
 * @note Used for measuring end-to-end message processing latency
 * 
 * @see Mission assignment, status processing, and heartbeat response times
 */
void perf_record_response_time(double response_time_ms);

/** @} */ // end of metrics_recording group

/**
 * @defgroup metrics_reporting Metrics Reporting and Output
 * @brief Functions for displaying and exporting performance data
 * @{
 */

/**
 * @brief Calculate elapsed time since monitoring began
 * 
 * Utility function to compute the total runtime of the monitoring
 * system. Used for calculating rates and averages that require
 * time-based normalization.
 * 
 * @return Elapsed time in seconds as floating-point value
 * 
 * @pre Performance monitoring must be initialized
 * @post Returns accurate elapsed time measurement
 * 
 * @note Uses high-resolution CLOCK_MONOTONIC for accuracy
 * @note Result includes fractional seconds for precision
 */
double get_elapsed_seconds(void);

/**
 * @brief Display comprehensive performance metrics to console
 * 
 * Outputs a formatted summary of all current performance metrics
 * to the console. Includes calculated rates, averages, and peak
 * values for easy human interpretation.
 * 
 * **Display Sections:**
 * - System uptime and message throughput rates
 * - Breakdown by message type (status, missions, heartbeats)
 * - Connection statistics and peak usage
 * - Data transfer volume in KB
 * - Response time statistics (min/avg/max)
 * 
 * @pre Performance monitoring must be initialized
 * @post Formatted metrics displayed on console
 * 
 * @note Thread-safe through internal mutex locking
 * @note Calculations performed at display time for current accuracy
 * 
 * @see log_perf_metrics_to_file() for persistent logging
 */
void log_perf_metrics(void);

/**
 * @brief Append current metrics to CSV log file
 * 
 * Writes a timestamped line of metrics data to the configured CSV
 * log file. Provides persistent storage of performance data for
 * historical analysis and trend identification.
 * 
 * **CSV Columns:**
 * timestamp, elapsed_seconds, total_messages, msg_per_sec,
 * status_updates, missions, heartbeats, errors, active_connections,
 * total_bytes_rx, total_bytes_tx, avg_response_ms, max_response_ms,
 * peak_msg_per_sec
 * 
 * @pre Performance monitoring must be initialized
 * @pre CSV log file must be open (log_filename provided to init)
 * @post New data line appended to CSV file with automatic flushing
 * 
 * @note Does nothing if no log file is configured
 * @note Thread-safe through internal mutex locking
 * @note File is flushed immediately for data persistence
 */
void log_perf_metrics_to_file(void);

/** @} */ // end of metrics_reporting group

/**
 * @defgroup metrics_automation Automated Monitoring
 * @brief Functions for background performance monitoring
 * @{
 */

/**
 * @brief Background thread function for periodic metrics logging
 * 
 * Main function for the performance monitoring thread that runs
 * continuously in the background, logging metrics at regular
 * intervals to both console and CSV file.
 * 
 * **Operation:**
 * - Logs metrics every 5 seconds while monitoring is active
 * - Calls both console and file logging functions
 * - Continues until is_monitoring flag is cleared
 * - Provides consistent performance visibility
 * 
 * @param arg Unused thread parameter (required for pthread compatibility)
 * @return NULL when monitoring thread terminates
 * 
 * @pre Performance monitoring must be initialized
 * @post Continuous metrics logging until termination
 * 
 * @note Runs in separate thread for non-blocking operation
 * @note Sleep interval is fixed at 5 seconds
 * 
 * @see start_perf_monitor() for thread creation
 * @see stop_perf_monitor() for thread termination
 */
void* perf_monitor_thread(void* arg);

/**
 * @brief Start automated performance monitoring in background thread
 * 
 * Initializes the performance monitoring system and creates a
 * background thread for continuous metrics logging. This provides
 * automated performance visibility without manual intervention.
 * 
 * **Setup Process:**
 * 1. Initialize performance monitoring system
 * 2. Create background monitoring thread
 * 3. Return thread ID for later management
 * 
 * @param log_filename Path to CSV log file (can be NULL for console-only)
 * @return Thread ID of monitoring thread, or 0 on failure
 * 
 * @pre None - can be called on uninitialized system
 * @post Background performance monitoring is active
 * 
 * @note Thread ID must be saved for proper shutdown
 * @note Failure returns 0 and logs error message
 * 
 * @see stop_perf_monitor() for proper shutdown
 */
pthread_t start_perf_monitor(const char* log_filename);

/**
 * @brief Stop background monitoring and display final report
 * 
 * Terminates the performance monitoring thread and outputs
 * comprehensive final statistics. Ensures proper cleanup of
 * all monitoring resources including file handles and mutexes.
 * 
 * **Shutdown Process:**
 * 1. Clear is_monitoring flag to signal thread termination
 * 2. Wait for monitoring thread to complete (pthread_join)
 * 3. Display final performance report
 * 4. Close CSV log file if open
 * 5. Destroy metrics mutex
 * 
 * @param monitor_thread Thread ID returned by start_perf_monitor()
 * 
 * @pre monitor_thread must be valid thread ID or 0
 * @post All monitoring resources are cleaned up
 * 
 * @note Safe to call with 0 thread ID (no-op)
 * @note Displays final comprehensive performance report
 * @note Closes log file and releases all resources
 */
void stop_perf_monitor(pthread_t monitor_thread);

/** @} */ // end of metrics_automation group

/**
 * @defgroup metrics_export Data Export Functions
 * @brief Functions for exporting metrics in various formats
 * @{
 */

/**
 * @brief Export complete metrics data to JSON format
 * 
 * Creates a comprehensive JSON file containing all current performance
 * metrics in a structured format suitable for external analysis tools,
 * dashboards, or automated processing systems.
 * 
 * **JSON Structure:**
 * ```json
 * {
 *   "server_metrics": {
 *     "uptime_seconds": float,
 *     "total_messages": int,
 *     "messages_per_second": float,
 *     "peak_messages_per_second": int,
 *     "status_updates": int,
 *     "missions_assigned": int,
 *     "heartbeats_sent": int,
 *     "errors": int,
 *     "active_connections": int,
 *     "total_connections": int,
 *     "peak_connections": int,
 *     "bytes_received": int,
 *     "bytes_sent": int,
 *     "avg_response_time_ms": float,
 *     "max_response_time_ms": float,
 *     "min_response_time_ms": float
 *   }
 * }
 * ```
 * 
 * @param filename Path to output JSON file
 * 
 * @pre Performance monitoring must be initialized
 * @pre filename must be valid writable path
 * @post JSON file created with current metrics snapshot
 * 
 * @note Thread-safe through internal mutex locking
 * @note Overwrites existing files with same name
 * @note Handles file creation errors gracefully
 * 
 * @see Standard JSON format for maximum compatibility
 */
void export_metrics_json(const char* filename);
// clang-format on

/** @} */ // end of metrics_export group

/** @} */ // end of monitoring group

#endif // SERVER_THROUGHPUT_H