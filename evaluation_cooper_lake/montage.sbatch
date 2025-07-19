#!/bin/bash
# --- Validation block ---
# Get physical cores on the node
PHYSICAL_CORES=$(nproc)

# PBS does not provide an exact equivalent of SLURM_CPUS_PER_TASK.
# We approximate with ncpus in the node allocation line.
ALLOCATED_CORES=$(grep -c ^processor /proc/cpuinfo)

if [[ "$ALLOCATED_CORES" -ne "$PHYSICAL_CORES" ]]; then
  echo "WARNING: PBS allocated $ALLOCATED_CORES CPUs, but $PHYSICAL_CORES physical cores are available."
  # No direct --cpus-per-task control; log and continue
else
  echo "Validation OK: Allocated CPUs ($ALLOCATED_CORES) match physical cores ($PHYSICAL_CORES)"
fi

# --- Run your program here ---

