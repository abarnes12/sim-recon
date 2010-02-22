// $Id$
//
//    File: DTrackTimeBased.h
// Created: Thu Sep  4 14:02:44 EDT 2008
// Creator: davidl (on Darwin harriet.jlab.org 8.11.1 i386)
//

#ifndef _DTrackTimeBased_
#define _DTrackTimeBased_

#include <JANA/JObject.h>
#include <JANA/JFactory.h>
#include <PID/DKinematicData.h>
#include <TRACKING/DTrackFitter.h>

class DReferenceTrajectory;


class DTrackTimeBased:public DKinematicData{
	public:
		JOBJECT_PUBLIC(DTrackTimeBased);
		
		oid_t trackid;			///< id of DTrackWireBased corresponding to this track
		oid_t candidateid;   ///< id of DTrackCandidate corresponding to this track
		float chisq;			///< Chi-squared for the track (not chisq/dof!)
		int Ndof;				///< Number of degrees of freedom in the fit
		vector<DTrackFitter::pull_t> pulls;	///< Holds pulls used in chisq calc. (not including off-diagonals)

		const DReferenceTrajectory *rt; ///< pointer to reference trjectory representing this track

		double FOM;
		void toStrings(vector<pair<string,string> > &items)const{
			DKinematicData::toStrings(items);
			AddString(items, "candidate","%d",candidateid);
			AddString(items, "wirebased","%d",trackid);
			AddString(items, "chisq", "%f", chisq);
			AddString(items, "Ndof", "%d", Ndof);
			AddString(items, "FOM", "%f",FOM);
		}
};

#endif // _DTrackTimeBased_

