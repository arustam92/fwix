#pragma once
#include "CudaOperator.h"
#include <complex5DReg.h>
#include <complex2DReg.h>
#include <prop_kernels.cuh>

using namespace SEP;

// operator injecting a wavelet or data into the set of wavefields: [Ns, Nw, Nx, Ny]
class Injection : public CudaOperator<complex2DReg, complex5DReg>  {
public:
  Injection(const std::shared_ptr<hypercube>& domain,const std::shared_ptr<hypercube>& range, 
  const std::vector<float>& cx, const std::vector<float>& cy, const std::vector<float>& cz, const std::vector<int>& ids, 
  complex_vector* model = nullptr, complex_vector* data = nullptr, dim3 grid=1, dim3 block=1);
  
  ~Injection() {
    CHECK_CUDA_ERROR(cudaFree(d_cx));
    CHECK_CUDA_ERROR(cudaFree(d_cy));
    CHECK_CUDA_ERROR(cudaFree(d_ids));
  };

  void cu_forward (bool add, const complex_vector* __restrict__ model, complex_vector* __restrict__ data);
  void cu_adjoint (bool add, complex_vector* __restrict__ model, const complex_vector* __restrict__ data);

  void set_coords(const std::vector<float>& cx, const std::vector<float>& cy, const std::vector<float>& cz, const std::vector<int>& ids) {
    CHECK_CUDA_ERROR(cudaMemcpyAsync(d_cx, cx.data(), sizeof(float)*ntrace, cudaMemcpyHostToDevice));
    CHECK_CUDA_ERROR(cudaMemcpyAsync(d_cy, cy.data(), sizeof(float)*ntrace, cudaMemcpyHostToDevice));
    CHECK_CUDA_ERROR(cudaMemcpyAsync(d_cz, cz.data(), sizeof(float)*ntrace, cudaMemcpyHostToDevice));
    CHECK_CUDA_ERROR(cudaMemcpyAsync(d_ids, ids.data(), sizeof(int)*ntrace, cudaMemcpyHostToDevice));
  };

private:
  Injection_launcher launcher;
  float *d_cx, *d_cy, *d_cz;
  int *d_ids;
  const std::vector<float> _cx, _cy, _cz;
  const std::vector<int> _ids;
  int ntrace;
};
