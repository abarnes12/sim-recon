//
// File: DTAGMGeometry_factory.h
// Created: Sat Jul 5 10:09:27 EST 2014
// Creator: jonesrt on gluey.phys.uconn.edu
//

#ifndef _DTAGMGeometry_factory_
#define _DTAGMGeometry_factory_

#include "JANA/JFactory.h"
#include "DTAGMGeometry.h"

class DTAGMGeometry_factory : public JFactory<DTAGMGeometry> {
 public:
   DTAGMGeometry_factory() {}
   ~DTAGMGeometry_factory(){}

 private:
   jerror_t brun(JEventLoop *loop, int runnumber);   
   jerror_t erun(void);   
};

#endif // _DTAGMGeometry_factory_
