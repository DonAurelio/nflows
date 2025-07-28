[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.15986001.svg)](https://doi.org/10.5281/zenodo.15986001)

<img src="https://raw.githubusercontent.com/DonAurelio/nflows/refs/heads/main/logo.png"
     align="right"
     width="15%"
     alt="nFlows logo\">

# nFlows: NUMA-Aware Workflow Execution Runtime System

nFlows is a C/C++ runtime system primarily developed to enable the development and evaluation of NUMA-aware scheduling algorithms for scientific workflows. It executes scientific workflows on both simulated and real NUMA (Non-Uniform Memory Access) hardware.

The system leverages `simgrid` to load DOT-formatted workflows, emulating them as task graphs with annotated FLOP counts and communication sizes. During execution, nFlows tracks task placement, memory locality, and data access times, providing this information to scheduling algorithms for adaptive runtime scheduling. It utilizes `pthreads` for parallel task execution and core affinity, and the `hwloc` library to monitor and manage memory locality across NUMA nodes.

### Requirements

1. **OS:** Ubuntu 22.04 
2. **Hardware:** Non-Uniform Memory Access (NUMA) Memory Architecture. At least two available NUMA nodes.
3. **Software:**  simgrid-3.35, python3.10, hwloc 2.7.0, etc. See `install_requirements.sh` for the list of all dependencies.

### Install

```sh
sudo ./install_requirements.sh
sudo ./install_simgrid.sh
make -j
sudo make install
```

_Install to default /usr/local_
* `./install_simgrid.sh`
* make install

_Install to a custom prefix_
* `./install_simgrid.sh /path/to/custom/dir`
* `make install PREFIX=/path/to/custom/dir`. Add `/path/to/custom/dir` to the `$PATH`.

### Example

```sh
nflows --log=fifo_scheduler.thres:debug --log=heft_scheduler.thres:debug --log=eft_scheduler.thres:debug --log=hardware.thres:debug ./example/config.json
```

### Test

```sh
make test
```
