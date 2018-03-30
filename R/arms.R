#' Adaptive Rejection Metropolis Sampling
#' @param n_samples Number of samples to return
#' @param log_pdf Potentially unnormalised log density from which to sample
#' @param lower Lower bound of the support of the distribution
#' @param upper Upper bound of the support of the distribution
#' @param x_initial Initial points with which to build the rejection distribution
#' @param n_initial Number of points used to form `x_initial`; ignored if `x_initial` provided.
#' @param convex Convexity adjustment
#' @param max_points Maximum number of points to allow in the rejection distribution
#' @param metropolis Whether to use a Metropolis-Hastings step after rejection sampling
#' @param x_previous The previous value of the Markov chain to be used if `metropolis = TRUE`
#' @param include_neval Whether to return an object specifying the number of function evaluations used
#' @return Vector of samples if include_neval is FALSE, otherwise a list
#' @seealso \url{http://www1.maths.leeds.ac.uk/~wally.gilks/adaptive.rejection/web_page/Welcome.html}
#' @references Gilks, W. R., Best, N. G. and Tan, K. K. C. (1995) Adaptive rejection Metropolis sampling. Applied Statistics, 44, 455-472.
#' @export
arms <- function(
  n_samples,
  log_pdf,
  lower,
  upper,
  x_initial = lower + (1 : n_initial) * (upper - lower) / (n_initial + 1),
  n_initial = 10,
  convex = 0,
  max_points = 100,
  metropolis = FALSE,
  x_previous = NA,
  include_neval = FALSE
) {
  .arms(
    x_initial,
    lower,
    upper,
    log_pdf,
    convex,
    max_points,
    metropolis,
    x_previous,
    n_samples,
    include_neval
  )
}