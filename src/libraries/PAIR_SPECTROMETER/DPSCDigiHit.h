// $Id$
//
//    File: DPSCDigiHit.h
// Created: Wed Oct 15 16:45:56 EDT 2014
// Creator: staylor (on Linux gluon05.jlab.org 2.6.32-358.18.1.el6.x86_64 x86_64)
//

#ifndef _DPSCDigiHit_
#define _DPSCDigiHit_

#include <JANA/JObject.h>
#include <JANA/JFactory.h>

class DPSCDigiHit:public jana::JObject{
	public:
		JOBJECT_PUBLIC(DPSCDigiHit);
		
		// Add data members here. For example:
		// int id;
		// double E;
		
		// This method is used primarily for pretty printing
		// the second argument to AddString is printf style format
		void toStrings(vector<pair<string,string> > &items)const{
			// AddString(items, "id", "%4d", id);
			// AddString(items, "E", "%f", E);
		}
		
};

#endif // _DPSCDigiHit_
