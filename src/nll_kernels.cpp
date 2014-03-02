#include <iostream>
#include <cmath>
#include <hemi/hemi.h>
#include <TRandom.h>

#include <sxmc/nll_kernels.h>

#ifdef __CUDACC__
#include <curand_kernel.h>
#else
#include <TMath.h>
#endif

#ifndef __HEMI_ARRAY_H__
#define __HEMI_ARRAY_H__
#include <hemi/array.h>
#endif

#ifdef __CUDACC__
__global__ void init_device_rngs(int nthreads, unsigned long long seed,
                                 curandStateXORWOW* state) {
  int idx = hemiGetElementOffset();
  if (idx >= nthreads) {
    return;
  }
  curand_init(seed, idx, 0, &state[idx]);
}
#endif


HEMI_DEV_CALLABLE_INLINE
void pick_new_vector_device(int nthreads, RNGState* rng,
                            const float* sigma,
                            const double* current_vector,
                            double* proposed_vector) {
  int offset = hemiGetElementOffset();
  int stride = hemiGetElementStride();

  for (int i=offset; i<(int)nthreads; i+=stride) {
#ifdef HEMI_DEV_CODE
    double u = curand_normal(&rng[i]);
    proposed_vector[i] = current_vector[i] + sigma[i] * u;
#else
    double u = gRandom->Gaus(current_vector[i], sigma[i]);
    proposed_vector[i] = u;
#endif
  }
}


HEMI_DEV_CALLABLE_INLINE
void jump_decider_device(RNGState* rng, double* nll_current,
                         const double* nll_proposed, double* v_current,
                         const double* v_proposed, unsigned nparameters,
                         int* accepted, int* counter, float* jump_buffer,
                         const bool debug_mode=false) {
#ifdef HEMI_DEV_CODE
  double u = curand_uniform(&rng[0]);
#else
  double u = gRandom->Uniform();
#endif

  // metropolis algorithm
  double np = nll_proposed[0];
  double nc = nll_current[0];
  if (debug_mode || (np < nc || u <= exp(nc - np))) {
    nll_current[0] = np;
    for (unsigned i=0; i<nparameters; i++) {
      v_current[i] = v_proposed[i];
    }
    accepted[0] += 1;
  }

  // append all steps to jump buffer
  int count = counter[0];
  for (unsigned i=0; i<nparameters; i++) {
    jump_buffer[count * (nparameters + 1) + i] = v_current[i];
  }
  jump_buffer[count * (nparameters + 1) + nparameters] = nll_current[0];
  counter[0] = count + 1;    
}


HEMI_KERNEL(nll_event_chunks)(const float* __restrict__ lut,
                              const int* __restrict__ dataweights,
                              const double* __restrict__ pars,
                              const size_t ne, const size_t ns,
                              double* sums) {
  int offset = hemiGetElementOffset();
  int stride = hemiGetElementStride();

  double sum = 0;
  for (int i=offset; i<(int)ne; i+=stride) {
    double s = 0;
    for (size_t j=0; j<ns; j++) {
      float v = lut[j * ne + i];
      s += pars[j] * (!isnan(v) ? v : 0);  // handle nans from empty hists
    }
    sum += logf(s) * dataweights[i];
  }
  if (!isnan(sum)) {
    sums[offset] = sum;
  }
}


HEMI_DEV_CALLABLE_INLINE
void nll_event_reduce_device(const size_t nthreads, const double* sums,
                             double* total_sum) {
  int offset = hemiGetElementOffset();
  int stride = hemiGetElementStride();

#ifdef HEMI_DEV_CODE
  extern __shared__ double block_sums[];
#else
  double block_sums[1];
#endif

  double thread_sum = 0.0;
  for (int i=offset; i<(int)nthreads; i+=stride) {
    thread_sum += sums[i];
  }
  block_sums[offset] = thread_sum;

#ifdef HEMI_DEV_CODE
  // Shared memory reduction
  __syncthreads();
  for (int i = blockDim.x/2; i > 0; i >>= 1)
    if (threadIdx.x < i)
      block_sums[threadIdx.x] += block_sums[threadIdx.x + i];
  __syncthreads();
#endif
  total_sum[0] = block_sums[0];
}


HEMI_DEV_CALLABLE_INLINE
void nll_total_device(const size_t nparameters, const size_t nsignals,
                      const double* pars,
                      const double* means,
                      const double* sigmas,
                      const double* events_total,
                      double* nll) {
  // total from sum over events, once
  double sum = -events_total[0];

  if (isnan(sum)) {
    nll[0] = 1e18;
    return;
  }

  for (unsigned i=0; i<nparameters; i++) {
    // non-negative rates
    if (i < nsignals && pars[i] < 0) {
      nll[0] = 1e18;
      return;      
    }

    // normalization constraints
    if (i < nsignals) {
      sum += pars[i];
    }

    // gaussian constraints
    if (sigmas[i] > 0) {
      double x = (pars[i] - means[i]) / sigmas[i];
      sum += x * x;
    }
  }

  nll[0] = sum;
}


HEMI_KERNEL(pick_new_vector)(int nthreads, RNGState* rng,
                             const float* sigma,
                             const double* current_vector,
                             double* proposed_vector) {
  pick_new_vector_device(nthreads, rng, sigma, current_vector,
                         proposed_vector);
}


HEMI_KERNEL(jump_decider)(RNGState* rng, double* nll_current,
                          const double* nll_proposed, double* v_current,
                          const double* v_proposed, unsigned nparameters,
                          int* accepted, int* counter, float* jump_buffer) {
  jump_decider_device(rng, nll_current, nll_proposed, v_current, v_proposed,
                      nparameters, accepted, counter, jump_buffer);
}


HEMI_KERNEL(nll_event_reduce)(const size_t nthreads, const double* sums,
                              double* total_sum) {
  nll_event_reduce_device(nthreads, sums, total_sum);
}


HEMI_KERNEL(nll_total)(const size_t npars, const double* pars,
                       const size_t nsignals,
                       const double* means, const double* sigmas,
                       const double* events_total,
                       double* nll) {
  nll_total_device(npars, nsignals, pars, means, sigmas, events_total, nll);
}


HEMI_KERNEL(finish_nll_jump_pick_combo)(const size_t npartial_sums,
                                        const double* sums, const size_t ns,
                                        const double* means,
                                        const double* sigmas,
                                        RNGState* rng,
                                        double *nll_current,
                                        double *nll_proposed,
                                        double *v_current, double *v_proposed,
                                        int* accepted, int* counter,
                                        float* jump_buffer, int nparameters,
                                        const float* sigma,
                                        const bool debug_mode) {
  double total_sum;

  nll_event_reduce_device(npartial_sums, sums, &total_sum);

#ifdef HEMI_DEV_CODE
  __syncthreads();
#endif

  if (hemiGetElementOffset() == 0) {
    nll_total_device(nparameters, ns, v_proposed, means, sigmas, &total_sum,
                     nll_proposed);

    jump_decider_device(rng, nll_current, nll_proposed, v_current, v_proposed,
                        nparameters, accepted, counter, jump_buffer,
                        debug_mode);
  }

#ifdef HEMI_DEV_CODE
  __syncthreads();
#endif

  pick_new_vector_device(nparameters, rng, sigma, v_current, v_proposed);
}

