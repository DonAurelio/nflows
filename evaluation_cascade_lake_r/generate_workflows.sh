#!/bin/bash

rm ../workflows/cycles/raw/cycles-chameleon-5l-3c-9p-001.json
rm ../workflows/cycles/raw/cycles-chameleon-1l-1c-12p-001.json
rm ../workflows/soykb/raw/soykb-chameleon-30fastq-20ch-001.json
rm ../workflows/soykb/raw/soykb-chameleon-20fastq-10ch-001.json
rm ../workflows/srasearch/raw/srasearch-chameleon-40a-003.json
rm ../workflows/srasearch/raw/srasearch-chameleon-20a-002.json
rm ../workflows/epigenomics/raw/epigenomics-chameleon-ilmn-6seq-100k-001.json
rm ../workflows/epigenomics/raw/epigenomics-chameleon-hep-2seq-50k-001.json

cd ../scripts

bash generate_workflows.sh ../workflows/cycles/raw ../evaluation_cascade_lake_r/workflows
bash generate_workflows.sh ../workflows/soykb/raw ../evaluation_cascade_lake_r/workflows
bash generate_workflows.sh ../workflows/srasearch/raw ../evaluation_cascade_lake_r/workflows
bash generate_workflows.sh ../workflows/epigenomics/raw ../evaluation_cascade_lake_r/workflows

cd -

git checkout ../workflows
