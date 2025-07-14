[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.15811369.svg)](https://doi.org/10.5281/zenodo.15811369)

# nFlows: NUMA-aware Runtime Workflow Scheduling System

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

## Run validations [[info](./tests/README.MD)]

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

The [evaluation](./evaluation/) folder contains three subfolders [system](./evaluation/system/), [templates](./evaluation/templates/), and [workflows](./evaluation/workflows/).

The **system folder** contains three configuration files:

1. Bandwidth distance matrix (values in GB/s). Examples: [non_uniform_bw.txt](./evaluation/system/non_uniform_bw.txt), [uniform_bw.txt](./evaluation/system/uniform_bw.txt).
2. Latency distance matrix (values in nanoseconds). Examples: [non_uniform_lat.txt](./evaluation/system/non_uniform_bw.txt), [uniform_lat.txt](./evaluation/system/uniform_bw.txt).
3. Relative distances among NUMA domains (dimensionless) [Optional]. Example: [non_uniform_lat_rel.txt](./evaluation/non_uniform_lat_rel.txt).

These configuration values are exposed exclusively to scheduling algorithms through the runtime API, enabling informed decision-making during execution. In parallel, the runtime system retrieves memory and core locality information via the `hwloc` library. This information is also made accessible to scheduling algorithms at runtime.

The **workflows folder** contains the workflows used for evaluation. 

```
C_montage-chameleon-2mass-01d-001.dot  
C_montage-chameleon-2mass-015d-001.dot  
C_montage-chameleon-2mass-025d-001.dot  
C_montage-chameleon-dss-05d-001.dot  
C_montage-chameleon-dss-10d-001.dot  
C_montage-chameleon-dss-075d-001.dot  
L_montage-chameleon-2mass-01d-001.dot  
L_montage-chameleon-2mass-015d-001.dot  
L_montage-chameleon-2mass-025d-001.dot  
L_montage-chameleon-dss-05d-001.dot  
L_montage-chameleon-dss-10d-001.dot  
L_montage-chameleon-dss-075d-001.dot  
S_montage-chameleon-2mass-01d-001.dot  
S_montage-chameleon-2mass-015d-001.dot  
S_montage-chameleon-2mass-025d-001.dot  
S_montage-chameleon-dss-05d-001.dot  
S_montage-chameleon-dss-10d-001.dot  
S_montage-chameleon-dss-075d-001.dot
```

```sh
./scripts/env/bin/python3 ./scripts/generate_dot.py ./workflows/srasearch/raw/srasearch-chameleon-50a-003.json ./workflows/srasearch/dot/srasearch-chameleon-50a-003.dot --dep_constant 4e7 1e8 --flops_constant 10000000
```

```sh
./scripts/env/bin/python3 ./scripts/generate_dot.py ./workflows/srasearch/raw/srasearch-chameleon-50a-003.json ./workflows/srasearch/dot/srasearch-chameleon-50a-003.dot --dep_scale_range 4e7 5e7 --flops_constant 10000000
```

```sh
./scripts/env/bin/python3 ./scripts/generate_dot.py ./workflows/srasearch/raw/srasearch-chameleon-50a-003.json ./workflows/srasearch/dot/srasearch-chameleon-50a-003.dot --dep_scale_range 4e7 1e8 --flops_constant 10000000
```

