#!/usr/bin/env bash
# Quick sanity checks for GPU, RDMA, and network stack.
set -euo pipefail
echo "== NVIDIA GPU =="
nvidia-smi || echo "nvidia-smi not available"
echo
echo "== RDMA devices =="
which ibv_devinfo >/dev/null 2>&1 && ibv_devinfo | egrep 'hca_id|link_layer|state' || echo "ibv_devinfo not available"
echo
echo "== RDMA CM =="
which rdma_cm >/dev/null 2>&1 && rdma_cm || echo "rdma_cm not installed"
echo
echo "== Network Interfaces =="
ip -br a
echo
echo "== Suggested NCCL ENV =="
cat <<EOF
export NCCL_SOCKET_IFNAME=ib0
export NCCL_IB_HCA=mlx5
export NCCL_DEBUG=INFO
EOF
