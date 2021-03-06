/**
 * \file generator.h
 *
 * Fake data generation.
 */

#ifndef __GENERATOR_H__
#define __GENERATOR_H__

#include <utility>
#include <vector>
#include <TH1.h>

#include <sxmc/signals.h>

/**
 * Make a fake data set.
 *
 * Create a fake data set by extracting ROOT histograms from each signal's PDF
 * and sampling. Only works for signals with 1, 2, or 3 observables.
 * Systematics are applied to PDFs before sampling.
 *
 * The output array of sampled observations is in a row-major format like:
 *
 *         obs.0 obs.1 obs.2
 *     ev0   0     1     2
 *     ev1   3     4     5
 *
 * \param signals List of Signals defining the PDFs to sample
 * \param systematics List of Systematics to be applied to PDFs
 * \param observables List of Observables common to all PDFs
 * \param params List of parameters (normalizations then systematics)
 * \param poisson If true, Poisson-distribute the signal rates
 * \return Array with samples
 */
std::pair<std::vector<float>, std::vector<int> >
make_fake_dataset(std::vector<Signal>& signals,
                  std::vector<Systematic>& systematics,
                  std::vector<Observable>& observables,
                  std::vector<double> params,
                  bool poisson=true, int maxsamples=1e7);


/**
 * Sample a ROOT TH1 (TH1, TH2, or TH3) histogram.
 */
std::pair<std::vector<float>, std::vector<int> >
sample_pdf(TH1* hist, long int nsamples, long int maxsamples=1e7);


/**
 * TMath::Nint clone to avoid clash with CUDA headers.
 */
unsigned nint(float nexpected);

#endif  // __GENERATOR_H__

