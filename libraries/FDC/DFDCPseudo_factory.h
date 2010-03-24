//******************************************************************
// DFDCPseudo_factory.h: class definition for a factory creating
// pseudopoints from anode hits and cathode clusters.
// Author: Craig Bookwalter
// Date: Apr 2006
// Several revisions made by Simon Taylor, Fall 2006
//******************************************************************
#ifndef DFACTORY_DFDCPSEUDO_H
#define DFACTORY_DFDCPSEUDO_H

#include <JANA/JFactory.h>
#include <JANA/JObject.h>
#include <JANA/JException.h>
#include <JANA/JStreamLog.h>
using namespace jana;

#include "DFDCPseudo.h"
#include "DFDCCathodeCluster.h"
#include "DFDCHit.h"
#include "DFDCGeometry.h"
#include "HDGEOMETRY/DGeometry.h"
#include <TRACKING/DMCTrackHit.h>

#include <DMatrix.h>
#include <TDecompLU.h>

#include <algorithm>
#include <map>
#include <cmath>

typedef struct {
  float pos;
  float q;
  int numstrips;
  float t; // mean time of strips in peak
  float t_rms; // rms of strips in peak
  unsigned int cluster; // index for cluster from which this centroid was generated
}centroid_t;

///
/// class DFDCPseudo_factory: definition for a JFactory that
/// produces pseudopoints from anode hits and DFDCCathodeClusters.
/// For now, it is purely geometry-based.
/// 
class DFDCPseudo_factory : public JFactory<DFDCPseudo> {
	public:
		
		///
		/// DFDCPseudo_factory::DFDCPseudo_factory():
		/// default constructor -- initializes log file
		///
		DFDCPseudo_factory();
		
		///
		/// DFDCPseudo_factory::~DFDCPseudo_factory():
		/// default destructor -- closes log file
		///
		~DFDCPseudo_factory();	
							

	protected:
		///
		/// DFDCPseudo_factory::evnt():
		/// this is the place that anode hits and DFDCCathodeClusters 
		/// are organized into pseudopoints.
		/// For now, this is done purely by geometry, with no drift
		/// information. See also
		/// DFDCPseudo_factory::makePseudo().
		///
		jerror_t evnt(JEventLoop *eventLoop, int eventNo);
		jerror_t brun(JEventLoop *loop, int runnumber);

		/// 
		/// DFDCPseudo_factory::makePseudo():
		/// performs UV+X matching to create pseudopoints
		///
		void makePseudo( vector<const DFDCHit*>& x,
				 vector<const DFDCCathodeCluster*>& u,
				 vector<const DFDCCathodeCluster*>& v,
				 int layer,
				 vector<const DMCTrackHit*> &mctrackhits);
		
		///
		/// DFDCPseudo_factory::CalcMeanTime()
		/// Calculates mean and RMS time for a cluster of cathode hits
		///
		void CalcMeanTime(const vector<const DFDCHit*>& H, float &t, float &t_rms);
		void CalcMeanTime(vector<const DFDCHit *>::const_iterator peak, float &t, float &t_rms);
		
		///
		/// DFDCPseudo_factory::FindCentroid()
		/// Calculates the centroids of groups of three adjacent strips
		/// containing a peak.
		///
		jerror_t FindCentroid(const vector<const DFDCHit*>& H, 
				 vector<const DFDCHit *>::const_iterator peak,
				 vector<centroid_t> &centroids);
		// Backtracking routine needed by FindCentroid 
		jerror_t FindNewParmVec(DMatrix N,
						       DMatrix X,
						       DMatrix F,
						       DMatrix J,DMatrix par,
						       DMatrix &newpar);
 		
	private:
		std::vector<centroid_t>upeaks;
		std::vector<centroid_t>vpeaks;
		vector<vector<DFDCWire*> >fdcwires;

		JStreamLog* _log;
};

#endif // DFACTORY_DFDCPSEUDO_H

