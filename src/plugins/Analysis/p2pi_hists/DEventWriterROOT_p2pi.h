// $Id$
//
//    File: DEventWriterROOT_p2pi.h
// Created: Thu Jul 23 10:12:00 EDT 2015
// Creator: jrsteven (on Linux ifarm1401 2.6.32-431.el6.x86_64 x86_64)
//

#ifndef _DEventWriterROOT_p2pi_
#define _DEventWriterROOT_p2pi_

#include <map>
#include <string>

#include <ANALYSIS/DEventWriterROOT.h>
#include <ANALYSIS/DCutActions.h>

using namespace std;
using namespace jana;

class DEventWriterROOT_p2pi : public DEventWriterROOT
{
	public:
		DEventWriterROOT_p2pi(JEventLoop* locEventLoop);
		virtual ~DEventWriterROOT_p2pi(void);

	protected:

		//CUSTOM FUNCTIONS: //Write custom code in these functions
			//DO NOT: Acquire/release the ROOT lock.  It is already acquired prior to entry into these functions
			//DO NOT: Write any code that requires a lock of ANY KIND. No reading calibration constants, accessing gParams, etc. This can cause deadlock.
			//DO NOT: Call TTree::Fill().  This will be called after calling the custom fill functions.
			//DO: Use the inherited functions for creating/filling branches.  They will make your life MUCH easier: You don't need to manage the branch memory.
		virtual void Create_CustomBranches_DataTree(TTree* locTree, const DReaction* locReaction, bool locIsMCDataFlag) const;
		virtual void Create_CustomBranches_ThrownTree(TTree* locTree) const;
		virtual void Fill_CustomBranches_DataTree(TTree* locTree, JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo) const;
		virtual void Fill_CustomBranches_ThrownTree(TTree* locTree, JEventLoop* locEventLoop) const;

	private:
		DEventWriterROOT_p2pi(void) : DEventWriterROOT(NULL){}; //don't allow default constructor

		// cut actions to call for filling TTree
                DCutAction_TrueBeamParticle* dCutAction_TrueBeamParticle;
		DCutAction_ThrownTopology* dCutAction_ThrownTopology;
		DCutAction_TrueCombo* dCutAction_TrueCombo;
};

#endif //_DEventWriterROOT_p2pi_
