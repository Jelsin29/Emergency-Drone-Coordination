/**
 * @file multi_drone_test.c
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @brief Multi-client stress testing program for drone coordination server
 * @version 0.1
 * @date 2025-05-22
 * 
 * This test program simulates a realistic emergency scenario by launching
 * multiple autonomous drone clients simultaneously to stress-test the
 * coordination server's ability to handle concurrent connections, message
 * processing, and mission assignments under load.
 * 
 * **Test Objectives:**
 * - Validate server capacity for concurrent drone connections
 * - Test network protocol robustness under high load
 * - Verify mission assignment algorithms with multiple active drones
 * - Stress-test thread safety of server components
 * - Measure system performance and identify bottlenecks
 * 
 * **Load Testing Features:**
 * - Configurable number of concurrent drone clients (default: 50)
 * - Staggered client launches to simulate realistic connection patterns
 * - Automatic process management with clean termination
 * - Signal handling for graceful shutdown of all clients
 * - Process monitoring and status reporting
 * 
 * **Realistic Simulation:**
 * - Each drone client operates as an independent autonomous entity
 * - Clients connect at different times (200ms intervals)
 * - Multiple drones compete for mission assignments
 * - Network communication stress-tests server threading
 * 
 * **Usage Scenarios:**
 * - Performance benchmarking with varying client counts
 * - Stress testing before production deployment
 * - Identifying memory leaks under sustained load
 * - Validating graceful degradation under resource constraints
 * 
 * @copyright Copyright (c) 2024
 * 
 * @ingroup testing
 * @ingroup load_testing
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <string.h>

/**
 * @defgroup load_testing Load and Stress Testing
 * @brief Programs for testing system performance under load
 * @ingroup testing
 * @{
 */

/**
 * @defgroup process_management Process Management for Testing
 * @brief Utilities for managing multiple test processes
 * @ingroup load_testing
 * @{
 */

/** 
 * @brief Default number of concurrent drone clients for stress testing
 * 
 * This default value provides a good balance between meaningful load
 * testing and resource consumption on typical development systems.
 * Can be overridden via command-line argument.
 */
#define DEFAULT_NUM_DRONES 50

/** 
 * @brief Inter-client launch delay in microseconds
 * 
 * Staggered launch timing prevents overwhelming the server with
 * simultaneous connection attempts, simulating more realistic
 * deployment scenarios where drones come online gradually.
 */
#define DELAY_USEC 200000 // 200ms between drone launches

/** 
 * @brief Array to track child process IDs for management
 * 
 * Maintains PIDs of all launched drone client processes to enable
 * proper cleanup and termination when test completes or is interrupted.
 */
pid_t *child_pids = NULL;

/** 
 * @brief Number of drone clients currently being managed
 * 
 * Tracks the total count of launched processes for iteration
 * during cleanup and status reporting operations.
 */
int num_drones = 0;

/**
 * @brief Signal handler for graceful cleanup of all child processes
 * 
 * Responds to termination signals (SIGINT, SIGTERM) by systematically
 * shutting down all launched drone client processes. Ensures clean
 * test termination without leaving orphaned processes.
 * 
 * **Cleanup Process:**
 * 1. Display termination message with signal information
 * 2. Iterate through all tracked child processes
 * 3. Send SIGTERM to each active drone client
 * 4. Log termination of each process for verification
 * 5. Free allocated memory for process tracking
 * 6. Exit cleanly with status 0
 * 
 * **Signal Handling:**
 * - SIGINT: User pressed Ctrl+C (interactive termination)
 * - SIGTERM: System or script requested termination
 * - Both signals trigger identical cleanup behavior
 * 
 * @param sig Signal number that triggered the handler
 * 
 * @pre child_pids array must be allocated and populated
 * @pre num_drones must reflect actual number of processes
 * @post All child processes receive termination signal
 * @post Process tracking memory is freed
 * @post Program exits cleanly
 * 
 * @note Does not wait for child process termination - relies on OS cleanup
 * @warning This function calls exit() and does not return
 */
void cleanup_handler(int sig) {
    printf("\nReceived signal %d. Terminating all drone clients...\n", sig);
    
    // Kill all child processes
    for (int i = 0; i < num_drones; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGTERM);
            printf("Terminated drone client %d with PID %d\n", i + 1, child_pids[i]);
        }
    }
    
    if (child_pids) {
        free(child_pids);
        child_pids = NULL;
    }
    
    printf("Cleanup completed. Exiting test program.\n");
    exit(0);
}

/**
 * @brief Main test orchestration function for multi-drone load testing
 * 
 * Coordinates the complete load testing process including argument parsing,
 * process creation, launch sequencing, and monitoring. Creates a realistic
 * multi-client scenario to validate server performance and stability.
 * 
 * **Test Execution Flow:**
 * 1. **Configuration**: Parse command-line arguments for drone count
 * 2. **Validation**: Verify drone client executable exists and is executable
 * 3. **Initialization**: Allocate process tracking structures
 * 4. **Signal Setup**: Configure handlers for clean termination
 * 5. **Launch Phase**: Fork and execute drone clients with staggered timing
 * 6. **Monitoring**: Track all processes and wait for completion
 * 7. **Cleanup**: Handle process termination and resource deallocation
 * 
 * **Command-Line Usage:**
 * ```
 * ./multi_drone_test [number_of_drones]
 * ```
 * - number_of_drones: Optional integer specifying client count (default: 50)
 * - Invalid or missing arguments default to DEFAULT_NUM_DRONES
 * 
 * **Process Management:**
 * - Each drone client runs in independent process space
 * - Parent process tracks all child PIDs for cleanup
 * - Staggered launch prevents connection storms
 * - Automatic cleanup on interruption or completion
 * 
 * **Error Handling:**
 * - Validates executable accessibility before launching
 * - Handles fork() failures with partial cleanup
 * - Reports process creation status for monitoring
 * - Graceful degradation on resource constraints
 * 
 * **Performance Monitoring:**
 * - Reports launch timing and success rates
 * - Tracks process IDs for system monitoring integration
 * - Provides clear status updates during execution
 * - Enables external monitoring of resource usage
 * 
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return 0 on successful test completion, 1 on failure
 * 
 * @pre Drone client executable must exist at ../drone_client
 * @pre System must support fork() and execv() operations
 * @pre Sufficient system resources for requested drone count
 * @post All launched processes are properly tracked and managed
 * @post Clean termination of all child processes on exit
 * 
 * @note Uses SIGINT and SIGTERM handlers for graceful shutdown
 * @note Memory allocation failures result in immediate termination
 * @warning Large drone counts may exhaust system resources
 * 
 * @see cleanup_handler() for signal handling details
 * @see DEFAULT_NUM_DRONES for default configuration
 */
int main(int argc, char *argv[]) {
    printf("=== Multi-Drone Load Testing Program ===\n");
    printf("Testing concurrent drone client connections\n\n");
    
    // Parse command line arguments for drone count
    if (argc > 1) {
        num_drones = atoi(argv[1]);
        if (num_drones <= 0) {
            printf("Warning: Invalid drone count '%s', using default\n", argv[1]);
            num_drones = DEFAULT_NUM_DRONES;
        }
    } else {
        num_drones = DEFAULT_NUM_DRONES;
    }
    
    printf("Configuration:\n");
    printf("  - Number of drone clients: %d\n", num_drones);
    printf("  - Launch interval: %d ms\n", DELAY_USEC / 1000);
    printf("  - Client executable: ../drone_client\n\n");
    
    // Verify client drone executable exists and is executable
    const char *client_path = "../drone_client";
    if (access(client_path, X_OK) != 0) {
        fprintf(stderr, "ERROR: Client drone executable not found at %s\n", client_path);
        fprintf(stderr, "Make sure to compile it first with 'make clientDrone'\n");
        fprintf(stderr, "Current working directory: %s\n", getcwd(NULL, 0));
        return 1;
    }
    
    printf("✓ Drone client executable verified\n");
    
    // Allocate memory for child process ID tracking
    child_pids = (pid_t*)malloc(num_drones * sizeof(pid_t));
    if (!child_pids) {
        perror("ERROR: Failed to allocate memory for child PIDs");
        return 1;
    }
    
    // Initialize all PIDs to 0 for safety
    for (int i = 0; i < num_drones; i++) {
        child_pids[i] = 0;
    }
    
    printf("✓ Process tracking initialized\n");
    
    // Set up signal handlers for clean termination
    signal(SIGINT, cleanup_handler);   // Ctrl+C
    signal(SIGTERM, cleanup_handler);  // Termination request
    
    printf("✓ Signal handlers configured\n");
    printf("\nStarting launch sequence...\n");
    
    // Launch drone clients with staggered timing
    time_t start_time = time(NULL);
    int successful_launches = 0;
    
    for (int i = 0; i < num_drones; i++) {
        child_pids[i] = fork();
        
        if (child_pids[i] < 0) {
            // Fork failed - handle gracefully
            perror("Fork failed");
            printf("Failed to launch drone client %d\n", i + 1);
            // Continue attempting to launch remaining drones
            child_pids[i] = 0; // Mark as failed
        }
        else if (child_pids[i] == 0) {
            // Child process - execute the drone client
            char id_str[16];
            sprintf(id_str, "%d", i + 1); // Generate unique ID
            
            // Create command arguments for client execution
            char *args[] = {
                (char*)client_path,
                id_str,            // Optional ID parameter
                NULL
            };
            
            // Alternative for clients that don't accept ID parameter:
            // char *args[] = { (char*)client_path, NULL };
            
            // Replace process image with drone client
            execv(client_path, args);
            
            // If execv returns, an error occurred
            perror("ERROR: execv failed");
            exit(EXIT_FAILURE);
        }
        else {
            // Parent process - track successful launch
            successful_launches++;
            printf("✓ Launched drone client %d (PID: %d)\n", i + 1, child_pids[i]);
            
            // Stagger launches to prevent connection storms
            usleep(DELAY_USEC);
        }
    }
    
    time_t end_time = time(NULL);
    double launch_duration = difftime(end_time, start_time);
    
    printf("\n=== Launch Summary ===\n");
    printf("Successfully launched: %d/%d drone clients\n", successful_launches, num_drones);
    printf("Total launch time: %.1f seconds\n", launch_duration);
    printf("Average launch rate: %.1f clients/second\n", 
           successful_launches / (launch_duration > 0 ? launch_duration : 1));
    
    if (successful_launches == 0) {
        printf("ERROR: No drone clients launched successfully\n");
        free(child_pids);
        return 1;
    }
    
    printf("\n=== Test Running ===\n");
    printf("All drone clients are now active and connecting to server\n");
    printf("Monitor server logs for connection and performance data\n");
    printf("Press Ctrl+C to terminate all drone clients and end test\n\n");
    
    // Wait for all child processes to complete
    // This will be interrupted by Ctrl+C signal
    int completed_processes = 0;
    for (int i = 0; i < num_drones; i++) {
        if (child_pids[i] > 0) {
            int status;
            pid_t result = waitpid(child_pids[i], &status, 0);
            if (result > 0) {
                completed_processes++;
                printf("Drone client %d (PID: %d) completed\n", i + 1, result);
            }
        }
    }
    
    // Natural completion (all processes finished)
    printf("\n=== Test Completed ===\n");
    printf("All %d drone clients have terminated naturally\n", completed_processes);
    
    // Clean up allocated resources
    free(child_pids);
    child_pids = NULL;
    
    printf("Load testing completed successfully\n");
    return 0;
}

/** @} */ // end of process_management group
/** @} */ // end of load_testing group