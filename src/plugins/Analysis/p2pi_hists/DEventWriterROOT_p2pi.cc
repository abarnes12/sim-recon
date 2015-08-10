// $Id$
//
//    File: DEventWriterROOT_p2pi.cc
// Created: Thu Jul 23 10:12:00 EDT 2015
// Creator: jrsteven (on Linux ifarm1401 2.6.32-431.el6.x86_64 x86_64)
//

#include "DEventWriterROOT_p2pi.h"

DEventWriterROOT_p2pi::DEventWriterROOT_p2pi(JEventLoop* locEventLoop) : DEventWriterROOT(locEventLoop)
{
	//DO NOT TOUCH THE ROOT TREES OR FILES IN THIS FUNCTION!!!!
	vector<const DReaction*> locReactions;
        Get_Reactions(locEventLoop, locReactions);

	vector<string> locReactionNames;
	locReactionNames.push_back("p2pi_preco");

	for(size_t loc_i = 0; loc_i < locReactions.size(); ++loc_i) {
		for(size_t loc_j = 0; loc_j < locReactionNames.size(); ++loc_j) {
			if(locReactions[loc_i]->Get_ReactionName() != locReactionNames[loc_j]) 
				continue;
			
			// initialize cut actions (is this the right place?)
			dCutAction_TrueBeamParticle = new DCutAction_TrueBeamParticle(locReactions[loc_i]);
			dCutAction_TrueBeamParticle->Initialize(locEventLoop);
			dCutAction_ThrownTopology = new DCutAction_ThrownTopology(locReactions[loc_i], true);
			dCutAction_ThrownTopology->Initialize(locEventLoop);
			dCutAction_TrueCombo = new DCutAction_TrueCombo(locReactions[loc_i], 5.73303E-7, true);
			dCutAction_TrueCombo->Initialize(locEventLoop);
		}
	}
}

DEventWriterROOT_p2pi::~DEventWriterROOT_p2pi(void)
{
	//DO NOT TOUCH THE ROOT TREES OR FILES IN THIS FUNCTION!!!!
}

//void DEventWriterROOT_p2pi::Initialize(JEventLoop* locEventLoop, vector<TString> locReactionNames) 
//{
//       
//}

void DEventWriterROOT_p2pi::Create_CustomBranches_DataTree(TTree* locTree, const DReaction* locReaction, bool locIsMCDataFlag) const
{
	Create_Branch_Fundamental<Bool_t>(locTree, "TrueBeamParticle");
	Create_Branch_Fundamental<Bool_t>(locTree, "TrueTopology");
	Create_Branch_Fundamental<Bool_t>(locTree, "TrueCombo");
}

void DEventWriterROOT_p2pi::Create_CustomBranches_ThrownTree(TTree* locTree) const
{
	//DO NOT: Acquire/release the ROOT lock.  It is already acquired prior to entry into these functions
	//DO NOT: Write any code that requires a lock of ANY KIND. No reading calibration constants, accessing gParams, etc. This can cause deadlock.
	//DO NOT: Call TTree::Fill().  This will be called after calling the custom fill functions.
	//DO: Use the inherited functions for creating/filling branches.  They will make your life MUCH easier: You don't need to manage the branch memory.

	//Examples: See Create_CustomBranches_DataTree
}

void DEventWriterROOT_p2pi::Fill_CustomBranches_DataTree(TTree* locTree, JEventLoop* locEventLoop, const DParticleCombo* locParticleCombo) const
{
	bool trueBeamParticle = false;
	bool trueTopology = false;
	bool trueCombo = false;
	vector<const DMCThrown*> locMCThrowns;
        locEventLoop->Get(locMCThrowns);
	if(!locMCThrowns.empty()){
		trueBeamParticle = (*dCutAction_TrueBeamParticle)(locEventLoop, locParticleCombo);
		trueTopology = (*dCutAction_ThrownTopology)(locEventLoop, locParticleCombo);
		trueCombo = (*dCutAction_TrueCombo)(locEventLoop, locParticleCombo);
	}

	Fill_FundamentalData<Bool_t>(locTree, "TrueBeamParticle", trueBeamParticle); 
	Fill_FundamentalData<Bool_t>(locTree, "TrueTopology", trueTopology); 
	Fill_FundamentalData<Bool_t>(locTree, "TrueCombo", trueCombo); 
}

void DEventWriterROOT_p2pi::Fill_CustomBranches_ThrownTree(TTree* locTree, JEventLoop* locEventLoop) const
{
	//DO NOT: Acquire/release the ROOT lock.  It is already acquired prior to entry into these functions
	//DO NOT: Write any code that requires a lock of ANY KIND. No reading calibration constants, accessing gParams, etc. This can cause deadlock.
	//DO NOT: Call TTree::Fill().  This will be called after calling the custom fill functions.
	//DO: Use the inherited functions for creating/filling branches.  They will make your life MUCH easier: You don't need to manage the branch memory.

	//Examples: See Fill_CustomBranches_DataTree
}
