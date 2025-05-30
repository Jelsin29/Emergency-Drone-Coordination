/**
 * @file server_throughput.c
 * @brief Performance monitoring system implementation for drone coordination server
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @version 0.1
 * @date 2025-05-22
 * 
 * This module implements a comprehensive performance monitoring system that
 * tracks all aspects of server operation including message throughput,
 * response times, connection statistics, and system performance metrics.
 * It provides real-time monitoring with multiple output formats for analysis.
 * 
 * **Monitoring Capabilities:**
 * - Real-time message throughput tracking (messages per second)
 * - Response time analysis with min/max/average calculations
 * - Connection lifecycle monitoring (connects, disconnects, peak)
 * - Data transfer volume tracking (bytes sent/received)
 * - Error rate monitoring and peak performance detection
 * - Multi-threaded data collection with atomic operations
 * 
 * **Output Formats:**
 * - Real-time console output with formatted statistics
 * - CSV logging for historical analysis and trending
 * - JSON export for integration with external tools
 * - Configurable logging intervals and detail levels
 * 
 * **Thread Safety:**
 * - Mutex-protected metrics structure for concurrent access
 * - Atomic counter updates from multiple threads
 * - Background monitoring thread for continuous logging
 * - Safe initialization and cleanup procedures
 * 
 * **Performance Impact:**
 * - Minimal overhead through efficient data structures
 * - Lock-free increments where possible
 * - Background logging to avoid blocking main operations
 * - Configurable monitoring granularity
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup monitoring
 * @ingroup core_modules
 */

#define _POSIX_C_SOURCE 199309L
#include "headers/server_throughput.h"

// Global metrics instance definition
PerfMetrics metrics = { 0 };

/**
 * @brief Initialize performance monitoring with optional log file
 * 
 * Sets up the metrics structure and creates a CSV log file if a filename is provided
 * 
 * @param log_filename Path to CSV log file (can be NULL for no logging)
 */
void init_perf_monitor(const char *log_filename)
{
    pthread_mutex_init(&metrics.metrics_lock, NULL);
    clock_gettime(CLOCK_MONOTONIC, &metrics.start_time);
    metrics.is_monitoring = 1;
    metrics.min_response_time_ms = 999999.0; // Initialize to high value
    metrics.log_file = NULL;

    if (log_filename)
    {
        metrics.log_file = fopen(log_filename, "w");
        if (metrics.log_file)
        {
            // Write CSV header
            fprintf(
                metrics.log_file,
                "timestamp,elapsed_seconds,total_messages,msg_per_sec,status_updates,missions,heartbeats,errors,active_"
                "connections,total_bytes_rx,total_bytes_tx,avg_response_ms,max_response_ms,peak_msg_per_sec\n");
            fflush(metrics.log_file);
        }
    }
}

/**
 * @brief Increment status update count with byte tracking
 * 
 * Thread-safe function to record a status update from a drone
 * 
 * @param bytes_received Size of the received message in bytes
 */
void perf_record_status_update(size_t bytes_received)
{
    pthread_mutex_lock(&metrics.metrics_lock);
    metrics.status_updates_received++;
    metrics.messages_processed++;
    metrics.total_bytes_received += bytes_received;
    pthread_mutex_unlock(&metrics.metrics_lock);
}

/**
 * @brief Increment mission assignment count with byte tracking
 * 
 * Thread-safe function to record a mission assignment to a drone
 * 
 * @param bytes_sent Size of the sent message in bytes
 */
void perf_record_mission_assigned(size_t bytes_sent)
{
    pthread_mutex_lock(&metrics.metrics_lock);
    metrics.missions_assigned++;
    metrics.messages_processed++;
    metrics.total_bytes_sent += bytes_sent;
    pthread_mutex_unlock(&metrics.metrics_lock);
}

/**
 * @brief Increment heartbeat count with byte tracking
 * 
 * Thread-safe function to record a heartbeat sent to a drone
 * 
 * @param bytes_sent Size of the sent message in bytes
 */
void perf_record_heartbeat(size_t bytes_sent)
{
    pthread_mutex_lock(&metrics.metrics_lock);
    metrics.heartbeats_sent++;
    metrics.messages_processed++;
    metrics.total_bytes_sent += bytes_sent;
    pthread_mutex_unlock(&metrics.metrics_lock);
}

/**
 * @brief Record error occurrences
 * 
 * Thread-safe function to increment the error counter
 */
void perf_record_error(void)
{
    pthread_mutex_lock(&metrics.metrics_lock);
    metrics.error_count++;
    pthread_mutex_unlock(&metrics.metrics_lock);
}

/**
 * @brief Record connection events
 * 
 * Thread-safe function to track drone connections and disconnections
 * 
 * @param is_new 1 for new connection, 0 for disconnection
 */
void perf_record_connection(int is_new)
{
    pthread_mutex_lock(&metrics.metrics_lock);
    if (is_new)
    {
        metrics.active_connections++;
        metrics.total_connections++;
        if (metrics.active_connections > metrics.peak_connections)
        {
            metrics.peak_connections = metrics.active_connections;
        }
    }
    else
    {
        if (metrics.active_connections > 0)
        {
            metrics.active_connections--;
        }
        metrics.disconnections++;
    }
    pthread_mutex_unlock(&metrics.metrics_lock);
}

/**
 * @brief Record response time for latency tracking
 * 
 * Thread-safe function to track message response times
 * 
 * @param response_time_ms Response time in milliseconds
 */
void perf_record_response_time(double response_time_ms)
{
    pthread_mutex_lock(&metrics.metrics_lock);
    metrics.total_response_time_ms += response_time_ms;
    metrics.response_count++;

    if (response_time_ms > metrics.max_response_time_ms)
    {
        metrics.max_response_time_ms = response_time_ms;
    }
    if (response_time_ms < metrics.min_response_time_ms)
    {
        metrics.min_response_time_ms = response_time_ms;
    }
    pthread_mutex_unlock(&metrics.metrics_lock);
}

/**
 * @brief Get elapsed time in seconds since monitoring started
 * 
 * @return Elapsed time in seconds as a double
 */
double get_elapsed_seconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - metrics.start_time.tv_sec) + (now.tv_nsec - metrics.start_time.tv_nsec) / 1000000000.0;
}

/**
 * @brief Log performance metrics to console
 * 
 * Displays a formatted summary of all current performance metrics
 */
void log_perf_metrics(void)
{
    pthread_mutex_lock(&metrics.metrics_lock);

    double elapsed = get_elapsed_seconds();
    double msg_rate = elapsed > 0 ? metrics.messages_processed / elapsed : 0;
    double avg_response = metrics.response_count > 0 ? metrics.total_response_time_ms / metrics.response_count : 0;

    // Update peak message rate
    unsigned long current_rate = (unsigned long)msg_rate;
    if (current_rate > metrics.peak_messages_per_second)
    {
        metrics.peak_messages_per_second = current_rate;
    }

    printf("\n===== SERVER THROUGHPUT METRICS =====\n");
    printf("Duration: %.2f seconds\n", elapsed);
    printf("Messages: %lu total (%.2f msgs/sec, peak: %lu msgs/sec)\n",
           metrics.messages_processed,
           msg_rate,
           metrics.peak_messages_per_second);
    printf("  - Status updates: %lu (%.2f/sec)\n",
           metrics.status_updates_received,
           elapsed > 0 ? metrics.status_updates_received / elapsed : 0);
    printf("  - Missions assigned: %lu (%.2f/sec)\n",
           metrics.missions_assigned,
           elapsed > 0 ? metrics.missions_assigned / elapsed : 0);
    printf("  - Heartbeats sent: %lu (%.2f/sec)\n",
           metrics.heartbeats_sent,
           elapsed > 0 ? metrics.heartbeats_sent / elapsed : 0);
    printf("  - Errors: %lu\n", metrics.error_count);

    printf("Connections: %lu active, %lu total, %lu disconnected (peak: %lu)\n",
           metrics.active_connections,
           metrics.total_connections,
           metrics.disconnections,
           metrics.peak_connections);

    printf("Data Transfer: %.2f KB received, %.2f KB sent\n",
           metrics.total_bytes_received / 1024.0,
           metrics.total_bytes_sent / 1024.0);

    if (metrics.response_count > 0)
    {
        printf("Response Times: avg %.2fms, min %.2fms, max %.2fms\n",
               avg_response,
               metrics.min_response_time_ms,
               metrics.max_response_time_ms);
    }

    printf("======================================\n\n");

    pthread_mutex_unlock(&metrics.metrics_lock);
}

/**
 * @brief Log performance metrics to CSV file
 * 
 * Appends a line with current metrics to the CSV log file if one is open
 */
void log_perf_metrics_to_file(void)
{
    if (!metrics.log_file)
        return;

    pthread_mutex_lock(&metrics.metrics_lock);

    double elapsed = get_elapsed_seconds();
    double msg_rate = elapsed > 0 ? metrics.messages_processed / elapsed : 0;
    double avg_response = metrics.response_count > 0 ? metrics.total_response_time_ms / metrics.response_count : 0;

    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(metrics.log_file,
            "%s,%.2f,%lu,%.2f,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%.2f,%.2f,%lu\n",
            timestamp,
            elapsed,
            metrics.messages_processed,
            msg_rate,
            metrics.status_updates_received,
            metrics.missions_assigned,
            metrics.heartbeats_sent,
            metrics.error_count,
            metrics.active_connections,
            metrics.total_bytes_received,
            metrics.total_bytes_sent,
            avg_response,
            metrics.max_response_time_ms,
            metrics.peak_messages_per_second);

    fflush(metrics.log_file);

    pthread_mutex_unlock(&metrics.metrics_lock);
}

/**
 * @brief Performance monitor thread function
 * 
 * Periodically logs metrics to both console and CSV file
 * 
 * @param arg Unused thread parameter
 * @return NULL when thread terminates
 */
// clang-format off
void* perf_monitor_thread(void* arg)
// clang-format on
{
    (void)arg; // Suppress unused parameter warning

    while (metrics.is_monitoring)
    {
        sleep(5); // Log every 5 seconds
        log_perf_metrics();
        log_perf_metrics_to_file();
    }
    return NULL;
}

/**
 * @brief Start the performance monitor in a separate thread
 * 
 * Initializes the metrics system and starts a background thread for logging
 * 
 * @param log_filename Path to CSV log file (can be NULL for no logging)
 * @return Thread ID of the monitor thread, or 0 on failure
 */
pthread_t start_perf_monitor(const char *log_filename)
{
    pthread_t monitor_thread;
    init_perf_monitor(log_filename);

    if (pthread_create(&monitor_thread, NULL, perf_monitor_thread, NULL) != 0)
    {
        fprintf(stderr, "Error: Failed to create performance monitor thread\n");
        return 0;
    }

    printf("Performance monitoring started%s\n", log_filename ? " with CSV logging" : "");

    return monitor_thread;
}

/**
 * @brief Stop performance monitoring and log final metrics
 * 
 * Terminates the monitor thread and outputs final statistics
 * 
 * @param monitor_thread Thread ID of the monitor thread
 */
void stop_perf_monitor(pthread_t monitor_thread)
{
    if (monitor_thread == 0)
        return;

    metrics.is_monitoring = 0;
    pthread_join(monitor_thread, NULL);

    printf("\n===== FINAL PERFORMANCE REPORT =====\n");
    log_perf_metrics();

    if (metrics.log_file)
    {
        fclose(metrics.log_file);
        metrics.log_file = NULL;
    }

    pthread_mutex_destroy(&metrics.metrics_lock);
}

/**
 * @brief Export metrics to JSON format for external analysis
 * 
 * Creates a JSON file with all current metrics for easier processing
 * 
 * @param filename Path to output JSON file
 */
void export_metrics_json(const char *filename)
{
    if (!filename)
        return;
    // clang-format off
    FILE* json_file = fopen(filename, "w");
    // clang-format on
    if (!json_file)
    {
        fprintf(stderr, "Error: Could not create JSON file %s\n", filename);
        return;
    }

    pthread_mutex_lock(&metrics.metrics_lock);

    double elapsed = get_elapsed_seconds();
    double avg_response = metrics.response_count > 0 ? metrics.total_response_time_ms / metrics.response_count : 0;

    fprintf(json_file, "{\n");
    fprintf(json_file, "  \"server_metrics\": {\n");
    fprintf(json_file, "    \"uptime_seconds\": %.2f,\n", elapsed);
    fprintf(json_file, "    \"total_messages\": %lu,\n", metrics.messages_processed);
    fprintf(json_file, "    \"messages_per_second\": %.2f,\n", elapsed > 0 ? metrics.messages_processed / elapsed : 0);
    fprintf(json_file, "    \"peak_messages_per_second\": %lu,\n", metrics.peak_messages_per_second);
    fprintf(json_file, "    \"status_updates\": %lu,\n", metrics.status_updates_received);
    fprintf(json_file, "    \"missions_assigned\": %lu,\n", metrics.missions_assigned);
    fprintf(json_file, "    \"heartbeats_sent\": %lu,\n", metrics.heartbeats_sent);
    fprintf(json_file, "    \"errors\": %lu,\n", metrics.error_count);
    fprintf(json_file, "    \"active_connections\": %lu,\n", metrics.active_connections);
    fprintf(json_file, "    \"total_connections\": %lu,\n", metrics.total_connections);
    fprintf(json_file, "    \"peak_connections\": %lu,\n", metrics.peak_connections);
    fprintf(json_file, "    \"bytes_received\": %lu,\n", metrics.total_bytes_received);
    fprintf(json_file, "    \"bytes_sent\": %lu,\n", metrics.total_bytes_sent);
    fprintf(json_file, "    \"avg_response_time_ms\": %.2f,\n", avg_response);
    fprintf(json_file, "    \"max_response_time_ms\": %.2f,\n", metrics.max_response_time_ms);
    fprintf(json_file,
            "    \"min_response_time_ms\": %.2f\n",
            metrics.min_response_time_ms == 999999.0 ? 0.0 : metrics.min_response_time_ms);
    fprintf(json_file, "  }\n");
    fprintf(json_file, "}\n");

    pthread_mutex_unlock(&metrics.metrics_lock);

    fclose(json_file);
    printf("Metrics exported to %s\n", filename);
}