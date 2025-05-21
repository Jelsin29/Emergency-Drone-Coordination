#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#define DEFAULT_NUM_DRONES 50
#define DELAY_USEC 200000 // 200ms between drone launches

// Array to keep track of child PIDs
pid_t *child_pids = NULL;
int num_drones = 0;

// Signal handler to clean up child processes when terminated
void cleanup_handler(int sig) {
    printf("\nReceived signal %d. Terminating all drone clients...\n", sig);
    
    // Kill all child processes
    for (int i = 0; i < num_drones; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGTERM);
            printf("Terminated drone client with PID %d\n", child_pids[i]);
        }
    }
    
    free(child_pids);
    exit(0);
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    if (argc > 1) {
        num_drones = atoi(argv[1]);
        if (num_drones <= 0) {
            num_drones = DEFAULT_NUM_DRONES;
        }
    } else {
        num_drones = DEFAULT_NUM_DRONES;
    }
    
    printf("Starting %d drone clients...\n", num_drones);
    
    // Verify client drone executable exists
    const char *client_path = "../drone_client";
    if (access(client_path, X_OK) != 0) {
        fprintf(stderr, "Error: Client drone executable not found at %s\n", client_path);
        fprintf(stderr, "Make sure to compile it first with 'make clientDrone'\n");
        return 1;
    }
    
    // Allocate memory for child PIDs
    child_pids = (pid_t*)malloc(num_drones * sizeof(pid_t));
    if (!child_pids) {
        perror("Failed to allocate memory for child PIDs");
        return 1;
    }
    
    // Set up signal handler for clean termination
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);
    
    // Launch drone clients
    for (int i = 0; i < num_drones; i++) {
        child_pids[i] = fork();
        
        if (child_pids[i] < 0) {
            // Fork failed
            perror("Fork failed");
            cleanup_handler(SIGTERM); // Clean up already created processes
            return 1;
        }
        else if (child_pids[i] == 0) {
            // Child process - exec the drone client
            char id_str[16];
            sprintf(id_str, "%d", i + 1); // Generate unique ID for each drone
            
            // Create command arguments
            char *args[] = {
                (char*)client_path,
                id_str,            // Optional ID parameter if your client supports it
                NULL
            };
            
            // If your client doesn't take an ID parameter, use this instead:
            // char *args[] = { (char*)client_path, NULL };
            
            execv(client_path, args);
            
            // If execv returns, an error occurred
            perror("Execv failed");
            exit(EXIT_FAILURE);
        }
        else {
            // Parent process
            printf("Started drone client %d (PID: %d)\n", i + 1, child_pids[i]);
            usleep(DELAY_USEC); // Delay between starting clients
        }
    }
    
    printf("\nAll %d drone clients have been launched.\n", num_drones);
    printf("Press Ctrl+C to terminate all drone clients...\n");
    
    // Wait for all child processes (will be interrupted by Ctrl+C)
    for (int i = 0; i < num_drones; i++) {
        waitpid(child_pids[i], NULL, 0);
    }
    
    // Clean up
    free(child_pids);
    printf("All drone clients have terminated.\n");
    
    return 0;
}