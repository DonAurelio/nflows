
# Workflow Performance Evaluation

The evaluation folder contains three subfolders: system, templates, and workflows.

* The system folder includes the distance matrices of the NUMA node system under evaluation.
* The templates folder contains configuration file templates used to describe the experimental setup.
* The workflows folder contains all the input workflows considered for evaluation.

### The `system` folder

Contains three configuration files:

1. Bandwidth distance matrix (values in GB/s). Examples: [non_uniform_bw.txt](./system/non_uniform_bw.txt), [uniform_bw.txt](./system/uniform_bw.txt).
2. Latency distance matrix (values in nanoseconds). Examples: [non_uniform_lat.txt](./system/non_uniform_bw.txt), [uniform_lat.txt](./system/uniform_bw.txt).
3. Relative distances among NUMA domains (dimensionless) [Optional]. Example: [non_uniform_lat_rel.txt](./non_uniform_lat_rel.txt).

Values in these configuration files are made available to scheduling algorithms through the runtime API.
This enables the scheduler to make NUMA-aware scheduling decisions. In parallel, the runtime system retrieves memory and core locality information via the `hwloc` library. This information is also made accessible to scheduling algorithms at runtime.

### The `templates` folder

Contains experimental configuration files grouped into folders. These groups determine how the results will be organized. Each workflow is evaluated against all experimental configurations defined in the templates folder. A new configuration is automatically created by varying the `dag_file` and `out_file_name` fields in the configuration file when the workflow is executed.

```sh
templates
├── fifo_16
│   ├── 1_MA_1.json
│   ├── 1_M_1.json
│   ├── 1_SL_1.json
│   └── 1_SR_1.json
├── fifo_24
│   ├── 1_MA_1.json
│   ├── 1_M_1.json
│   ├── 1_SL_1.json
│   └── 1_SR_1.json
├── fifo_8
│   ├── 1_MA_1.json
│   ├── 1_M_1.json
│   ├── 1_SL_1.json
│   └── 1_SR_1.json
├── heft_16
│   ├── 1_MA_1.json
│   ├── 1_M_1.json
│   ├── 1_SL_1.json
│   └── 1_SR_1.json
├── heft_24
│   ├── 1_MA_1.json
│   ├── 1_M_1.json
│   ├── 1_SL_1.json
│   └── 1_SR_1.json
├── heft_8
│   ├── 1_MA_1.json
│   ├── 1_M_1.json
│   ├── 1_SL_1.json
│   └── 1_SR_1.json
├── min_min_16
│   ├── 1_MA_1.json
│   ├── 1_M_1.json
│   ├── 1_SL_1.json
│   └── 1_SR_1.json
├── min_min_24
│   ├── 1_MA_1.json
│   ├── 1_M_1.json
│   ├── 1_SL_1.json
│   └── 1_SR_1.json
└── min_min_8
    ├── 1_MA_1.json
    ├── 1_M_1.json
    ├── 1_SL_1.json
    └── 1_SR_1.json
```

### The `workflows` folder 

Contains the workflows used for evaluation. The workflows were downloaded from the [WfInstances browser](https://wfinstances.ics.hawaii.edu/). Since the workflow traces were provided in JSON format, the [generate_dot.py](../scripts/generate_dot.py) script was used to generate the DOT input files required by the runtime system. To handle parallel dependencies between tasks sharing multiple files, the script consolidates data items with identical filenames between parent outputs and child inputs.

We considered three variations of each workflow. All variations use a constant computation payload of 10 MFlops but differ in communication payload: constant (40 MB), small (40–50 MB), and large (40–100 MB). The communication payload ranges scale the original values in the workflow traces to match the specified range while preserving the original statistical distribution. Input and output files defined in the workflows are not considered during scaling or execution.

```sh
generate_dot.py ../workflows/montage/raw/montage-chameleon-2mass-01d-001.json ./workflows/C_montage-chameleon-2mass-01d-001.dot --dep_constant 4e7 1e8 --flops_constant 10000000
generate_dot.py ../workflows/montage/raw/montage-chameleon-2mass-01d-001.json ./workflows/S_montage-chameleon-2mass-01d-001.dot --dep_scale_range 4e7 5e7 --flops_constant 10000000
generate_dot.py ../workflows/montage/raw/montage-chameleon-2mass-01d-001.json ./workflows/L_montage-chameleon-2mass-01d-001.dot --dep_scale_range 4e7 1e8 --flops_constant 10000000
```

Available workflows for evaluation.

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

### Run evaluation

1. Ajust [Makefile](./Makefile) to reflect the number of times each workflow should be executed (to compute averages or reduce the impact of outliers).

```sh
EVALUATION_REPEATS := 1
```

2. Select one or more of the preconfigured workflows from [./evaluation/workflows](./evaluation/workflows/) for evaluation.

3. **[Option 1]** execute one or more of the preconfigured workflows. Each workflow will be executed `EVALUATION_REPEATS` times for every experimental configuration defined in [./evaluation/templates/](./evaluation/templates/).
All workflow execution and experimental automation logic are defined in the main Makefile. Note: The name of the workflow must end with .yaml. This triggers the execution of the workflow and the generation of the runtime system output.

```sh
make C_montage-chameleon-2mass-01d-001.yaml
```

4. **[Option 2]** generate a slurm submission file for the Slurm Workload Manager System. Note: The name of the workflow must end with .slurm.

```sh
make C_montage-chameleon-2mass-01d-001.slurm
```

The system will create a single submission file per experimental configuration file defined in [./evaluation/templates/](./evaluation/templates/).

```sh 
[INFO] Generating SLURM job for: S_montage-chameleon-2mass-015d-001
  [SUCCESS] ./results/slurm/S_montage-chameleon-2mass-015d-001/fifo_8/1_M_1/submit.sbatch
```

5. Results will be stored in a newly created `results` folder. This folder will include the configuration files generated from [./evaluation/templates/](./evaluation/templates/), the log files (`.log`) produced during each repetition, the output files (`.yaml`) for each repetition, and the Slurm submission file if Option 2 is selected.

```sh
results/
├── config
│   └── S_montage-chameleon-2mass-015d-001
│       └── fifo_8
│           └── 1_M_1
│               └── config.json
├── log
│   └── S_montage-chameleon-2mass-015d-001
│       ├── fifo_8
│       │   └── 1_M_1
│       │       └── 1.log
│       └── log.txt
├── output
│   └── S_montage-chameleon-2mass-015d-001
│       └── fifo_8
│           └── 1_M_1
│               └── 1.yaml
└── slurm
    ├── S_montage-chameleon-2mass-015d-001
    │   └── fifo_8
    │       └── 1_M_1
    │           └── submit.sbatch
    └── S_montage-chameleon-2mass-015d-001.slurm
````
