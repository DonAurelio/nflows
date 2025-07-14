
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

