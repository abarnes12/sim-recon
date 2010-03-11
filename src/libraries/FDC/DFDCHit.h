//************************************************
// DFDCHit.h: A class defining a general FDC hit
// Author: Craig Bookwalter, David Lawrence
// Date:	March 2006
//************************************************

#ifndef DFDCHIT_H
#define DFDCHIT_H

#include <sstream>
using namespace std;

#include <JANA/JObject.h>
#include <JANA/JFactory.h>
using namespace jana;

///
/// class DFDCHit: definition for a basic FDC hit data type.
///
class DFDCHit : public JObject{
	public:
		JOBJECT_PUBLIC(DFDCHit);		
		int layer;				// 1(V), 2(X), or 3(U)
		int module;				// 1 through 8, 1 module = 3 detection layers
		int element;			// wire or strip number
	    int plane;				// for cathodes only: u(3) or v(1) plane, u@+45,v@-45  
	    int gPlane;				// 1 through 72
	    int gLayer;				// 1 through 24
	    float q;				// charge deposited
	    float t;				// drift time
	    float r;				// perpendicular distance from 
	    						// center of chamber to wire/strip center
	    int type;				// cathode=1, anode=0

		void toStrings(vector<pair<string,string> > &items)const{
			AddString(items, "layer", "%d", layer);
			AddString(items, "module", "%d", module);
			AddString(items, "w/s #", "%d", element);
			AddString(items, "plane", "%s", plane==1 ? "V":(plane==2 ? "X":"U"));
			AddString(items, "gPlane", "%d", gPlane);
			AddString(items, "gLayer", "%d", gLayer);
			AddString(items, "q", "%f", q);
			AddString(items, "t", "%f", t);
			AddString(items, "r", "%f", r);
			AddString(items, "type", "%d", type);
		}
	    	
};


#endif // DFDCHIT_H

