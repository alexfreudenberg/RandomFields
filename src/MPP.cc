

/*
 Authors 
 Martin Schlather, schlath@hsu-hh.de 

 Copyright (C) 2001 -- 2005 Martin Schlather, 


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

/* TO DO
  * noch fast alles auf key->x programmiert statt s->x
  * time component: aenderungen siehe TBM !
*/

#include <math.h>
#include <assert.h>
#include "RFsimu.h"
#include "MPPFcts.h"

double MPP_RADIUS =0.0;
double MPP_APPROXZERO  =0.001;
double ADDMPP_REALISATIONS = 100.0; // logically an integer, implementation 
                                  //  allows for positive real ! 

void SetMPP(int *action, double *approxzero, double *realisations, 
	    double *radius)
{
  if (*action) {
    if (*approxzero<=0.0) {
      if (GENERAL_PRINTLEVEL>0) 
	PRINTF("\nWARNING! `approx.zero' not a positive number. Ignored.\n");
    } else  MPP_APPROXZERO = *approxzero;
   if (*realisations<=0.0) {
     if (GENERAL_PRINTLEVEL>0) 
       PRINTF("\nWARNING! `realisation' not a positive number. Ignored.\n");
   } else ADDMPP_REALISATIONS = *realisations;
   MPP_RADIUS = *radius;
   if ((MPP_RADIUS>0.0) && (GENERAL_PRINTLEVEL>1))
     PRINTF("\nWARNING! Positive `addradius' may result in simulation results that are difficult to detect.\n");
  } else {
    *approxzero = MPP_APPROXZERO;
    *realisations =  ADDMPP_REALISATIONS;
    *radius = MPP_RADIUS;
  }
}

void mpp_destruct(void **S) {
  // mpp_storage in RFsimu.h !!
  if (*S!=NULL) {
    free(*S);
    *S = NULL;
  }
}

int init_mpp(key_type * key, int m)
{
  methodvalue_type *meth; 
  int error, d, i, v, timespacedim, actcov;
  double max[MAXDIM];  
  mpp_storage *s;
  covinfo_type *keycov;

  error = NOERROR;
  meth = &(key->meth[m]);
  SET_DESTRUCT(mpp_destruct, m);
  assert(meth->S==NULL);
  if ((meth->S=malloc(sizeof(mpp_storage)))==0){
    error=ERRORMEMORYALLOCATION; goto ErrorHandling;
  }
  s = (mpp_storage*) meth->S;
  s->dim = 0;  // otherwise error possible in InternalGetKeyInfo

  for (actcov=v=0; v<key->ncov; v++) {
    keycov = &(key->cov[v]);
    if ((keycov->method==AdditiveMpp) && keycov->left
        && (keycov->param[VARIANCE]>0)) {
      cov_fct *cov;
      assert(keycov->nr>=0 && keycov->nr<currentNrCov);
      meth->covlist[actcov] = v;
      cov = &(CovList[keycov->nr]);
      if (key->Time && !keycov->genuine_last_dimension && 
	  (cov->type==SPACEISOTROPIC)) {// unclear whether this is too cautious
	  error= ERRORWITHOUTTIME; goto ErrorHandling;}
      if ((error=cov->check(keycov->param, keycov->truetimespacedim, 
			    AdditiveMpp)) != NOERROR) goto ErrorHandling;
      if (cov->add_mpp_scl==NULL) {error=ERRORNOTDEFINED; goto ErrorHandling;}
      if (cov->type==ISOHYPERMODEL || cov->type==ANISOHYPERMODEL) {
	error=ERRORHYPERNOTALLOWED; goto ErrorHandling;}
      if ((v<key->ncov-1 &&  keycov->op)) {
	// no multiplicative models are allowed
	  error=ERRORNOMULTIPLICATION; goto ErrorHandling;}
      if ((error=Transform2NoGrid(key, v)) != NOERROR) goto ErrorHandling;
      keycov->left=false;
      actcov++; 
      // do not pile up more than 1 elementary covariance function.
      // creating max-stable fields may cause difficulties! 
      break;
    }
  }
  if (actcov==0) {
    if (error==NOERROR) error=NOERROR_ENDOFLIST;
    goto ErrorHandling;
  } else assert(actcov==1);

  meth->actcov = actcov;
  keycov = &(key->cov[0]);
  // determine the minimum area where random field values are to be generated
  if (keycov->simugrid) {
    timespacedim = key->timespacedim;
    for (d=0; d<timespacedim; d++) {
      s->min[d]   = keycov->x[XSTARTD[d]];
      s->length[d]= keycov->x[XSTEPD[d]] * (double) (key->length[d] - 1);
      // x[XSTARTD[d]] and x[XSTEPD[d]] are assumed to be precise, but
      // not x[XENDD[d]] 
    }
  } else {
    int ix;
    timespacedim = keycov->truetimespacedim;
      for (d=0; d<timespacedim; d++) {
      s->min[d]=RF_INF; 
      max[d]=RF_NEGINF;
    }
    v = 0;
    keycov = &(key->cov[meth->covlist[v]]);
    for (ix=i=0; i<key->totalpoints; i++, ix+=timespacedim) {
      for (d=0; d<timespacedim; d++) {
	if (keycov->x[ix+d] < s->min[d]) s->min[d] = keycov->x[ix+d];
	if (keycov->x[ix+d] > max[d]) max[d] = keycov->x[ix+d];
      }
    }
    for (d=0; d<timespacedim; d++) 
      s->length[d] = max[d] - s->min[d];
  }
  v = 0;
  keycov = &(key->cov[meth->covlist[v]]);
  s->addradius = MPP_RADIUS;  // must be set BEFORE the next command!!
  CovList[keycov->nr].add_mpp_scl(s, timespacedim, keycov->param);
  s->MppFct = CovList[keycov->nr].add_mpp_rnd;
  if ((MPP_RADIUS>0.0) && (GENERAL_PRINTLEVEL>=2))
    PRINTF("Note: window has been enlarged by fixed value (%f)\n",s->addradius);
 
  if (key->anisotropy) return NOERROR_REPEAT;
  else return 0;

 ErrorHandling:
  return error; 
}


void do_addmpp(key_type *key, int m, double *res )
{ 
  methodvalue_type *meth; 
  covinfo_type *keycov;
  int i, d, v, timespacedim;
  double lambda, //[MAXDIM], 
    min[MAXDIM], max[MAXDIM], 
    factor, average, poisson;
  long segment[MAXDIM+1];
  mpp_storage *s;
  mppmodel model;

  assert(key->active);
  meth = &(key->meth[m]);
  assert(meth->actcov==1);
  {
    register long i;
    for (i=key->totalpoints - 1; i>=0; res[i--]=0.0);
  }

  s =  (mpp_storage*) meth->S;  
  v = 0;  // NOTE !
  
  switch (key->distribution) {
  case DISTR_GAUSS : 
    keycov = &(key->cov[meth->covlist[v]]);
    lambda = ADDMPP_REALISATIONS * s->effectivearea;
    factor = sqrt(keycov->param[VARIANCE] / 
		  (ADDMPP_REALISATIONS * s->integralsq));
    average = ADDMPP_REALISATIONS * s->integral;
    break;
  case DISTR_POISSON :
    // not done yet -- it is only a fragment
    // on the R level, it is excluded that the program runs 
    // into this part
    //
    // only actcov==1 is possible!!
    //
    // maybe nice to switch MPPFcts back to actcov=1
    keycov = &(key->cov[meth->covlist[v]]);
    lambda = keycov->param[VARIANCE] * s->effectivearea; 
    factor = average = 0.0 /* replace by true value !! */;  
    assert(false);
    if (key->mean!=0) {
      // do not compare mean against VARIANCE since
      // VARIANCEs may differ with v
      //      if (GENERAL_PRINTLEVEL>0)
      ErrorMessage(AdditiveMpp,ERRORVARMEAN); assert(false); // print it always
      
      assert(false); // for debugging -- delete this later on
      
      // finally, the value of MEAN is ignored, and that of VARIANCE
      // is used for simulating the random field
    }
    break;
  default : assert(false);
  }
  
  segment[0] = 1;  // only used if simugrid
  for (d=0; d<key->timespacedim; d++)  
      segment[d+1] = segment[d] * key->length[d];
  for (i=0; i<key->totalpoints; i++) res[i]=0.0;
  keycov = &(key->cov[meth->covlist[v]]);
  timespacedim = keycov->simugrid ? key->timespacedim : keycov->truetimespacedim;
  for (poisson = rexp(1.0); poisson < lambda; poisson += rexp(1.0)) {   
    (s->MppFct)(s, min, max, &model);
    if (keycov->simugrid) {
      int start[MAXDIM], end[MAXDIM], resindex, index[MAXDIM],
	  segmentdelta[MAXDIM], dimM1;
      double coord[MAXDIM], startcoord[MAXDIM];
	  
      resindex = 0;
      for (d=0; d<timespacedim; d++) {	 
        // determine rectangle of indices, where the mpp function
        // is different from zero
        if (min[d]< keycov->x[XSTARTD[d]]) {start[d]=0;}
        else start[d] = (int) ((min[d] - keycov->x[XSTARTD[d]]) / 
      			 keycov->x[XSTEPD[d]]);
        // "end[d] = 1 + *"  since inequalities are "* < end[d]" 
        // not "* <= end[d]" !
	end[d] = 1 + (int) ((max[d] - keycov->x[XSTARTD[d]]) /
			    keycov->x[XSTEPD[d]]);
	if (end[d] > key->length[d]) end[d] = key->length[d];	 

	// prepare coord starting value and the segment vectors for res
	index[d] = start[d];
	segmentdelta[d] = segment[d] * (end[d]-start[d]);
	resindex += segment[d] * start[d];
	coord[d] = startcoord[d] = 
	    keycov->x[XSTARTD[d]] + (double) start[d] * keycov->x[XSTEPD[d]];
      }
	 
      // add mpp function to res
      dimM1 = timespacedim -1;
      d = 0; // only needed for assert(d<dim)
      while (index[dimM1] < end[dimM1]) {
	assert(d<timespacedim); 
	res[resindex] += model(coord); 
	d=0;
	index[d]++;
	coord[d] += keycov->x[XSTEPD[d]];
	resindex++;
	while (index[d] >= end[d]) { 
	  if (d>=dimM1) break;  // below not possible
	  //                       as dim==1 will fail!
	  index[d] = start[d];
	  coord[d] = startcoord[d];
	  resindex -= segmentdelta[d];
	  d++; // if (d>=dim) break;
	  index[d]++;
	  coord[d] += keycov->x[XSTEPD[d]];
	  resindex += segment[d];
	}
      }
    } else { // !key->simugrid
      double y[MAXDIM];
      long j;
      // the following algorithm can greatly be improved !
      // but for ease, just the stupid algorithm
      for (j=i=0; i<key->totalpoints; i++) {
	for (d=0; d<timespacedim; d++, j++) y[d] = keycov->x[j];
	res[i] += model(y); 
      }
    }      
  }

  if (key->distribution==DISTR_GAUSS) {
    for (i=0; i<key->totalpoints;i++) {
      res[i] = factor * (res[i] - average);
    }
  }
}

  










