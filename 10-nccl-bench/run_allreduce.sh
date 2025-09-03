#!/usr/bin/env bash
# Run NCCL all_reduce_perf across 10 ranks and save logs.
# Usage: bash run_allreduce.sh ./hostfile.10xA100
set -euo pipefail
HF=${1:-./hostfile.10xA100}
: ${NCCL_SOCKET_IFNAME:=ib0}
: ${NCCL_IB_HCA:=mlx5}
: ${NCCL_DEBUG:=INFO}
mkdir -p logs
echo "[INFO] NCCL all_reduce_perf (10 ranks)"
mpirun -np 10 -hostfile "$HF"   -x NCCL_DEBUG -x NCCL_SOCKET_IFNAME -x NCCL_IB_HCA   --mca btl ^openib --mca pml ucx   /opt/nccl-tests/build/all_reduce_perf -b 8 -e 1G -f 2 -g 1 | tee logs/allreduce.log
