#!/bin/bash

# Script to repeatedly run make clean && make until error occurs

max_attempts=100  # Maximum number of attempts
count=0
start_time=$(date +%s)

echo "Starting test loop - will run make clean && make until error occurs (max $max_attempts times)"
echo "Press Ctrl+C to abort"

while [ $count -lt $max_attempts ]; do
  count=$((count+1))
  echo "==================== Attempt $count of $max_attempts ===================="
  
  # Run make clean and make
  make clean > /dev/null
  if make; then
    echo "Build $count: SUCCESS"
  else
    error_code=$?
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    
    echo "==================== ERROR DETECTED ===================="
    echo "Build $count: FAILED with exit code $error_code"
    echo "Found error after $count attempts"
    echo "Total time: $duration seconds ($(($duration / 60)) minutes)"
    exit 0
  fi
  
  # Optional: Add a small delay between attempts
  sleep 1
done

end_time=$(date +%s)
duration=$((end_time - start_time))

echo "==================== MAX ATTEMPTS REACHED ===================="
echo "Completed $max_attempts attempts without error"
echo "Total time: $duration seconds ($(($duration / 60)) minutes)"