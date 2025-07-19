#!/bin/bash

rm ./workflows/cycles/raw/cycles-chameleon-5l-3c-9p-001.json
rm ./workflows/cycles/raw/cycles-chameleon-1l-1c-12p-001.json
rm ./workflows/soykb/raw/soykb-chameleon-30fastq-20ch-001.json
rm ./workflows/soykb/raw/soykb-chameleon-20fastq-10ch-001.json
rm ./workflows/srasearch/raw/srasearch-chameleon-40a-003.json
rm ./workflows/srasearch/raw/srasearch-chameleon-20a-002.json
rm ./workflows/epigenomics/raw/epigenomics-chameleon-ilmn-6seq-100k-001.json
rm ./workflows/epigenomics/raw/epigenomics-chameleon-hep-2seq-50k-001.json
rm ./workflows/montage/raw/montage-chameleon-dss-10d-001.json
rm ./workflows/montage/raw/montage-chameleon-dss-075d-001.json
rm ./workflows/montage/raw/montage-chameleon-2mass-01d-001.json

python3 ./scripts/generate_workflows.sh ../workflows/cycles/raw ./workflows
python3 ./scripts/generate_workflows.sh ../workflows/soykb/raw ./workflows
python3 ./scripts/generate_workflows.sh ../workflows/srasearch/raw ./workflows
python3 ./scripts/generate_workflows.sh ../workflows/epigenomics/raw ./workflows
python3 ./scripts/generate_workflows.sh ../workflows/montage/raw ./workflows

git checkout ../workflows
