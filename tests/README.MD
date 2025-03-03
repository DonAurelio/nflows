# Validation

The proposed and implemented scheduling algorithms, along with the underlying modeling runtime system, were validated based on two credibility and assessment aspects for modeling and simulations proposed in [1]: code verification and solution verification.

The code and solution verification processes in this work integrate *mathematical analysis*, *logical verification*, and *empirical testing*.

Validation is performed through simulation, as the implemented algorithms produce deterministic results (i.e., workflow makespan), which are easily predictable for basic instances.

## Min-Min Algorithm 

The Min-Min algorithm schedules tasks by selecting the task with the minimum completion time first and assigning it to the resource that can complete it the earliest.

### Test 1 [`config_1.json`](./config/min_min_simulation/config_1.json)
   
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

### Test 2 [`config_2.json`](./config/min_min_simulation/config_2.json)
 
* System Setup:  
  * Two processors (physical cores) operate at the same frequency: `1`.  
  * Each processor belongs to a different memory domain.  
  * Three tasks with different computational requirements (in FLOPs):  
    * `Task1 [size=160]`  
    * `Task2 [size=360]`  
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
  * The first task, `Task1 [size=160]`, will be executed on the first core, while `Task2 [size=360]` will be executed on the second core.  
  * Each core writes a data item to its **local memory**, but `Task3 [size=80]` must perform:  
    * **One local access** to read the first item.  
    * **One remote access** to read the second item.  
  * The final core availabilities should be **460** (corresponding to the **workflow makespan**) and **340**, respectively.

### Test 3 [`config_3.json`](./config/min_min_simulation/config_3.json)
   
* System Setup:  
  * Same as **Test 2**, with the following difference:  
  * Memory channel bandwidths are **uniform** across memory domains: **4 GB/s**.  

* Validation Criteria:
  * Compares the **workflow makespan** between **Test 2** and **Test 3** to evaluate the impact of uniform vs. non-uniform memory access.  

* Expected Outcome:  
  * Since memory access is **uniform** in **Test 3**, there is **no additional cost for remote access**.  
  * As a result, the **workflow makespan** should be **lower** (**440**) than in **Test 2** (**460**).
  * Therefore, there is a penalty (**20 seconds**) when performing remote memory accesses in NUMA systems that must be taken into consideration when scheduling workflows.

## HEFT Algorithm 

The HEFT (Heterogeneous Earliest Finish Time) scheduling algorithm prioritizes tasks in a task dependency graph based on their upward rank, which considers task execution time and communication delays. It then assigns each task to the processor that minimizes its earliest finish time [2].

### Test 1 [`config_1.json`](./config/heft_simulation/config_1.json)
   
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
  * Ensures the scheduler selects the task with the **maximum upward rank** and assigns it to the resource where the **earliest completion time** is achieved.
  * Confirms that **core availability** is correctly updated throughout the scheduling process.  

* Expected Outcome:
  * Execution order should be `Task3` (core 3), `Task2` (core 2), and `Task1` (core 1).
  * The final core availability should be **40**, which corresponds to the **workflow makespan**.

### Test 2 [`config_2.json`](./config/heft_simulation/config_2.json)
 
* System Setup:  
  * Two processors (physical cores) operate at the same frequency: `1`.  
  * Each processor belongs to a different memory domain.  
  * Three tasks with different computational requirements (in FLOPs):  
    * `Task1 [size=160]`  
    * `Task2 [size=320]`  
    * `Task3 [size=80]`  
  * Tasks 1 and 2 reduce to Task 3:  
    * `Task1 -> Task3 [size=80]`  
    * `Task2 -> Task3 [size=80]`  
  * Memory channel latencies are set to `0`, so communication cost **only depends on bandwidths**.  
  * Memory channel bandwidths are **non-uniform** across memory domains:  
    * **Local access:** 4 GB/s  
    * **Remote access:** 2 GB/s  

* Validation Criteria:  
  * Ensures the scheduler selects the task with the **highest upward rank** and assigns it to the resource where the **earliest completion time** is achieved, considering **non-uniform memory access times**.
  * Confirms that **core availability** is correctly updated throughout the scheduling process.  

* Expected Outcome: 
  * Task, `Task2 [size=320]`, will be executed on the first core, while `Task1 [size=160]` will be executed on the second core.  
  * Each core writes a data item to its **local memory**, but `Task3 [size=80]` must perform:  
    * **One local access** to read the first item.  
    * **One remote access** to read the second item.  
  * The final core availabilities should be **340** and **460** (corresponding to the **workflow makespan**), respectively.
  * Although it provides the same workflow makespan as Min-Min, it differs in core selection.

### Test 3 [`config_3.json`](./config/heft_simulation/config_3.json)
   
* System Setup:  
  * Same as **Test 2**, with the following difference:  
  * Memory channel bandwidths are **uniform** across memory domains: **4 GB/s**.  

* Validation Criteria:
  * Compares the **workflow makespan** between **Test 2** and **Test 3** to evaluate the impact of uniform vs. non-uniform memory access.  

* Expected Outcome:  
  * Since memory access is **uniform** in **Test 3**, there is **no additional cost for remote access**.  
  * As a result, the **workflow makespan** should be **lower** (**440**) than in **Test 2** (**460**).
  * Therefore, there is a penalty (**20 seconds**) when performing remote memory accesses in NUMA systems that must be taken into consideration when scheduling workflows.

## FIFO Algorithm

The FIFO (First-In, First-Out) scheduling algorithm selects tasks in the order they arrive, without prioritization. The oldest (earliest arrived) task in the queue is selected first. The first available core is assigned to execute the selected task.

### Test 1 [`config_1.json`](./config/fifo_simulation/config_1.json)
   
* System Setup:
  * A single processor (physical core) operating at **1 Hz**, making computational cost equivalent to execution time.  
  * Memory channel latencies and bandwidths are set so that communication time is equal to communication cost.  
  * Memory channel latencies and bandwidths are **uniform** across all memory domains, resulting in a **Uniform Memory Access (UMA) system** (i.e., no NUMA effects).  
  * Processor speed and clock frequency are set so that **computation time is directly determined** by dividing the task size (in FLOPs) by the processor frequency.  
  * Five tasks, each with identical **computational (10 FLOPs) and communication (10 bytes)** requirements.  

```nginx
       Task_1
      /      \
  Task_2    Task_5
   /    \
Task_3  Task_4
```

* Validation Criteria:
  * Ensures the scheduler selects tasks following the FIFO order.
  * Confirms that **core availability** is correctly updated throughout the scheduling process.  

* Expected Outcome:
  * FIFO order (assuming **level-order traversal, depth-first approach**): `Task_1 → Task_2 → Task_5 → Task_3 → Task_4`  
  * The final core availability should be **110**, which corresponds to the **workflow makespan**.

### Test 2 [`config_2.json`](./config/fifo_simulation/config_2.json)
   
* System Setup:
  * Same as **Test 1**, with the following difference:  
  * Four processors (physical cores) operating at **1 Hz**.  

* Validation Criteria:
  * Ensures the scheduler selects the next core in a round robin fashion.
  * Confirms that **core availability** is correctly updated throughout the scheduling process.  

* Expected Outcome:
  * FIFO order task selection order and round-robin core selection: `Task_1 (core 0) → Task_2 (core 1) → Task_5 (core 2) → Task_3 (core 3) → Task_4 (core 0)`  
  * The final core availability should be **70**, which corresponds to the **workflow makespan**.

## Generic Validations

### Script 1 [validate_offsets.py](./validators/validate_offsets.py)

This script verifies that the execution offsets collected by the tool align with expected total execution times. Specifically, it ensures that the sum of the **read, compute, and write offsets** matches the total execution time for each task.  

**Input Data**: The script processes a **YAML file** containing the following execution offset data:  

- `comm_name_read_offsets`: Communication **read** offsets between tasks.  
- `comm_name_write_offsets`: Communication **write** offsets between tasks.  
- `exec_name_compute_offsets`: **Compute** offsets for each task.  
- `exec_name_total_offsets`: **Total** execution offsets for each task.  

**Validation Logic**: For each task:

1. Computes the **maximum read offset** from incoming communication.  
2. Adds the **compute offset** for the task.  
3. Computes the **maximum write offset** from outgoing communication.  
4. Derives the **computed total execution time** using: $\text{Computed Total} = \max(\text{Read End}) + \text{Compute Time} + \max(\text{Write Offset})$ 
5. Compares the **computed total** against the **expected total execution time** in `exec_name_total_offsets`.  
6. If the computed total does **not match** the expected total, the script **flags a validation error**.  

**Dependency Check (Indirect Validation)**: By ensuring the **computed total execution time includes the read offsets**, the script indirectly validates that:  
- **Dependencies are respected**: A task cannot start execution before its required input data is available.  
- **Data dependencies influence scheduling**: The order of execution is preserved based on task communication.  

#### Example

```yaml
comm_name_read_offsets:
  Task_2->Task_3: {start: 180, end: 200, payload: 80}
  Task_1->Task_3: {start: 180, end: 200, payload: 80}

comm_name_write_offsets:
  Task_2->Task_3: {start: 160, end: 180, payload: 80}
  Task_1->Task_3: {start: 160, end: 180, payload: 80}

exec_name_compute_offsets:
  Task_3: {start: 200, end: 280, payload: 80}
  Task_2: {start: 0, end: 160, payload: 160}
  Task_1: {start: 0, end: 160, payload: 160}

exec_name_total_offsets:
  Task_3: {start: 180, end: 280, payload: 80}
  Task_2: {start: 0, end: 180, payload: 160}
  Task_1: {start: 0, end: 180, payload: 160}
```

Task 3:
- *Max Read Time:* `max(200, 200) = 200` (from `Task_1->Task_3` and `Task_2->Task_3`)  
- *Compute Time:* `80` (from `exec_name_compute_offsets`)  
- *Max Write Time:* `0` (Task 3 is an end task, so no outgoing communication)  
- *Computed Total:* `200` (Max Read Time) + ``80` (Compute Time) + `0` (Max Write Time) = `280`
- *Expected Total:* `280` (Validation Passed)  

Task 2:
- *Max Read Time:* `0` (Task 2 is a root task, no dependencies)  
- *Compute Time:* `160`  
- *Max Write Time:* `20` (to `Task_3`)  
- *Computed Total:*  `0` (Max Read Time) + `160` (Compute Time) + `20` (Max Write Time) = `180`
- *Expected Total:* `180` (Validation Passed)  

Task 1:
- *Max Read Time:* `0` (Task 1 is a root task, no dependencies)  
- *Compute Time:* `160`  
- *Max Write Time:* `20` (to `Task_3`)  
- *Computed Total:*  `0` (Max Read Time) + `160` (Compute Time) + `20` (Max Write Time) = `180`
- *Expected Total:* `180` (Validation Passed)  

# References

1. Blattnig, S., Green, L., Luckring, J., Morrison, J., Tripathi, R., & Zang, T. (2008). Towards a credibility assessment of models and simulations. In 49th AIAA/ASME/ASCE/AHS/ASC Structures, Structural Dynamics, and Materials Conference, 16th AIAA/ASME/AHS Adaptive Structures Conference, 10th AIAA Non-Deterministic Approaches Conference, 9th AIAA Gossamer Spacecraft Forum, 4th AIAA Multidisciplinary Design Optimization Specialists Conference (p. 2156).
2. Topcuoglu, H., Hariri, S., & Wu, M. Y. (2002). Performance-effective and low-complexity task scheduling for heterogeneous computing. IEEE transactions on parallel and distributed systems, 13(3), 260-274.
