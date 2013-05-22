/**
 * \file nll_kernels.h
 * \brief CUDA/HEMI kernels supporting NLL calculation
 */

#ifndef __NLL_H__
#define __NLL_H__

#include <cuda.h>
#include <hemi/hemi.h>

#ifdef __CUDACC__
#include <curand_kernel.h>
#endif

#ifndef __HEMI_ARRAY_H__
#define __HEMI_ARRAY_H__
#include <hemi/array.h>
#endif

/**
 * \typedef RNGState
 * \brief Defines RNG for CURAND, ignored in CPU mode
 */
#ifdef __CUDACC__
typedef curandStateXORWOW RNGState;
#else
typedef int RNGState;
#endif

class TNtuple;

#ifdef __CUDACC__
/**
 * Initialize device-side RNGs.
 *
 * Generators all have the same seed but a different offset in the sequence.
 *
 * \param nthreads Number of threads (same as the number of states)
 * \param seed Random seed shared by all generators
 * \param state Array of CUDA RNG states
 */
__global__ void init_device_rngs(int nthreads, unsigned long long seed,
                                 curandState* state);
#endif


/**
 * Pick a new position distributed around the given one.
 *
 * Uses CURAND XORWOW generator on GPU, or ROOT's gRandom on the CPU.
 *
 * \param nthreads Number of threads == length of vectors
 * \param rng CUDA RNG states, ignored on CPU
 * \param sigma Standard deviation to sample
 * \param current_vector Vector of current parameters
 * \param proposed_vector Output vector of proposed parameters
 */
HEMI_KERNEL(pick_new_vector)(int nthreads, RNGState* rng, float sigma,
                             const float* current_vector,
                             float* proposed_vector);

/**
 * Decide whether to accept a random MCMC step
 *
 * Compare likelihoods of current and proposed parameter vectors. If the step
 * is accepted, store it in a buffer which can be flushed periodically,
 * minimizing transfer overhead.
 *
 * The step buffer is an (Nsignals + 1 x Nsteps) matrix, where the last column
 * contains the likelihood value.
 *
 * \param rng Random-number generator states, used in GPU mode only
 * \param nll_current The NLL of the current parameters
 * \param nll_proposed the NLL of the proposed parameters
 * \param v_current The current parameters
 * \param v_proposed The proposed parameters
 * \param ns The number of signals
 * \param counter The number of steps in the buffer
 * \param jump_buffer The step buffer
 */
HEMI_KERNEL(jump_decider)(RNGState* rng, double* nll_current,
                          const double* nll_proposed, float* v_current,
                          const float* v_proposed, unsigned ns, int* counter,
                          float* jump_buffer);

/**
 * NLL Part 1
 *
 * Calculate -sum(log(sum(Nj * Pj(xi)))) contribution to NLL.
 *
 * \param lut Pj(xi) lookup table
 * \param pars Event rates (normalizations) for each signal
 * \param ne Number of events in the data
 * \param ns Number of signals
 * \param sums Output sums for subsets of events
 */
HEMI_KERNEL(nll_event_chunks)(const float* lut, const float* pars,
                              const size_t ne, const size_t ns,
                              double* sums);


/**
 * NLL Part 2
 *
 * Total up the partial sums from Part 1
 *
 * \param nthreads Number of threads == number of sums to total
 * \param sums The partial sums
 * \param total_sum Output: the total sum
 */
HEMI_KERNEL(nll_event_reduce)(const size_t nthreads, const double* sums,
                              double* total_sum);


/**
 * NLL Part 3
 *
 * Calculate overall normalization and constraints contributions to NLL, add
 * in the event term to get the total.
 *
 * \param ns Number of signals
 * \param pars Event rates (normalizations) for each signal
 * \param expectations Expected rates for each signal
 * \param constraints Fractional constraints for each signal
 * \param events_total Sum of event term contribution
 * \param nll The total NLL
 */
HEMI_KERNEL(nll_total)(const size_t ns, const float* pars,
                       const float* expectations,
                       const float* constraints,
                       const double* events_total,
                       double* nll);

#endif  // __NLL_H__
