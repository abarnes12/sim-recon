// $Id$
//
//    File: DTrajectory.h
// Created: Mon May 17 14:49:48 EDT 2010
// Creator: davidl (on Darwin eleanor.jlab.org 10.2.0 i386)
//

#ifndef _DTrajectory_
#define _DTrajectory_

#include <JANA/jerror.h>

#include <DVector3.h>
#include <HDGEOMETRY/DMagneticFieldMap.h>
#include <HDGEOMETRY/DGeometry.h>

#ifndef qBr2p
#define qBr2p 0.003  // conversion factor for converting q*B*r to GeV/c
#endif // qBr2p

class DTrajectory{
	public:
		DTrajectory(const DMagneticFieldMap *bfield, const DGeometry *geom=NULL);
		virtual ~DTrajectory();

		typedef struct {
			double B[3];	// B-field at x
			double xdir[3];	// RT x-direction
			double ydir[3];	// RT y-direction
			double zdir[3];	// RT z-direction
			double p[3];	// momemtum in lab coordinates
			double Ro;
			double sin_theta;
			double cos_theta;
			bool straight_track; // true if track is straight due to B=0 or q=0
		}RTdirs;

		typedef struct {
			double x,y,z;
			double px,py,pz;
			double P;
			double s; // distance along RT
			double t; // flight time
			double dP; // momentum loss between previous step and this one
			
			// The following are used to calculate the covariance matrix for MULS
			double itheta02;	// running sum of MULS angle theta_0 squared
			double itheta02s;	// ditto but times s
			double itheta02s2;	// ditto but times s^2
		}swim_step_t;
		
		typedef double ThreeVector[3]; // this is needed to allow one to pass by reference

		inline void CalcDirs(ThreeVector &pos, ThreeVector &p, RTdirs &dirs);
		inline void CalcPosMom(double h, RTdirs &dirs, ThreeVector &pos, double *p=NULL);
		void Swim(const DVector3 &pos, const DVector3 &mom, double q=-1000.0, double smax=2000.0);
		//bool AdjustForMaterial(swim_step_t *swim_step, double &h);
		int GetMaterialInfo(double P, DVector3 &pos, DVector3 &mom, double &dP_dx, double &X0, double &s_to_boundary);
		//double dPdx(double ptot, double KrhoZ_overA, double rhoZ_overA,double LogI) const;
		
		unsigned int Nswim_steps;
		swim_step_t *swim_steps;
	
	protected:
	
		bool own_swim_steps;
		unsigned int Max_swim_steps;
		
		double mass;
		
		double ZMIN;
		double ZMAX;
		double RMAX;
		double R2MAX;
		double BOUNDARY_STEP_FRACTION;
		double MIN_STEP_SIZE;
		double MAX_STEP_SIZE;
		
		bool check_material_boundaries;
		
		const DMagneticFieldMap *bfield;
		const DGeometry *geom;

	private:

};



#endif // _DTrajectory_

