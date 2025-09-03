# Build environment for nccl-tests on CUDA 12.4
FROM nvidia/cuda:12.4.1-devel-ubuntu22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
      git build-essential make cmake numactl hwloc \
      rdma-core ibverbs-utils openssh-client \
    && rm -rf /var/lib/apt/lists/*

# Clone and build nccl-tests with MPI support
RUN git clone https://github.com/NVIDIA/nccl-tests.git /opt/nccl-tests && \
    make -C /opt/nccl-tests MPI=1 CUDA_HOME=/usr/local/cuda

WORKDIR /workspace
ENV NCCL_DEBUG=INFO
