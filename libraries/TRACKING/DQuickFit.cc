// $Id$
// Author: David Lawrence  June 25, 2004
//
//
//

#include <iostream>
using namespace std;


#include "DQuickFit.h"

//-----------------
// DQuickFit
//-----------------
DQuickFit::DQuickFit()
{
	
	x0 = y0 = 0;
	chisq = 0;
	chisq_source = NOFIT;

	hits = new DContainer(NULL, sizeof(TVector3*), "hits");
	chisqv = new DContainer(NULL, sizeof(int), "chisqv");
}

//-----------------
// ~DQuickFit
//-----------------
DQuickFit::~DQuickFit()
{

	TVector3 **h = (TVector3**)hits->first();
	for(int i=0;i<hits->nrows;i++, h++){
		delete (*h);
	}
	delete hits;
	delete chisqv;
}

//-----------------
// AddHit
//-----------------
derror_t DQuickFit::AddHit(float r, float phi, float z)
{
	
	*(TVector3**)hits->Add() = new TVector3(r*cos(phi), r*sin(phi), z);

	return NOERROR;
}

//-----------------
// AddHits
//-----------------
derror_t DQuickFit::AddHits(int N, TVector3 *v)
{
	TVector3 *my_v = v;
	for(int i=0; i<N; i++, my_v++){
		*(TVector3**)hits->Add() = new TVector3(*my_v);
	}

	return NOERROR;
}

//-----------------
// PruneHits
//-----------------
derror_t DQuickFit::PruneHits(float chisq_limit)
{
	if(hits->nrows != chisqv->nrows){
		cerr<<__FILE__<<":"<<__LINE__<<" hits and chisqv do not have the same number of rows!"<<endl;
		cerr<<"Call FitCircle() or FitTrack() method first!"<<endl;

		return NOERROR;
	}
	
	// Loop over these backwards to make it easier to
	// handle the deletes
	TVector3 **v = (TVector3**)hits->last();
	int *c = (int*)chisqv->last();
	for(int i=hits->nrows-1; i>=0; i--, v--, c--){
		if(*c > chisq_limit){
			hits->Delete(i);
			chisqv->Delete(i);
		}
	}
	
	return NOERROR;
}

//-----------------
// CopyToFitParms
//-----------------
derror_t DQuickFit::CopyToFitParms(FitParms_t *fit)
{
	/// Copy the results of the most recent fit into the specified FitParms_t
	/// structure.

	fit->x0 = x0;
	fit->y0 = y0;
	fit->q = q;
	fit->p = p;
	fit->p_trans = p_trans;
	fit->phi = phi;
	fit->theta = theta;
	fit->chisq = chisq;
	int *c = (int*)chisqv->first();
	fit->nhits = 0;
	for(int i=0; i<chisqv->nrows; i++, c++){
		if(i>=MAX_CHISQV_HITS)break;
		
		fit->chisqv[fit->nhits++] = *c;
	}

	return NOERROR;
}

static int qsort_points_by_z(const void* arg1, const void* arg2);


//-----------------
// FitCircle
//-----------------
derror_t DQuickFit::FitCircle(void)
{
	/// Fit the current set of hits to a circle
	///
	/// This takes the hits which have been added thus far via one
	/// of the AddHit() methods and fits them to a circle.
	/// The alogorithm used here calculates the parameters directly using
	/// a technique very much like linear regression. The key assumptions
	/// are:
	/// 1. The magnetic field is uniform and along z so that the projection
	///    of the track onto the X/Y plane will fall on a circle
	///    (this also implies no multiple-scattering)
	/// 2. The vertex is at the target center (i.e. 0,0,0 in the coordinate
	///    system of the points passed to us.
	///
	/// IMPORTANT: The value of phi which results from this assumes
	/// the particle was positively charged. If the particle was
	/// negatively charged, then phi will be 180 degrees off. To
	/// correct this, one needs z-coordinate information to determine
	/// the sign of the charge.
	///
	/// ALSO IMPORTANT: This assumes a charge of +1 for the particle. If
	/// the particle actually has a charge of +2, then the resulting
	/// value of p_trans will be half of what it should be.

	float alpha=0.0, beta=0.0, gamma=0.0, deltax=0.0, deltay=0.0;
	
	// Loop over hits to calculate alpha, beta, gamma, and delta
	TVector3 **v= (TVector3**)hits->first();
	for(int i=0;i<hits->nrows;i++, v++){
		float x=(*v)->x();
		float y=(*v)->y();
		alpha += x*x;
		beta += y*y;
		gamma += x*y;
		deltax += x*(x*x+y*y)/2.0;
		deltay += y*(x*x+y*y)/2.0;
	}
	
	// Calculate x0,y0 - the center of the circle
	x0 = (deltax*beta-deltay*gamma)/(alpha*beta-gamma*gamma);
	y0 = (deltay-gamma*x0)/beta;

	// Momentum depends on magnetic field. Assume 2T for now.
	// Also assume a singly charged track (i.e. q=+/-1)
	// The sign of the charge will be determined below.
	float B=-2.0*0.593; // The 0.5931 is empirical
	q = +1.0;
	float r0 = sqrt(x0*x0 + y0*y0);
	float hbarc = 197.326;
	float p_trans = q*B*r0/hbarc; // are these the right units?
	
	phi = atan2(y0,x0) - M_PI_2;
	if(phi<0)phi+=2.0*M_PI;
	if(phi>=2.0*M_PI)phi-=2.0*M_PI;

	return NOERROR;
}

//-----------------
// FitTrack
//-----------------
derror_t DQuickFit::FitTrack(void)
{
#if 0
	// Sort by Z
	qsort(points, Npoints, sizeof(TVector3), qsort_points_by_z);

	// Assuming points are ordered in increasing z, the sign of the
	// cross-product between successive points will be the opposite
	// sign of the charge. Since it's possible to have a few "bad"
	// points, we don't want to rely on any one to determine this.
	// The method we use is to sum cross-products of the first and
	// middle points, the next-to-first and next-to-middle, etc.
	float xprod_sum = 0.0;
	int n_2 = Npoints/2; 
	v = points;
	TVector3 *v2=&points[n_2];
	for(int i=0;i<n_2;i++, v++, v2++){
		xprod_sum += v->x()*v2->y() - v2->x()*v->y();
	}
	if(xprod_sum>0.0)q = -q;

	// Phi is pi/2 out of phase with x0,y0. The sign of the phase difference
	// depends on the charge
	phi = atan2(y0,x0);
	phi += q>0.0 ? -M_PI_2:M_PI_2;
	
	// Theta is determined by extrapolating the helix back to the target.
	// To do this, we need dphi/dz and a phi,z point. The easiest way to
	// get these is by a simple average (I guess).
	v = points;
	v2=&v[1];
	float dphidz =0.0;
	int Ndphidzpoints = 0;
	for(int i=0;i<Npoints-1;i++, v++, v2++){
		float myphi1 = atan2(v->y()-y0,  v->x()-x0);
		float myphi2 = atan2(v2->y()-y0, v2->x()-x0);
		float mydphidz = (myphi2-myphi1)/(v2->z()-v->z());
		if(finite(mydphidz)){
			dphidz+=mydphidz;
			Ndphidzpoints++;
		}
	}
	if(Ndphidzpoints){
		dphidz/=(float)Ndphidzpoints;
	}
	
	theta = atan(r0*fabs(dphidz));
	p = -p_trans/sin(theta);

#endif

	return NOERROR;
}

//------------------------------------------------------------------
// qsort_points_by_z
//------------------------------------------------------------------
static int qsort_points_by_z(const void* arg1, const void* arg2)
{
	TVector3 *a = (TVector3*)arg1;
	TVector3 *b = (TVector3*)arg2;
	
	if(a->z() == b->z())return 0;
	return b->z() > a->z() ? 1:-1;
}


#if 0
//-----------------
// firstguess_curtis
//-----------------
derror_t DCDC::firstguess_curtis(s_Cdc_trackhit_t *hits, int Npoints
	, float &theta ,float &phi, float &p, float &q)
{
	if(Npoints<3)return NOERROR;
	// pick out 3 points to calculate the circle with
	s_Cdc_trackhit_t *hit1, *hit2, *hit3;
	hit1 = hits;
	hit2 = &hits[Npoints/2];
	hit3 = &hits[Npoints-1];

	float x1 = hit1->x, y1=hit1->y;
	float x2 = hit2->x, y2=hit2->y;
	float x3 = hit3->x, y3=hit3->y;

	float b = (x2*x2+y2*y2-x1*x1-y1*y1)/(2.0*(x2-x1));
	b -= (x3*x3+y3*y3-x1*x1-y1*y1)/(2.0*(x3-x1));
	b /= ((y1-y2)/(x1-x2)) - ((y1-y3)/(x1-x3));
	float a = (x2*x2-y2*y2-x1*x1-y1*y1)/(2.0*(x2-x1));
	a -= b*(y2-y1)/(x2-x1);

	// Above is the method from Curtis's crystal barrel note 93, pg 72
	// Below here is just a copy of the code from David's firstguess
	// routine above (after the x0,y0 calculation)
	float x0=a, y0=b;

	// Momentum depends on magnetic field. Assume 2T for now.
	// Also assume a singly charged track (i.e. q=+/-1)
	// The sign of the charge will be deterined below.
	float B=-2.0*0.593; // The 0.61 is empirical
	q = +1.0;
	float r0 = sqrt(x0*x0 + y0*y0);
	float hbarc = 197.326;
	float p_trans = q*B*r0/hbarc; // are these the right units?
	
	// Assuming points are ordered in increasing z, the sign of the
	// cross-product between successive points will be the opposite
	// sign of the charge. Since it's possible to have a few "bad"
	// points, we don't want to rely on any one to determine this.
	// The method we use is to sum cross-products of the first and
	// middle points, the next-to-first and next-to-middle, etc.
	float xprod_sum = 0.0;
	int n_2 = Npoints/2; 
	s_Cdc_trackhit_t *v = hits;
	s_Cdc_trackhit_t *v2=&hits[n_2];
	for(int i=0;i<n_2;i++, v++, v2++){
		xprod_sum += v->x*v2->y - v2->x*v->y;
	}
	if(xprod_sum>0.0)q = -q;

	// Phi is pi/2 out of phase with x0,y0. The sign of the phase difference
	// depends on the charge
	phi = atan2(y0,x0);
	phi += q>0.0 ? -M_PI_2:M_PI_2;
	
	// Theta is determined by extrapolating the helix back to the target.
	// To do this, we need dphi/dz and a phi,z point. The easiest way to
	// get these is by a simple average (I guess).
	v = hits;
	v2=&v[1];
	float dphidz =0.0;
	int Ndphidzpoints = 0;
	for(int i=0;i<Npoints-1;i++, v++, v2++){
		float myphi1 = atan2(v->y-y0,  v->x-x0);
		float myphi2 = atan2(v2->y-y0, v2->x-x0);
		float mydphidz = (myphi2-myphi1)/(v2->z-v->z);
		if(finite(mydphidz)){
			dphidz+=mydphidz;
			Ndphidzpoints++;
		}
	}
	if(Ndphidzpoints){
		dphidz/=(float)Ndphidzpoints;
	}
	
	theta = atan(r0*fabs(dphidz));
	p = -p_trans/sin(theta);

	return NOERROR;
}

#endif

