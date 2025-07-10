[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.15811369.svg)](https://doi.org/10.5281/zenodo.15811369)

# nflows: NUMA-aware Runtime Workflow Scheduling System

**nFlows** (**N**UMA-Aware Work**flow** Execution Runtime **S**ystem), a C/C++ program built to model, execute (both in simulation and on actual hardware), and analyze scientific workflow scheduling algorithms, specifically considering NUMA (Non-uniform Memory Access) effects in HPC systems. nFlows emulates the execution of workflows with clearly defined structures. It models computational tasks using floating-point operations (FLOPs) and communication payloads in bytes. During execution, it traces tasks and data locality, and records data access performance. This collected data can be accessed by dynamic scheduling algorithms at runtime to improve scheduling decisions. The system leverages POSIX threads (pthreads) for asynchronous task execution and core binding, and the Portable Hardware Locality (hwloc) library to monitor task placement and data locality across NUMA nodes.

### Content

1. evaluation.
2. example.
3. notebooks.
4. prefetchers.
5. scripts.
6. tests.
7. workflows.

### Requirements

1. **Operative System (OS):** Ubuntu 22.04 
2. **System Memory Archuitecture:** Non-Uniform Memory Access (NUMA). At least two available NUMA nodes.
3. **Software:**  simgrid-3.35, python3.10, hwloc 2.7.0, etc. See `simgrid_install.sh` for the list of all dependecies.

### Getting started

1. Install requirements

```sh
sudo bash simgrid_install.sh
```

2. Compile

```sh
make
```

3. Souce simgrid

```sh
source simgrid_source.sh
```

4. Execute example (`--log` flags are optional.)

```sh
./nflows --log=fifo_scheduler.thres:debug --log=heft_scheduler.thres:debug --log=eft_scheduler.thres:debug --log=hardware.thres:debug ./example/config.json
```

## Run tests

1. Create a Python virtualenv.

```sh
virtualenv --python=python3 ./scripts/.env
```

2. Install requirements.

```sh
./scripts/.env/bin/pip3 install -r ./scripts/requirements.txt
```

3. Execute tests.

```sh
make test
```

## Run evaluation

#### Create the system configuration files or use the existing ones.

The runtime system relies on three system configuration input files. This information is made available to scheduling algorithms exclusively through the runtime API, allowing them to make decisions based on these values. In parallel, the runtime system obtains information about memory and core locality through the hwloc `library`. System configurations files are described as follows.

1. Bandwidth distance matrix (values in GB/s). Examples: [non_uniform_bw.txt](./evaluation/system/non_uniform_bw.txt), [uniform_bw.txt](./evaluation/system/uniform_bw.txt).
2. Latency distance matrix (values in nanoseconds). Examples: [non_uniform_lat.txt](./evaluation/system/non_uniform_bw.txt), [uniform_lat.txt](./evaluation/system/uniform_bw.txt).
3. Relative distances among NUMA domains (dimensionless). Example: [non_uniform_lat_rel.txt](./evaluation/non_uniform_lat_rel.txt).

#### Create the experiment configuration files (templates) or use the existing ones.
#### Generate workflow instances for evaluation.
