#include "../headers/server_throughput.h"
#include <unistd.h>

int main() {
    pthread_t monitor = start_perf_monitor("test_metrics.csv");
    
    // Simulate some server activity
    for (int i = 0; i < 100; i++) {
        perf_record_status_update(50);
        perf_record_heartbeat(25);
        usleep(10000); // 10ms delay
    }
    
    export_metrics_json("test_results.json");
    stop_perf_monitor(monitor);
    
    printf("Throughput test completed!\n");
    return 0;
}