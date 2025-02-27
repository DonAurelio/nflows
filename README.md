# Runtime Workflow Scheduler

* The runtime scheduler provides and environment for the modeling and analysis of scientific workflows' performance in real NUMA systems. 

## Validation

The proposed and implemented scheduling algorithms, along with the underlying modeling runtime system, were validated based on two credibility and assessment aspects for modeling and simulations proposed in [1]: code verification and solution verification.

The code and solution verification processes in this work integrate mathematical analysis, logical verification, and empirical testing.

Validation is performed through simulation, as the implemented algorithms produce deterministic results (i.e., workflow makespan), which are easily predictable for basic instances.

### Min-Min Algorithm (Code verification)

The Min-Min algorithm schedules tasks by selecting the task with the minimum completion time first and assigning it to the resource that can complete it the earliest.

#### Test 1

**Configuration:** [`./tests/config/min_min_simulation/config_1.json`](./tests/config/min_min_simulation/config_1.json)  

- **System Setup:**  
  - Processors (physical cores) operate at varying frequencies: `[1, 2, 4, 8]`.  
  - Four tasks with different computational requirements (in FLOPs):  
    - `Task1 [size=80]`  
    - `Task2 [size=160]`  
    - `Task3 [size=320]`  
  - Tasks are independent (no dependencies exist).  
  - Memory channel latencies and bandwidths are uniform across all memory domains, leading to a **Uniform Memory Access (UMA) system** (i.e., no NUMA effects).  
  - Processor speeds and clock frequencies are set so that **computation time can be directly obtained** by dividing the task size (in FLOPs) by the processor frequency.  

- **Validation Criteria:**  
  - Ensures the scheduler selects the task with the **minimum completion time first** and assigns it to the resource where the **earliest completion time** is achieved.  
  - Confirms that **core availability** is correctly updated throughout the scheduling process.  

- **Expected Outcome:**  
  - All tasks should be executed on the **4th core** (the fastest, operating at frequency 8).  
  - The final core availability should be **70**, which corresponds to the **workflow makespan**.  

## References

1. Blattnig, S., Green, L., Luckring, J., Morrison, J., Tripathi, R., & Zang, T. (2008). Towards a credibility assessment of models and simulations. In 49th AIAA/ASME/ASCE/AHS/ASC Structures, Structural Dynamics, and Materials Conference, 16th AIAA/ASME/AHS Adaptive Structures Conference, 10th AIAA Non-Deterministic Approaches Conference, 9th AIAA Gossamer Spacecraft Forum, 4th AIAA Multidisciplinary Design Optimization Specialists Conference (p. 2156).