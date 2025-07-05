[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.15811369.svg)](https://doi.org/10.5281/zenodo.15811369)

# nflows: NUMA-aware Runtime Workflow Scheduling System

**nFlows** (**N**UMA-Aware Work**flow** Execution Runtime **S**ystem), a C/C++ program built to model, execute (both in simulation and on actual hardware), and analyze scientific workflow scheduling algorithms, specifically considering NUMA (Non-uniform Memory Access) effects in HPC systems. nFlows emulates the execution of workflows with clearly defined structures. It models computational tasks using floating-point operations (FLOPs) and communication payloads in bytes. During execution, it traces tasks and data locality, and records data access performance. This collected data can be accessed by dynamic scheduling algorithms at runtime to improve scheduling decisions. The system leverages POSIX threads (pthreads) for asynchronous task execution and core binding, and the Portable Hardware Locality (hwloc) library to monitor task placement and data locality across NUMA nodes.

### Validation

1. [Validation (Code/Tracing/Model)](./tests/README.MD)
