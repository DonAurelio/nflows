# Validation

The proposed and implemented scheduling algorithms, along with the underlying modeling runtime system, were validated based on two credibility and assessment aspects for modeling and simulations proposed in [1]: code verification and solution verification.

The code and solution verification processes in this work integrate *mathematical analysis*, *logical verification*, and *empirical testing*.

Validation is performed through simulation, as the implemented algorithms produce deterministic results (i.e., workflow makespan), which are easily predictable for basic instances.

## Min-Min Algorithm 

The Min-Min algorithm schedules tasks by selecting the task with the minimum completion time first and assigning it to the resource that can complete it the earliest.

### Test 1 (Code verification)

* Configuration: [`./tests/config/min_min_simulation/config_1.json`](./tests/config/min_min_simulation/config_1.json)  
* System Setup:  
  * Processors (physical cores) operate at varying frequencies: `[1, 2, 4, 8]`.  
  * Three tasks with different computational requirements (in FLOPs):  
    * `Task1 [size=80]`  
    * `Task2 [size=160]`  
    * `Task3 [size=320]`  
  * Tasks are independent (no dependencies exist).  
  * Memory channel latencies and bandwidths are uniform across all memory domains, leading to a **Uniform Memory Access (UMA) system** (i.e., no NUMA effects).  
  * Processor speeds and clock frequencies are set so that **computation time can be directly obtained** by dividing the task size (in FLOPs) by the processor frequency.  

* Validation Criteria:
  * Ensures the scheduler selects the task with the **minimum completion time first** and assigns it to the resource where the **earliest completion time** is achieved.  
  * Confirms that **core availability** is correctly updated throughout the scheduling process.  

* Expected Outcome: 
  * All tasks should be executed on the **4th core** (the fastest, operating at frequency 8).  
  * The final core availability should be **70**, which corresponds to the **workflow makespan**.

### Test 2 (Code Verification)

* Configuration: [`./tests/config/min_min_simulation/config_2.json`](./tests/config/min_min_simulation/config_2.json)  
* System Setup:  
  * Two processors (physical cores) operate at the same frequency: `1`.  
  * Each processor belongs to a different memory domain.  
  * Four tasks with different computational requirements (in FLOPs):  
    * `Task1 [size=160]`  
    * `Task2 [size=160]`  
    * `Task3 [size=80]`  
  * Tasks 1 and 2 reduce to Task 3:  
    * `Task1 -> Task3 [size=80]`  
    * `Task2 -> Task3 [size=80]`  
  * Memory channel latencies are set to `0`, so communication cost **only depends on bandwidths**.  
  * Memory channel bandwidths are **non-uniform** across memory domains:  
    * **Local access:** 4 GB/s  
    * **Remote access:** 2 GB/s  

* Validation Criteria:  
  * Ensures the scheduler selects the task with the **minimum completion time first** and assigns it to the resource where the **earliest completion time** is achieved, considering **non-uniform memory access times**.  
  * Confirms that **core availability** is correctly updated throughout the scheduling process.  

* Expected Outcome: 
  * The first task, `Task1 [size=160]`, will be executed on the first core, while `Task2 [size=160]` will be executed on the second core.  
  * Each core writes a data item to its **local memory**, but `Task3 [size=80]` must perform:  
    * **One local access** to read the first item.  
    * **One remote access** to read the second item.  
  * The final core availabilities should be **300** (corresponding to the **workflow makespan**) and **180**, respectively.

### Test 3 (Code Verification)

* Configuration: [`./tests/config/min_min_simulation/config_3.json`](./tests/config/min_min_simulation/config_3.json)  
* System Setup:  
  * Same as **Test 2**, with the following difference:  
  * Memory channel bandwidths are **uniform** across memory domains: **4 GB/s**.  

* Validation Criteria:
  * Compares the **workflow makespan** between **Test 2** and **Test 3** to evaluate the impact of uniform vs. non-uniform memory access.  

* Expected Outcome:  
  * Since memory access is **uniform** in **Test 3**, there is **no additional cost for remote access**.  
  * As a result, the **workflow makespan** should be **lower** (**280**) than in **Test 2** (**300**).


# References

1. Blattnig, S., Green, L., Luckring, J., Morrison, J., Tripathi, R., & Zang, T. (2008). Towards a credibility assessment of models and simulations. In 49th AIAA/ASME/ASCE/AHS/ASC Structures, Structural Dynamics, and Materials Conference, 16th AIAA/ASME/AHS Adaptive Structures Conference, 10th AIAA Non-Deterministic Approaches Conference, 9th AIAA Gossamer Spacecraft Forum, 4th AIAA Multidisciplinary Design Optimization Specialists Conference (p. 2156).
