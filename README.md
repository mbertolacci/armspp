# armspp

[![CRAN
status](https://www.r-pkg.org/badges/version/armspp)](https://cran.r-project.org/package=armspp) [![Travis build status](https://travis-ci.org/mbertolacci/armspp.svg?branch=master)](https://travis-ci.org/mbertolacci/armspp) [![Coverage status](https://codecov.io/gh/mbertolacci/armspp/branch/master/graph/badge.svg)](https://codecov.io/github/mbertolacci/armspp?branch=master)

This R package provides a header-only C++ implementation of [Adaptive Rejection Metropolis Sampling (ARMS)](http://www1.maths.leeds.ac.uk/~wally.gilks/adaptive.rejection/web_page/Welcome.html), along with R packages for the same purpose. The C++ implementation is not tied to R and can be used outside the context of this package.

## Installation

This package is in CRAN, so it can be installed with

    install.packages('armspp')

## Adaptive Rejection Metropolis Sampling (ARMS)

[Adaptive Rejection Metropolis Sampling (ARMS)](http://www1.maths.leeds.ac.uk/~wally.gilks/adaptive.rejection/web_page/Welcome.html) is a Markov chain Monte Carlo based algorithm to sample from a univariate target distribution specified by its (potentially unnormalised) log density. The algorithm constructs a rejection distribution based on piecewise linear functions that envelop the log density of the target. For distributions with [log concave](https://en.wikipedia.org/wiki/Logarithmically_concave_function) density functions, this envelope is used directly, and usually results in a very efficient sampler. For distributions that are not log concave, or have unknown log concavity, an extra [Metropolis Hastings accept/reject](https://en.wikipedia.org/wiki/Metropolis%E2%80%93Hastings_algorithm) step is used to correct for any mismatch between the proposal density and the target. This sometimes results in a sampler with good convergence properties.

## Examples

Here is a simple (and somewhat useless) example, sampling from a standard normal distribution:

```{r}
samples <- arms(
  1000, dnorm, -1000, 1000, metropolis = FALSE, arguments = list(log = TRUE)
)
hist(samples, freq = FALSE, br = 20)
curve(dnorm(x), min(samples), max(samples), col = 'red', add = TRUE)
```

We set `metropolis = FALSE` because the normal density is known to be log concave.

Here is a more complicated example, sampling from a mixture of normals:

```{r}
dnormmixture <- function(x) {
  parts <- log(c(0.4, 0.6)) + dnorm(x, mean = c(-1, 4), log = TRUE)
  log(sum(exp(parts - max(parts)))) + max(parts)
}
samples <- arms(1000, dnormmixture, -1000, 1000)
hist(samples, freq = FALSE)
```

Now we leave the `metropolis` parameter at its default value `TRUE`, because we know that a mixture of normals density is not log concave (though it is piecewise log concave).

## License

The code is released under the MIT license.
