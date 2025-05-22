#ifndef SERVER_THROUGHPUT_H
#define SERVER_THROUGHPUT_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

// Enhanced performance metrics structure
typedef struct {
    // Message counts
    unsigned long status_updates_received;
    unsigned long missions_assigned;
    unsigned long heartbeats_sent;
    unsigned long messages_processed;
    unsigned long error_count;
    
    // Data throughput (bytes)
    unsigned long total_bytes_received;
    unsigned long total_bytes_sent;
    
    // Connection metrics
    unsigned long active_connections;
    unsigned long total_connections;
    unsigned long disconnections;
    
    // Response time tracking
    double total_response_time_ms;
    unsigned long response_count;
    double max_response_time_ms;
    double min_response_time_ms;
    
    // Time tracking
    struct timespec start_time;
    struct timespec current_time;
    
    // Peak tracking
    unsigned long peak_messages_per_second;
    unsigned long peak_connections;
    
    // Synchronization
    pthread_mutex_t metrics_lock;
    
    // Flags
    int is_monitoring;
    
    // Log file
    FILE* log_file;
} PerfMetrics;

// Global metrics instance declaration
extern PerfMetrics metrics;

// Function prototypes
void init_perf_monitor(const char* log_filename);
void perf_record_status_update(size_t bytes_received);
void perf_record_mission_assigned(size_t bytes_sent);
void perf_record_heartbeat(size_t bytes_sent);
void perf_record_error(void);
void perf_record_connection(int is_new);
void perf_record_response_time(double response_time_ms);
double get_elapsed_seconds(void);
void log_perf_metrics(void);
void log_perf_metrics_to_file(void);
void* perf_monitor_thread(void* arg);
pthread_t start_perf_monitor(const char* log_filename);
void stop_perf_monitor(pthread_t monitor_thread);
void export_metrics_json(const char* filename);

#endif // SERVER_THROUGHPUT_H