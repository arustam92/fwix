#pragma once
#include <CudaOperator.h>
#include <complex4DReg.h>
#include <prop_kernels.cuh>

using namespace SEP;

class Selector : public CudaOperator<complex4DReg,complex4DReg>
{

public:

	Selector(const std::shared_ptr<hypercube>& domain, 
	complex_vector* model = nullptr, complex_vector* data = nullptr, dim3 grid=1, dim3 block=1) 
	: CudaOperator<complex4DReg, complex4DReg>(domain, domain, model, data, grid, block) {
		_grid_ = {128, 128, 8};
  	_block_ = {16, 16, 2};

		_size_ = domain->getAxis(1).n * domain->getAxis(2).n * domain->getAxis(3).n;
		CHECK_CUDA_ERROR(cudaMalloc((void **)&d_labels, sizeof(int)*_size_));
		launcher = Selector_launcher(&select_forward, _grid_, _block_);
	};
	
	~Selector() {
		CHECK_CUDA_ERROR(cudaFree(d_labels));
	};

	void set_labels(int* labels) {
		// labels are 3D -- (x,y,w)
		CHECK_CUDA_ERROR(cudaMemcpyAsync(d_labels, labels, sizeof(int)*_size_, cudaMemcpyHostToDevice));
	};
	void set_value(int value) {_value_ = value;}

	void cu_forward(bool add, const complex_vector* __restrict__ model, complex_vector* __restrict__ data) {
		if (!add) data->zero();
		launcher.run_fwd(model, data, _value_, d_labels);
	};
	void cu_adjoint(bool add, complex_vector* __restrict__ model, const complex_vector* __restrict__ data) {
		if (!add) model->zero();
		launcher.run_fwd(data, model, _value_, d_labels);
	};

private:
	int _value_;
	int _size_;
	int *d_labels;
	Selector_launcher launcher;

};

