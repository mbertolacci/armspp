# armspp

[![Travis build status](https://travis-ci.org/mbertolacci/armspp.svg?branch=master)](https://travis-ci.org/mbertolacci/armspp) [![Coverage status](https://codecov.io/gh/mbertolacci/armspp/branch/master/graph/badge.svg)](https://codecov.io/github/mbertolacci/armspp?branch=master)

This R package provides a header-only C++ implementation of [Adaptive Rejection Metropolis Sampling (ARMS)](http://www1.maths.leeds.ac.uk/~wally.gilks/adaptive.rejection/web_page/Welcome.html), along with R packages for the same purpose. The C++ implementation is not tied to R and can be used outside the context of this package.

## Installation

This package is not in CRAN right now. Just install using devtools and github

    devtools::install_github('mbertolacci/armspp')

## License

The code is released under the MIT license.
