#!/bin/bash

if [ $# -lt 1 ]; then
  echo "Usage: $0 <program> [arguments...]"
  exit 1
fi

# Extract the program and its arguments
PROGRAM=$1
shift
ARGS="$@"

# Run the program in the background
echo "Starting program: $PROGRAM $ARGS"
$PROGRAM $ARGS &
PID=$!

# Check if the program started successfully
if ! ps -p $PID > /dev/null 2>&1; then
  echo "Failed to start the program."
  exit 1
fi

echo "Program started with PID: $PID"

# Monitor NUMA stats for the running program
echo "Monitoring NUMA stats for PID $PID..."
echo "Press Ctrl+C to stop monitoring."

while ps -p $PID > /dev/null 2>&1; do
  echo "--- NUMA stats for PID $PID ---"
  cat /proc/$PID/numa_stat
  sleep 1
done

echo "Program has exited. Monitoring stopped."
