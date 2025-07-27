[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.15811369.svg)](https://doi.org/10.5281/zenodo.15811369)

# nFlows: NUMA-Aware Workflow Execution System

nFlows is a C/C++ runtime system primarily developed to enable the development and evaluation of NUMA-aware scheduling algorithms for scientific workflows. It executes scientific workflows on both simulated and real NUMA (Non-Uniform Memory Access) hardware.

The system leverages `simgrid` to load DOT-formatted workflows, emulating them as task graphs with annotated FLOP counts and communication sizes. During execution, nFlows tracks task placement, memory locality, and data access times, providing this information to scheduling algorithms for adaptive runtime scheduling. It utilizes `pthreads` for parallel task execution and core affinity, and the `hwloc` library to monitor and manage memory locality across NUMA nodes.

### Requirements

1. **OS:** Ubuntu 22.04 
2. **Hardware:** Non-Uniform Memory Access (NUMA) Memory Archuitecture. At least two available NUMA nodes.
3. **Software:**  simgrid-3.35, python3.10, hwloc 2.7.0, etc. See `requirements.sh` for the list of all dependecies.

### Install

```sh
sudo ./requirements.sh
make -j
sudo make install
```

### Example

```sh
nflows --log=fifo_scheduler.thres:debug --log=heft_scheduler.thres:debug --log=eft_scheduler.thres:debug --log=hardware.thres:debug ./example/config.json
```

### Test

```sh
make test
```
