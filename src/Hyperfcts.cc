/*
 Authors 
 Martin Schlather, schlath@hsu-hh.de

 Definition of correlation functions and derivatives of hypermodels

 Copyright (C) 2005 -- 2006 Martin Schlather

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.  
*/


#include <math.h>
#include <assert.h>
#include "RFsimu.h"
#include "RFCovFcts.h"

static double local_range[8] = {1, MAXCOV-1, 1, MAXCOV-1, // HYPERNR
		       OPEN, RF_INF, 1e-10, 1e10};// CUTOFF_DIAM

int check_submodels(int nr, char **allowed_fct, int nr_allowed_list, 
		    covinfo_arraytype keycov, 
		    covlist_type covlist, int left)
{
 int covnr[MAXCOV], v, i;
  covinfo_type *cn;
  char s[2][MAXERRORSTRING];

  if (nr < 1) { // always necessary to check
    strcpy(ERRORSTRING_OK, " at least 1 submodel");
    sprintf(ERRORSTRING_WRONG,"%d", nr);
    return ERRORCOVFAILED;
  }
  if (nr>1) { // should and will be relaxed in future
    strcpy(ERRORSTRING_OK, " at most 1 submodel currently. (This will be relaxed in future)");
    sprintf(ERRORSTRING_WRONG,"%d", nr);
    return ERRORCOVFAILED;
  }
  if (nr>left) { // always necessary to check
    strcpy(ERRORSTRING_OK, " further submodels");
//    strcpy(ERRORSTRING_OK,"number of models included must be at least 1 and less than or equal to the number of remaining models in the list (here %d)", left);
    sprintf(ERRORSTRING_WRONG,"announced=%d available=%d", nr, left);
    return ERRORCOVFAILED;
  }
  GetModelNr(allowed_fct, &nr_allowed_list, covnr);
  for (v=0; v<nr; v++) {
    cn = &(keycov[covlist[v]]);
    for (i=0; i<nr_allowed_list; i++) if (cn->nr==covnr[i]) break;
//    printf("check %d %d  %d %s %d\n", v, cn->nr, i, allowed_fct[i], covnr[i]);
    if (i==nr_allowed_list) {
      strcpy(s[0], "the models %s%s");
      for (i=0; i<nr_allowed_list; i++) {
        sprintf(s[(i+1) % 2 ], s[i % 2], CovList[covnr[i]].name, ",%s%s");
      }
      strcpy(ERRORSTRING_OK, s[i % 2]);
      sprintf(ERRORSTRING_WRONG,"%s", CovList[nr].name);
      return ERRORCOVFAILED;
    }
  }
  return NOERROR;
}



double co(double *x, double *p)
{
  double y=fabs(x[0]);
  if (y <= p[DIAMETER]) return x[1]; // value of submodel!
  if (y >= p[LOCAL_R]) return 0.0;
  return p[CUTOFF_B] * pow(p[CUTOFF_ASQRTR] - pow(y, p[CUTOFF_A]), 
			   2.0 * p[CUTOFF_A]);
}

int getcutoffparam_co(covinfo_type *kc, param_type q, int instance)
{
  double thres[2]; 
  assert(instance >= 0);

  // printf("get: %s %d %f\n", CovList[kc->nr].name, instance, kc->param[KAPPA]);
  
  if (kc->nr == EXPONENTIAL) {
	q[CUTOFF_A] = 1.0; 
	if (instance == 0) return MSGLOCAL_OK;
  } else if (kc->nr == CAUCHY) {
    q[CUTOFF_A] = 1.0;
    if (instance==0) return MSGLOCAL_JUSTTRY;
  } else {
    if (kc->nr == GENERALISEDCAUCHY || kc->nr==STABLE) {
      thres[0] = 0.5; thres[1] = 1.0;
    } else if (kc->nr == WHITTLEMATERN) {
      thres[0] = 0.25; thres[1] = 0.5; 
    } else return MSGLOCAL_ENDOFLIST;
    
    if (kc->param[KAPPA] <= thres[0]) {
#define nA 2
      static double A[nA] = {0.5, 1.0};
      if (instance < nA) {
	q[CUTOFF_A] = A[instance];
	return MSGLOCAL_OK;
      }
    } else if (kc->param[KAPPA] <= thres[1]) {
      q[CUTOFF_A] = 1.0; 
      if (instance == 0)  return MSGLOCAL_OK;
    } else {
      q[CUTOFF_A] = 1.0;
      if (instance==0) return MSGLOCAL_JUSTTRY;
    }
  }
  return MSGLOCAL_ENDOFLIST;
}

bool alternativeparam_co(covinfo_type *kc, int instance){
  return false;
}

void range_co(int reduceddim, int *index, double* range) {
  double cutoff_a[4] = {OPEN, RF_INF, 0.5, 2.0}; 
  *index = RANGE_LASTELEMENT; 
  memcpy(range, local_range, sizeof(double) * 8);
  memcpy(&(range[8]),cutoff_a, sizeof(double) * 4);
};

void info_co(double *p, int *maxdim, int *CEbadlybehaved) {
  *maxdim = INFDIM;
  *CEbadlybehaved=false;
}

int checkNinit_co(covinfo_arraytype keycov, covlist_type covlist, 
		  int remaining_covlistlength, SimulationType method) {
#define co_nr 5
  char *allowed_fct[co_nr] = {"stable", "cauchy",
			      "whittle", "gencauchy", "exponential"};
  int err, v, nsub; // timespacedim -- needed ?;
  double *param, phi0, phi1, store[MAXCOV], a, a2, d, one=1.0;
  covinfo_type *kc;

  if (method != CircEmbed && method!=Nothing && method != Direct) 
    return ERRORUNKNOWNMETHOD;
//  if (method != Nothing) return NOERROR; // already initialised by
  // CheckAndBuild; no specific things to do

  kc = &(keycov[covlist[0]]);
  param = kc->param;  
  nsub= (int) (kc->param[HYPERNR]);
  d = param[DIAMETER]; // SCALE is considered as space trafo and envolved here
  a = param[CUTOFF_A];
  if ((err=check_submodels(nsub, allowed_fct, co_nr, keycov, 
			   &(covlist[1]), remaining_covlistlength)) != NOERROR)
      return err;
  
  a2 = a * a;
  for (v=1; v<=nsub; v++) { 
      kc=&(keycov[covlist[v]]);
      store[v] = kc->aniso[0];
      kc->aniso[0] = d;
  }
  phi0 = CovFct(&one, 1, keycov, &(covlist[1]), nsub, false);
  phi1 = DerivCovFct(&one, 1, keycov, &(covlist[1]), nsub);
//  printf("check_co %f %f %f %f\n", phi0, phi1, a, a2);
  if (phi0 <= 0.0) return MSGLOCAL_SIGNPHI;
  if (phi1 >= 0.0) return MSGLOCAL_SIGNPHIFST;
  param[CUTOFF_B] = //sound constant even if variance of submodel is not 1
      pow(- phi1 / (2.0 * a2 * phi0), 2.0 * a) * phi0 / pow(d, 2.0 * a2);
  param[CUTOFF_THEOR] = pow(1.0 - 2.0 * a2 * phi0 / phi1, 1.0 / a);
//  printf("phi0=%f %f\n", phi0, phi1);  
  param[LOCAL_R] = d * param[CUTOFF_THEOR];
  param[CUTOFF_ASQRTR] = pow(param[LOCAL_R], a);
  for (v=1; v<=nsub; v++) 
      keycov[covlist[v]].aniso[0] = store[v];
  return NOERROR;
}


double Stein(double *x, double *p)
{
  double y=fabs(x[0]), z;
  if (y <= p[DIAMETER]) 
    return p[INTRINSIC_A0] + p[INTRINSIC_A2] * y * y + x[1]; 
                                                    // value of submodel!
  if (y >= p[LOCAL_R]) return 0.0;
//  printf("%f \n", p[LOCAL_R]);
  z = p[LOCAL_R] - y;
  return p[INTRINSIC_B] * z * z * z / y;
}

int getintrinsicparam_Stein(covinfo_type *kc, param_type q, int instance)
{
  assert(instance >= 0);
  if (instance>0) return MSGLOCAL_ENDOFLIST;
  if ((kc->nr == GENERALISEDCAUCHY && kc->param[KAPPA] <= 1.0) ||
      (kc->nr == STABLE && kc->param[KAPPA] <= 1.0) || 
      (kc->nr == EXPONENTIAL) || 
      (kc->nr == WHITTLEMATERN && kc->param[KAPPA] <= 0.5)) {
    q[INTRINSIC_RAWR] = 1.0;
    return MSGLOCAL_OK;
  } else if (kc->nr == BROWNIAN) {
      q[INTRINSIC_RAWR] = (kc->reduceddim <= 2 
			    ? ((kc->param[KAPPA] <= 1.5) ? 1.0 : 2.0)
			    : ((kc->param[KAPPA] <= 1.0) ? 1.0 : 2.0));
       // for genuine variogram models only
    return MSGLOCAL_OK;
  }
  
  // also for CAUCHY ...
  q[INTRINSIC_RAWR] = 1.5; // must be replaced by numerical results !
  return MSGLOCAL_JUSTTRY;
//  return MSGLOCAL_ENDOFLIST;
}

bool alternativeparam_Stein(covinfo_type *kc, int instance)
{
  if (instance > 0) return false;
  kc->param[INTRINSIC_RAWR] *= 2.0;
  return true;
}

void range_Stein(int reduceddim, int *index, double* range) 
{
  double stein_r[4] = {1, RF_INF, 1, 20.0}; 
  *index = RANGE_LASTELEMENT; 
  memcpy(range, local_range, sizeof(double) * 8);
  memcpy(&(range[8]), stein_r, sizeof(double) * 4);
};


void info_Stein(double *p, int *maxdim, int *CEbadlybehaved) 
{
  *maxdim = INFDIM;
  *CEbadlybehaved=false;
}

int checkNinit_Stein(covinfo_arraytype keycov, covlist_type covlist, 
		 int remaining_covlistlength, SimulationType method) 
{
#define stein_nr 6
  char *allowed_fct[stein_nr] = 
      {"stable", "whittle", "cauchy", "gencauchy", "fractalB", "exponential"};
  int v, nsub, err;
  double *param, C0, phi0, phi1, phi2, store[MAXCOV], d, zero = 0.0, r, 
    one = 1.0;
  covinfo_type *kc;
 
  if (method != CircEmbed && method!=Nothing && method != Direct)
      return ERRORUNKNOWNMETHOD;
  // if (method != Nothing) return NOERROR; // already initialised by
  // CheckAndBuild; no specific things to do

  kc = &(keycov[covlist[0]]);
  param = kc->param;  
  nsub= (int) (kc->param[HYPERNR]);
  d = param[DIAMETER];
  r = param[INTRINSIC_RAWR];
  if ((err=check_submodels(nsub, allowed_fct, stein_nr, keycov, 
			    &(covlist[1]), remaining_covlistlength)) != NOERROR)
      return err;

  for (v=1; v<=nsub; v++) { 
    kc = &(keycov[covlist[v]]);
    store[v] = kc->aniso[0];
    kc->aniso[0] = d;
    if (v<nsub && CovList[kc->nr].variogram &&  kc->op)
      return ERRORNOMULTIPLICATION;
  }
  
  C0 = CovFct(&zero, 1, keycov, &(covlist[1]), nsub, false);
  phi0 = CovFct(&one, 1, keycov, &(covlist[1]), nsub, false);
  phi1 = DerivCovFct(&one, 1, keycov, &(covlist[1]), nsub);
  phi2 = SndDerivCovFct(&one, 1, keycov, &(covlist[1]), nsub);
 
  param[LOCAL_R] =  r * d;   
  param[INTRINSIC_A2] = (phi2 - phi1) / (3.0 * r * (r + 1.0)) ;
  param[INTRINSIC_B]  = 
    (r == 1.0) ? 0.0 : param[INTRINSIC_A2] / ((r - 1.0) * d * d);
  param[INTRINSIC_A2] = 
    (param[INTRINSIC_A2] - phi1 / 3.0 - phi2 / 6.0) / (d * d);
  param[INTRINSIC_A0] = 
    0.5 * (r - 1.0) / (r + 1.0) * phi2 + phi1 / (r + 1.0) - phi0;
 
//   printf("check: %f %e r=%f intr:%f %f\n", 
//	  phi2, phi1, r,
//	  param[INTRINSIC_A0],  param[INTRINSIC_A2]);


  if ((param[INTRINSIC_B]  < 0.0) || (param[INTRINSIC_A2] < 0.0) ||
      (param[INTRINSIC_A0] + C0 < 0.0)) 
    return MSGLOCAL_INITINTRINSIC;
  
  for (v=1; v<=nsub; v++) 
    keycov[covlist[v]].aniso[0] = store[v];
  return NOERROR;
}


double MaStein(double *x, double *p)
{
  double s, nuG, gammas;
  int d, effectivedim;
  effectivedim = (int) p[EFFECTIVEDIM];
  // effectivedim + 1 : C(0) oder 0
  // effectvedim : C(x) oder -gamma(x)
  nuG = p[HYPERKAPPAI] + (x[effectivedim + 1] - x[effectivedim]);
  gammas = lgammafn(p[HYPERKAPPAI] + p[HYPERKAPPAII]) -
    lgammafn(p[HYPERKAPPAI]) -lgammafn(nuG + p[HYPERKAPPAII]);
  for (s = 0.0, d=0; d<effectivedim; d++) s += x[d] * x[d];
  if (s==0.0) return exp(lgammafn(nuG) + gammas);
  s = sqrt(s);
  return 2.0 * exp(nuG * log(0.5 * s) + gammas + log(bessel_k(s, nuG, 2.0)) - s);
}


void range_MaStein(int reduceddim, int *index, double* range) {
    static double range_MaStein[8]={
	1, MAXCOV-1, 1, MAXCOV-1, 
	OPEN, RF_INF, 1e-2, 10.0}; 
    memcpy(range, range_MaStein, sizeof(double) * 8);
    range[8] = range[10] = 0.5 * (double) (reduceddim - 1);
    range[9] = RF_INF; range[11] = 10;
    *index = -1; 
};

void info_MaStein(double *p, int *maxdim, int *CEbadlybehaved) {
  *maxdim = INFDIM;
  *CEbadlybehaved=false;
}

int checkNinit_MaStein(covinfo_arraytype keycov, covlist_type covlist, 
		  int remaining_covlistlength, SimulationType method) {
  int nsub, v, i, endfor;
  covinfo_type *kc;
  double *param;

  kc = &(keycov[covlist[0]]);
  param = kc->param;  

  if (kc->genuine_last_dimension) 
    return ERRORTIMECOMPONENT;
 
  nsub= (int) (kc->param[HYPERNR]);  
  endfor = ANISO +  kc->dim * kc->dim - 1;
  for (v=1; v<=nsub; v++) {
    kc=&(keycov[covlist[v]]);
    for (i=ANISO; i<endfor; i++) {
      if (kc->param[i]!=0.0) {
	  strcpy(ERRORSTRING_OK,
		 "anisotropy matrices in submodels with zeros every where except the very last component");
          sprintf(ERRORSTRING_WRONG,
		  "%f in element %d of anisotropy matrix of submodel %d",
		  kc->param[i],  1 - (ANISO - i), v); // i - ANISO + 1 gibt
	  // falsches Resultat !!!!!!!
	  return ERRORCOVFAILED;
      }
    }
  }
  return NOERROR;
}
