#!/bin/bash
# filepath: /home/jelsin/clases/programlama/project2-emergency-drone-coordination-venezuela/tests/launch_drones.sh

# Number of drone clients to start
NUM_DRONES=50
# Delay between starting drones (in microseconds)
DELAY=200000

# Path to the drone client executable (relative to the script location)
CLIENT_PATH="../drone_client"

echo "Starting $NUM_DRONES drone clients..."

# Check if client executable exists
if [ ! -f "$CLIENT_PATH" ]; then
    echo "Error: Client drone executable not found at $CLIENT_PATH"
    echo "Make sure to compile it first with 'make'"
    exit 1
fi

# Array to store PIDs
declare -a PIDS

# Trap ctrl-c and call cleanup function
trap cleanup INT TERM
cleanup() {
    echo "Terminating all drone clients..."
    for pid in "${PIDS[@]}"; do
        if ps -p $pid > /dev/null; then
            kill $pid 2>/dev/null
            echo "Terminated drone client with PID $pid"
        fi
    done
    exit 0
}

# Launch drone clients
for ((i=1; i<=NUM_DRONES; i++)); do
    # Start drone client in background
    $CLIENT_PATH &
    PID=$!
    PIDS+=($PID)
    echo "Started drone client $i (PID: $PID)"
    usleep $DELAY
done

echo "All $NUM_DRONES drone clients have been launched."
echo "Press Ctrl+C to terminate all drone clients..."

# Wait for ctrl-c
wait

# Should never reach here, but just in case
cleanup