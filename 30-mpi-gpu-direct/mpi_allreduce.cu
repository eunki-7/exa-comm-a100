/*
 * CUDA-aware MPI AllReduce example
 * - Allocates a GPU buffer, initializes with rank value,
 *   performs MPI_Allreduce (SUM) in-place on device memory,
 *   prints a few sample values from rank 0.
 * - Demonstrates GPUDirect RDMA path when supported (no CPU staging).
 */
#include <mpi.h>
#include <cuda_runtime.h>
#include <cstdio>
#include <vector>

#define CHECK_MPI(x) do { int e=(x); if(e!=MPI_SUCCESS){ fprintf(stderr,"MPI error %d\n",e); MPI_Abort(MPI_COMM_WORLD,e);} } while(0)
#define CHECK_CUDA(x) do { cudaError_t e=(x); if(e!=cudaSuccess){ fprintf(stderr,"CUDA error %s\n",cudaGetErrorString(e)); MPI_Abort(MPI_COMM_WORLD,1);} } while(0)

int main(int argc, char** argv){
    CHECK_MPI(MPI_Init(&argc, &argv));
    int rank,size; CHECK_MPI(MPI_Comm_rank(MPI_COMM_WORLD,&rank)); CHECK_MPI(MPI_Comm_size(MPI_COMM_WORLD,&size));

    int devCount=0; CHECK_CUDA(cudaGetDeviceCount(&devCount));
    int dev = rank % (devCount>0?devCount:1);
    CHECK_CUDA(cudaSetDevice(dev));

    const int N = 1<<20; // 1M floats (~4MB)
    float *d_buf=nullptr;
    CHECK_CUDA(cudaMalloc(&d_buf, N*sizeof(float)));

    // initialize: fill with 'rank'
    std::vector<float> h_tmp(N, (float)rank);
    CHECK_CUDA(cudaMemcpy(d_buf, h_tmp.data(), N*sizeof(float), cudaMemcpyHostToDevice));

    // Allreduce directly on GPU memory (CUDA-aware MPI)
    CHECK_MPI(MPI_Allreduce(MPI_IN_PLACE, d_buf, N, MPI_FLOAT, MPI_SUM, MPI_COMM_WORLD));

    if(rank==0){
        std::vector<float> out(4);
        CHECK_CUDA(cudaMemcpy(out.data(), d_buf, 4*sizeof(float), cudaMemcpyDeviceToHost));
        printf("[rank0] sample: %f %f %f %f | expected sum 0..%d = %d\n",
               out[0], out[1], out[2], out[3], size-1, (size*(size-1))/2);
    }

    CHECK_CUDA(cudaFree(d_buf));
    MPI_Finalize();
    return 0;
}
