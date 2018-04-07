/* adaptive rejection metropolis sampling */
/* adapted to Rcpp by mbertolacci */

/* *********************************************************************** */

#include <Rcpp.h>
#include "utils.hpp"

using namespace Rcpp;
using armspp::listToDottedPair;

/* *********************************************************************** */

typedef struct point {    /* a point in the x,y plane */
  double x,y;             /* x and y coordinates */
  double ey;              /* exp(y-ymax+YCEIL) */
  double cum;             /* integral up to x of rejection envelope */
  int f;                  /* is y an evaluated point of log-density */
  struct point *pl,*pr;   /* envelope points to left and right of x */
} POINT;

/* *********************************************************************** */

typedef struct envelope {  /* attributes of the entire rejection envelope */
  int cpoint;              /* number of POINTs in current envelope */
  int npoint;              /* max number of POINTs allowed in envelope */
  int *neval;              /* number of function evaluations performed */
  double ymax;             /* the maximum y-value in the current envelope */
  POINT *p;                /* start of storage of envelope POINTs */
  double convex;          /* adjustment for convexity */
} ENVELOPE;

/* *********************************************************************** */

typedef struct metropolis { /* for metropolis step */
  int on;            /* whether metropolis is to be used */
  double xprev;      /* previous Markov chain iterate */
  double yprev;      /* current log density at xprev */
} METROPOLIS;

/* *********************************************************************** */

#define XEPS  0.00001            /* critical relative x-value difference */
#define YEPS  0.1                /* critical y-value difference */
#define EYEPS 0.001              /* critical relative exp(y) difference */
#define YCEIL 50.                /* maximum y avoiding overflow in exp(y) */

/* *********************************************************************** */

/* declarations for functions defined in this file */

int arms_internal (double *xinit, int ninit, double xl, double xr, 
          Function lpdf,
          double convex, int npoint, int dometrop, double xprev, double *xsamp,
          int nsamp, int *neval,
          SEXP arguments);

int initial (double *xinit, int ninit, double xl, double xr, int npoint,
             Function lpdf, ENVELOPE *env, double convex, int *neval,
             METROPOLIS *metrop, SEXP arguments);

void sample(ENVELOPE *env, POINT *p);

void invert(double prob, ENVELOPE *env, POINT *p);

int test(ENVELOPE *env, POINT *p, Function lpdf, METROPOLIS *metrop, SEXP arguments);

int update(ENVELOPE *env, POINT *p, Function lpdf, METROPOLIS *metrop, SEXP arguments);

void cumulate(ENVELOPE *env);

int meet (POINT *q, ENVELOPE *env, METROPOLIS *metrop);

double area(POINT *q);

double expshift(double y, double y0);

double logshift(double y, double y0);

double perfunc(Function lpdf, ENVELOPE *env, double x, SEXP arguments);

/* *********************************************************************** */

// [[Rcpp::export(name = '.arms_gibbs')]]
NumericMatrix arms_gibbs(
  int nSamples,
  NumericVector xPrevious,
  Function logPdf,
  NumericVector xLower, NumericVector xUpper,
  List xInitial,
  NumericVector convex,
  IntegerVector maxPoints,
  IntegerVector metropolis
) {
  int nDimensions = xPrevious.size();

  NumericMatrix samples(nSamples, nDimensions);
  NumericVector xCurrent(xPrevious);
  for (int i = 0; i < nSamples; ++i) {
    for (int p = 0; p < nDimensions; ++p) {
      NumericVector xInitialVector = xInitial[p % xInitial.size()];
      int nEval;
      int err = arms_internal(
        xInitialVector.begin(),
        xInitialVector.size(),
        xLower[p % xLower.size()],
        xUpper[p % xUpper.size()],
        logPdf,
        convex[p % convex.size()],
        maxPoints[p % maxPoints.size()],
        metropolis[p % metropolis.size()],
        xCurrent[p],
        xCurrent.begin() + p,
        1,
        &nEval,
        DottedPair(xCurrent, p + 1)
      );
      if (err > 0) {
        stop("Failed with error %d", err);
      }
      samples(i, p) = xCurrent[p];
    }
  }
  Nullable<CharacterVector> names = xPrevious.names();
  colnames(samples) = names;

  return samples;
}

// [[Rcpp::export(name = '.arms')]]
RObject arms(
  List xInitial,
  NumericVector xl, NumericVector xr,
  List lpdf,
  NumericVector convex,
  IntegerVector npoint,
  IntegerVector dometrop,
  NumericVector xprev,
  List arguments,
  int nsamp,
  int include_neval
) {
  NumericVector xsamp(nsamp);
  int neval = 0;

  int maxArgumentsSize = 1;
  for (int i = 0; i < arguments.size(); ++i){
    if (::Rf_isVector(arguments[i]) && !Rf_isMatrix(arguments[i])) {
      maxArgumentsSize = std::max(maxArgumentsSize, static_cast<int>(
        static_cast<GenericVector>(
          arguments[i]
        ).size()
      ));
    }
  }

  if (
    xInitial.size() == 1
    && xl.size() == 1
    && xr.size() == 1
    && lpdf.size() == 1
    && convex.size() == 1
    && npoint.size() == 1
    && dometrop.size() == 1
    && xprev.size() == 1
    && maxArgumentsSize == 1
  ) {
    // Special case: only one distribution to sample from
    NumericVector xInitialVector = xInitial[0];
    DottedPair pArguments = listToDottedPair(arguments, 0);

    int err = arms_internal(
      xInitialVector.begin(), xInitialVector.size(),
      xl[0], xr[0], lpdf[0], convex[0], npoint[0], dometrop[0], xprev[0],
      xsamp.begin(), nsamp, &neval, pArguments
    );
    if (err > 0) {
      stop("Failed with error %d", err);
    }
  } else {
    for (int i = 0; i < nsamp; ++i) {
      NumericVector xInitialVector = xInitial[i % xInitial.size()];
      DottedPair pArguments = listToDottedPair(arguments, i);
      int thisNEval = 0;
      int err = arms_internal(
        xInitialVector.begin(),
        xInitialVector.size(),
        xl[i % xl.size()],
        xr[i % xr.size()],
        lpdf[i % lpdf.size()],
        convex[i % convex.size()],
        npoint[i % npoint.size()],
        dometrop[i % dometrop.size()],
        xprev[i % xprev.size()],
        xsamp.begin() + i,
        1,
        &thisNEval,
        pArguments
      );
      if (err > 0) {
        stop("Failed with error %d", err);
      }
      neval += thisNEval;
    }
  }

  if (include_neval) {
    List output;
    output["n_evaluations"] = neval;
    output["sample"] = xsamp;
    return output;
  } else {
    return xsamp;
  }
}

/* *********************************************************************** */

int arms_internal (
  double *xinit, int ninit, double xl, double xr,
  Function lpdf,
  double convex, int npoint, int dometrop, double xprev, double *xsamp,
  int nsamp, int *neval,
  SEXP arguments
) {

  ENVELOPE env;      /* rejection envelope */
  POINT pwork;        /* a working point, not yet incorporated in envelope */
  int msamp=0;        /* the number of x-values currently sampled */
  METROPOLIS metrop; /* to hold bits for metropolis step */
  int i,err;

  /* set up space required for envelope */
  metrop.on = dometrop;


  /* set up initial envelope */
  err = initial(xinit,ninit,xl,xr,npoint,lpdf,&env,convex,
        neval,&metrop,arguments);
  if(err)return err;


  /* finish setting up metropolis struct (can only do this after */
  /* setting up env) */
  if(metrop.on){
    if((xprev < xl) || (xprev > xr)){
      /* previous markov chain iterate out of range */
      return 1007;
    }
    metrop.xprev = xprev;
    metrop.yprev = perfunc(lpdf,&env,xprev,arguments);
  }

  /* now do adaptive rejection */
  do {
    /* sample a new point */
    sample (&env,&pwork);

    /* perform rejection (and perhaps metropolis) tests */
    i = test(&env,&pwork,lpdf,&metrop,arguments);
    if(i == 1){
      /* point accepted */
      xsamp[msamp++] = pwork.x;
    } else if (i != 0) {
      /* envelope error - violation without metropolis */
      return 2000;
    }  
  } while (msamp < nsamp);

  /* nsamp points now sampled */

  /* free space */
  free(env.p);

  return 0;
}

/* *********************************************************************** */

int initial (double *xinit, int ninit, double xl, double xr, int npoint,
         Function lpdf, ENVELOPE *env, double convex, int *neval,
             METROPOLIS *metrop, SEXP arguments)

/* to set up initial envelope */
/* xinit        : initial x-values */
/* ninit        : number of initial x-values */
/* xl,xr        : lower and upper x-bounds */
/* npoint       : maximum number of POINTs allowed in envelope */
/* *lpdf        : to evaluate log density */
/* *env         : rejection envelope attributes */
/* convex      : adjustment for convexity */
/* *neval       : current number of function evaluations */
/* *metrop      : for metropolis step */

{
  int i,j,k,mpoint;
  POINT *q;

  if(ninit<3){
    /* too few initial points */
    return 1001;
  }

  mpoint = 2*ninit + 1;
  if(npoint < mpoint){
    /* too many initial points */
    return 1002;
  }

  if((xinit[0] <= xl) || (xinit[ninit-1] >= xr)){
    /* initial points do not satisfy bounds */
    return 1003;
  }

  for(i=1; i<ninit; i++){
    if(xinit[i] <= xinit[i-1]){
      /* data not ordered */
      return 1004;
    }
  }

  if(convex < 0.0){
    /* negative convexity parameter */
    return 1008;
  }

  /* copy convexity address to env */
  env->convex = convex;

  /* copy address for current number of function evaluations */
  env->neval = neval;
  /* initialise current number of function evaluations */
  *(env->neval) = 0;

  /* set up space for envelope POINTs */
  env->npoint = npoint;
  env->p = (POINT *)malloc(npoint*sizeof(POINT));
  if(env->p == NULL){
    /* insufficient space */
    return 1006;
  }

  /* set up envelope POINTs */
  q = env->p;
  /* left bound */
  q->x = xl;
  q->f = 0;
  q->pl = NULL;
  q->pr = q+1;
  for(j=1, k=0; j<mpoint-1; j++){
    q++;
    if(j%2){
      /* point on log density */
      q->x = xinit[k++];
      q->y = perfunc(lpdf,env,q->x,arguments);
      q->f = 1;
    } else {
      /* intersection point */
      q->f = 0;
    }
    q->pl = q-1;
    q->pr = q+1;
  }
  /* right bound */
  q++;
  q->x = xr;
  q->f = 0;
  q->pl = q-1;
  q->pr = NULL;

  /* calculate intersection points */
  q = env->p;
  for (j=0; j<mpoint; j=j+2, q=q+2){
    if(meet(q,env,metrop)){
      /* envelope violation without metropolis */
      return 2000;
    }
  }

  /* exponentiate and integrate envelope */
  cumulate(env);

  /* note number of POINTs currently in envelope */
  env->cpoint = mpoint;

  return 0;
}

/* *********************************************************************** */

void sample(ENVELOPE *env, POINT *p)

/* To sample from piecewise exponential envelope */
/* *env    : envelope attributes */
/* *p      : a working POINT to hold the sampled value */

{
  double prob;

  /* sample a uniform */
  prob = R::unif_rand();
  /* get x-value correponding to a cumulative probability prob */
  invert(prob,env,p);

  return;
}

/* *********************************************************************** */

void invert(double prob, ENVELOPE *env, POINT *p)

/* to obtain a point corresponding to a qiven cumulative probability */
/* prob    : cumulative probability under envelope */
/* *env    : envelope attributes */
/* *p      : a working POINT to hold the sampled value */

{
  double u,xl,xr,yl,yr,eyl,eyr,prop;
  POINT *q;

  /* find rightmost point in envelope */
  q = env->p;
  while(q->pr != NULL)q = q->pr;

  /* find exponential piece containing point implied by prob */
  u = prob * q->cum;
  while(q->pl->cum > u)q = q->pl;

  /* piece found: set left and right POINTs of p, etc. */
  p->pl = q->pl;
  p->pr = q;
  p->f = 0;
  p->cum = u;

  /* calculate proportion of way through integral within this piece */
  prop = (u - q->pl->cum) / (q->cum - q->pl->cum);

  /* get the required x-value */
  if (q->pl->x == q->x){
    /* interval is of zero length */
    p->x = q->x;
    p->y = q->y;
    p->ey = q->ey;
  } else {
    xl = q->pl->x;
    xr = q->x;
    yl = q->pl->y;
    yr = q->y;
    eyl = q->pl->ey;
    eyr = q->ey;
    if(fabs(yr - yl) < YEPS){
      /* linear approximation was used in integration in function cumulate */
      if(fabs(eyr - eyl) > EYEPS*fabs(eyr + eyl)){
    p->x = xl + ((xr - xl)/(eyr - eyl))
           * (-eyl + sqrt((1. - prop)*eyl*eyl + prop*eyr*eyr));
      } else {
    p->x = xl + (xr - xl)*prop;
      }
      p->ey = ((p->x - xl)/(xr - xl)) * (eyr - eyl) + eyl;
      p->y = logshift(p->ey, env->ymax);
    } else {
      /* piece was integrated exactly in function cumulate */
      p->x = xl + ((xr - xl)/(yr - yl))
          * (-yl + logshift(((1.-prop)*eyl + prop*eyr), env->ymax));
      p->y = ((p->x - xl)/(xr - xl)) * (yr - yl) + yl;
      p->ey = expshift(p->y, env->ymax);
    }
  }

  /* guard against imprecision yielding point outside interval */
  if ((p->x < xl) || (p->x > xr)) stop("arms error 1");

  return;
}

/* *********************************************************************** */

int test(ENVELOPE *env, POINT *p, Function lpdf, METROPOLIS *metrop, SEXP arguments)

/* to perform rejection, squeezing, and metropolis tests */
/* *env          : envelope attributes */
/* *p            : point to be tested */
/* *lpdf         : to evaluate log-density */
/* *metrop       : data required for metropolis step */

{
  double u,y,ysqueez,ynew,yold,znew,zold,w;
  POINT *ql,*qr;
  
  /* for rejection test */
  u = R::unif_rand() * p->ey;
  y = logshift(u,env->ymax);

  if(!(metrop->on) && (p->pl->pl != NULL) && (p->pr->pr != NULL)){
    /* perform squeezing test */
    if(p->pl->f){
      ql = p->pl;
    } else {
      ql = p->pl->pl;
    }
    if(p->pr->f){
      qr = p->pr;
    } else {
      qr = p->pr->pr;
    }
    ysqueez = (qr->y * (p->x - ql->x) + ql->y * (qr->x - p->x))
               /(qr->x - ql->x);
    if(y <= ysqueez){
      /* accept point at squeezing step */
      return 1;
    }
  }

  /* evaluate log density at point to be tested */
  ynew = perfunc(lpdf,env,p->x,arguments);
  
  /* perform rejection test */
  if(!(metrop->on) || ((metrop->on) && (y >= ynew))){
    /* update envelope */
    p->y = ynew;
    p->ey = expshift(p->y,env->ymax);
    p->f = 1;
    if(update(env,p,lpdf,metrop,arguments)){
      /* envelope violation without metropolis */
      return -1;
    }
    /* perform rejection test */
    if(y >= ynew){
      /* reject point at rejection step */
      return 0;
    } else {
      /* accept point at rejection step */
      return 1;
    }
  }

  /* continue with metropolis step */
  yold = metrop->yprev;
  /* find envelope piece containing metrop->xprev */
  ql = env->p;
  while(ql->pl != NULL)ql = ql->pl;
  while(ql->pr->x < metrop->xprev)ql = ql->pr;
  qr = ql->pr;
  /* calculate height of envelope at metrop->xprev */
  w = (metrop->xprev - ql->x)/(qr->x - ql->x);
  zold = ql->y + w*(qr->y - ql->y);
  znew = p->y;
  if(yold < zold)zold = yold;
  if(ynew < znew)znew = ynew;
  w = ynew-znew-yold+zold;
  if(w > 0.0)w = 0.0;

  if(w > -YCEIL){
    w = exp(w);
  } else {
    w = 0.0;
  }
  u = R::unif_rand();
  if(u > w){
    /* metropolis says dont move, so replace current point with previous */
    /* markov chain iterate */
    p->x = metrop->xprev;
    p->y = metrop->yprev;
    p->ey = expshift(p->y,env->ymax);
    p->f = 1;
    p->pl = ql;
    p->pr = qr;
  } else {
    /* trial point accepted by metropolis, so update previous markov */
    /* chain iterate */
    metrop->xprev = p->x;
    metrop->yprev = ynew;
  }
  return 1;
}

/* *********************************************************************** */

int update(ENVELOPE *env, POINT *p, Function lpdf, METROPOLIS *metrop, SEXP arguments)

/* to update envelope to incorporate new point on log density*/
/* *env          : envelope attributes */
/* *p            : point to be incorporated */
/* *lpdf         : to evaluate log-density */
/* *metrop       : for metropolis step */

{
  POINT *m,*ql,*qr,*q;

  if(!(p->f) || (env->cpoint > env->npoint - 2)){
    /* y-value has not been evaluated or no room for further points */
    /* ignore this point */
    return 0;
  }

  /* copy working POINT p to a new POINT q */
  q = env->p + env->cpoint++;
  q->x = p->x;
  q->y = p->y;
  q->f = 1;

  /* allocate an unused POINT for a new intersection */
  m = env->p + env->cpoint++;
  m->f = 0;
  if((p->pl->f) && !(p->pr->f)){
    /* left end of piece is on log density; right end is not */
    /* set up new intersection in interval between p->pl and p */
    m->pl = p->pl;
    m->pr = q;
    q->pl = m;
    q->pr = p->pr;
    m->pl->pr = m;
    q->pr->pl = q;
  } else if (!(p->pl->f) && (p->pr->f)){
    /* left end of interval is not on log density; right end is */
    /* set up new intersection in interval between p and p->pr */
    m->pr = p->pr;
    m->pl = q;
    q->pr = m;
    q->pl = p->pl;
    m->pr->pl = m;
    q->pl->pr = q;
  } else {
    /* this should be impossible */
    stop("arms error 10");
  }

  /* now adjust position of q within interval if too close to an endpoint */
  if(q->pl->pl != NULL){
    ql = q->pl->pl;
  } else {
    ql = q->pl;
  }
  if(q->pr->pr != NULL){
    qr = q->pr->pr;
  } else {
    qr = q->pr;
  }
  if (q->x < (1. - XEPS) * ql->x + XEPS * qr->x){
    /* q too close to left end of interval */
    q->x = (1. - XEPS) * ql->x + XEPS * qr->x;
    q->y = perfunc(lpdf,env,q->x,arguments);
  } else if (q->x > XEPS * ql->x + (1. - XEPS) * qr->x){
    /* q too close to right end of interval */
    q->x = XEPS * ql->x + (1. - XEPS) * qr->x;
    q->y = perfunc(lpdf,env,q->x,arguments);
  }

  /* revise intersection points */
  if(meet(q->pl,env,metrop)){
    /* envelope violation without metropolis */
    return 1;
  }
  if(meet(q->pr,env,metrop)){
    /* envelope violation without metropolis */
    return 1;
  }
  if(q->pl->pl != NULL){
    if(meet(q->pl->pl->pl,env,metrop)){
      /* envelope violation without metropolis */
      return 1;
    }
  }
  if(q->pr->pr != NULL){
    if(meet(q->pr->pr->pr,env,metrop)){
      /* envelope violation without metropolis */
      return 1;
    }
  }

  /* exponentiate and integrate new envelope */
  cumulate(env);

  return 0;
}

/* *********************************************************************** */

void cumulate(ENVELOPE *env)

/* to exponentiate and integrate envelope */
/* *env     : envelope attributes */

{
  POINT *q,*qlmost;

  qlmost = env->p;
  /* find left end of envelope */
  while(qlmost->pl != NULL)qlmost = qlmost->pl;

  /* find maximum y-value: search envelope */
  env->ymax = qlmost->y;
  for(q = qlmost->pr; q != NULL; q = q->pr){
    if(q->y > env->ymax)env->ymax = q->y;
  }

  /* exponentiate envelope */
  for(q = qlmost; q != NULL; q = q->pr){
    q->ey = expshift(q->y,env->ymax);
  }

  /* integrate exponentiated envelope */
  qlmost->cum = 0.;
  for(q = qlmost->pr; q != NULL; q = q->pr){
    q->cum = q->pl->cum + area(q);
  }

  return;
}

/* *********************************************************************** */

int meet (POINT *q, ENVELOPE *env, METROPOLIS *metrop)
/* To find where two chords intersect */
/* q         : to store point of intersection */
/* *env      : envelope attributes */
/* *metrop   : for metropolis step */

{
  double gl,gr,grl,dl,dr;
  int il,ir,irl;

  // NOTE(mgnb): to prevent gcc complaining
  gl = 0;
  gr = 0;
  dl = 0;
  dr = 0;

  if(q->f){
    /* this is not an intersection point */
    stop("arms error 30");
  }

  /* calculate coordinates of point of intersection */
  if ((q->pl != NULL) && (q->pl->pl->pl != NULL)){
    /* chord gradient can be calculated at left end of interval */
    gl = (q->pl->y - q->pl->pl->pl->y)/(q->pl->x - q->pl->pl->pl->x);
    il = 1;
  } else {
    /* no chord gradient on left */
    il = 0;
  }
  if ((q->pr != NULL) && (q->pr->pr->pr != NULL)){
    /* chord gradient can be calculated at right end of interval */
    gr = (q->pr->y - q->pr->pr->pr->y)/(q->pr->x - q->pr->pr->pr->x);
    ir = 1;
  } else {
    /* no chord gradient on right */
    ir = 0;
  }
  if ((q->pl != NULL) && (q->pr != NULL)){
    /* chord gradient can be calculated across interval */
    grl = (q->pr->y - q->pl->y)/(q->pr->x - q->pl->x);
    irl = 1;
  } else {
    irl = 0;
  }

  if(irl && il && (gl<grl)){
    /* convexity on left exceeds current threshold */
    if(!(metrop->on)){
      /* envelope violation without metropolis */
      return 1;
    }
    /* adjust left gradient */
    gl = gl + (1.0 + env->convex) * (grl - gl);
  }

  if(irl && ir && (gr>grl)){
    /* convexity on right exceeds current threshold */
    if(!(metrop->on)){
      /* envelope violation without metropolis */
      return 1;
    }
    /* adjust right gradient */
    gr = gr + (1.0 + env->convex) * (grl - gr);
  }

  if(il && irl){
    dr = (gl - grl) * (q->pr->x - q->pl->x);
    if(dr < YEPS){
      /* adjust dr to avoid numerical problems */
      dr = YEPS;
    }
  }

  if(ir && irl){
    dl = (grl - gr) * (q->pr->x - q->pl->x);
    if(dl < YEPS){
      /* adjust dl to avoid numerical problems */
      dl = YEPS;
    }
  }

  if(il && ir && irl){
    /* gradients on both sides */
    q->x = (dl * q->pr->x + dr * q->pl->x)/(dl + dr);
    q->y = (dl * q->pr->y + dr * q->pl->y + dl * dr)/(dl + dr);
  } else if (il && irl){
    /* gradient only on left side, but not right hand bound */
    q->x = q->pr->x;
    q->y = q->pr->y + dr;
  } else if (ir && irl){
    /* gradient only on right side, but not left hand bound */
    q->x = q->pl->x;
    q->y = q->pl->y + dl;
  } else if (il){
    /* right hand bound */
    q->y = q->pl->y + gl * (q->x - q->pl->x);
  } else if (ir){
    /* left hand bound */
    q->y = q->pr->y - gr * (q->pr->x - q->x);
  } else {
    /* gradient on neither side - should be impossible */
    stop("arms error 31");
  }
  if(((q->pl != NULL) && (q->x < q->pl->x)) ||
     ((q->pr != NULL) && (q->x > q->pr->x))){
    /* intersection point outside interval (through imprecision) */
    stop("arms error 32");
  }
  /* successful exit : intersection has been calculated */
  return 0;
}

/* *********************************************************************** */

double area(POINT *q)

/* To integrate piece of exponentiated envelope to left of POINT q */

{
  double a;

  if(q->pl == NULL){
    /* this is leftmost point in envelope */
    stop("arms error 1");
  } else if(q->pl->x == q->x){
    /* interval is zero length */
    a = 0.;
  } else if (fabs(q->y - q->pl->y) < YEPS){
    /* integrate straight line piece */
    a = 0.5*(q->ey + q->pl->ey)*(q->x - q->pl->x);
  } else {
    /* integrate exponential piece */
    a = ((q->ey - q->pl->ey)/(q->y - q->pl->y))*(q->x - q->pl->x);
  }
  return a;
}

/* *********************************************************************** */

double expshift(double y, double y0)

/* to exponentiate shifted y without underflow */
{
  if(y - y0 > -2.0 * YCEIL){
    return exp(y - y0 + YCEIL);
  } else {
    return 0.0;
  }
}

/* *********************************************************************** */

double logshift(double y, double y0)

/* inverse of function expshift */
{
  return (log(y) + y0 - YCEIL);
}

/* *********************************************************************** */

double perfunc(Function lpdf, ENVELOPE *env, double x, SEXP arguments)

/* to evaluate log density and increment count of evaluations */

/* *lpdf   : structure containing pointers to log-density function and data */
/* *env    : envelope attributes */
/* x       : point at which to evaluate log density */

{
  double y;

  /* evaluate density function */
  NumericVector result;
  if (arguments == R_NilValue) {
    result = lpdf(x);
  } else {
    result = Rcpp_eval(Rcpp_lcons(
      lpdf,
      grow(x, arguments)
    ));
  }
  y = result[0];

  /* increment count of function evaluations */
  (*(env->neval))++;

  return y;
}

/* *********************************************************************** */
