#include <Rcpp.h>
#include <random>
#include <armspp>

#include "utils.hpp"

using namespace Rcpp;
using armspp::ARMS;
using armspp::listToDottedPair;

// [[Rcpp::export(name = '.arms_gibbs2')]]
RObject arms_gibbs2(
  int nSamples,
  NumericVector previous,
  Function logPdf,
  NumericVector lower, NumericVector upper,
  List initial,
  NumericVector convex,
  IntegerVector maxPoints,
  IntegerVector metropolis,
  bool includeNEvaluations
) {
  std::mt19937_64 rng(static_cast<uint_fast64_t>(UINT_FAST64_MAX * R::unif_rand()));
  int nDimensions = previous.size();

  NumericMatrix samples(nSamples, nDimensions);
  NumericVector current(previous);
  int nEvaluations = 0;
  int p;
  auto logPdfLambda = [&](double x) -> double {
    current[p] = x;
    ++nEvaluations;
    return as<NumericVector>(logPdf(current, p))[0];
  };

  for (int i = 0; i < nSamples; ++i) {
    for (p = 0; p < nDimensions; ++p) {
      NumericVector initialVector = initial[p % initial.size()];

      ARMS<double, decltype(logPdfLambda), NumericVector::iterator> dist(
        logPdfLambda,
        lower[p % lower.size()],
        upper[p % upper.size()],
        convex[p % convex.size()],
        initialVector.begin(), initialVector.size(),
        maxPoints[p % maxPoints.size()],
        metropolis[p % metropolis.size()],
        current[p % previous.size()]
      );

      current[p] = dist(rng);

      samples(i, p) = current[p];
    }
  }
  Nullable<CharacterVector> names = previous.names();
  colnames(samples) = names;

  if (includeNEvaluations) {
    List output;
    output["n_evaluations"] = nEvaluations;
    output["samples"] = samples;
    return output;
  } else {
    return samples;
  }
}
