// $Id$
//
//    File: DEventWriterROOT_factory_p2pi.h
// Created: Thu Jul 23 10:12:00 EDT 2015
// Creator: jrsteven (on Linux ifarm1401 2.6.32-431.el6.x86_64 x86_64)
//

#ifndef _DEventWriterROOT_factory_p2pi_
#define _DEventWriterROOT_factory_p2pi_

#include <JANA/JFactory.h>
#include <JANA/JEventLoop.h>

#include "DEventWriterROOT_p2pi.h"

class DEventWriterROOT_factory_p2pi : public jana::JFactory<DEventWriterROOT>
{
	public:
		DEventWriterROOT_factory_p2pi(){use_factory = 1;}; //prevents JANA from searching the input file for these objects
		~DEventWriterROOT_factory_p2pi(){};
		const char* Tag(void){return "p2pi";}

	private:
		jerror_t brun(jana::JEventLoop *locEventLoop, int locRunNumber)
		{
			// Create single DEventWriterROOT_p2pi object and marks the factory as persistent so it doesn't get deleted every event.
			SetFactoryFlag(PERSISTANT);
			ClearFactoryFlag(WRITE_TO_OUTPUT);
			_data.push_back(new DEventWriterROOT_p2pi(locEventLoop));
			return NOERROR;
		}
};

#endif // _DEventWriterROOT_factory_p2pi_
