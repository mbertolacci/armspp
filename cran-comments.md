This fixes a memory leak reported to me by Prof Brian Ripley.

The leak entered via the linked to `progress` package. I have removed this dependency by replicating the necessary functionality with my own code.

The package functionality has not changed since the last release.

## Test environments

* local OS X install, R 3.6.0
* ubuntu 14.04.5 LTS (on travis-ci), R 3.6.0
* win-builder (release)
* rhub (Windows Server 2008 R2 SP1, R-devel, 32/64 bit)

## R CMD check results

There were no ERRORs, WARNINGs or NOTEs (excepting the first submission NOTE).

## Downstream dependencies

There are currently no downstream dependencies for this package.
