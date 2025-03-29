# nflow: NUMA-aware Runtime Workflow Scheduling System

**nflow** is a tool designed to facilitate the implementation of NUMA-aware scheduling algorithms, addressing the impact of NUMA effects in high-performance computing systems. At the hardware level, performance is influenced by the locality of processors executing memory access operations and the memory controllers managing these operations. At the operating system level, additional complexities arise from resource optimization mechanisms, such as process migration for load balancing across NUMA nodes, NUMA balancing techniques for dynamically relocating data pages, and memory binding policies (first-touch, next-touch, interleave, bind) that govern data allocation across memory domains. By tackling these challenges, nflow enables more efficient task scheduling and memory management in NUMA architectures.

### Validation

1. [Validation (Code/Tracing/Model)](./tests/README.MD)

### Proposed Features for Future Implementations  

1. **Support for Additional Memory Policies in Simulation**  
   - The current simulation employs the *first-touch* memory policy.  
   - Future versions could extend support to other policies, such as *bind* and *next-touch*, enabling more comprehensive memory management emulation.  

2. **Support for Execution Trace Reproduction**  
   - Enabling the reproduction of execution traces, particularly data access patterns, in simulation could provide insights for optimizing data access performance in scheduling algorithms before real-time execution. 
   - This feature would facilitate the analysis of specific access patterns, aiding in performance investigations and improvements.  
