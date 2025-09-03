# Build a CUDA-aware MPI example (requires MPI with CUDA support)
NVCC ?= nvcc
CXXFLAGS := -O2 -std=c++17
LDFLAGS := -lcudart
all: mpi_allreduce_gpu
mpi_allreduce_gpu: mpi_allreduce.cu
	$(NVCC) $(CXXFLAGS) -o $@ $< $(LDFLAGS)
clean:
	rm -f mpi_allreduce_gpu
