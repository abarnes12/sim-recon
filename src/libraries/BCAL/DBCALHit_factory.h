// $Id$
//
//    File: DBCALHit_factory.h
// Created: Tue Aug  6 09:26:13 EDT 2013
// Creator: davidl (on Darwin harriet.jlab.org 11.4.2 i386)
//

#ifndef _DBCALHit_factory_
#define _DBCALHit_factory_

#include <vector>
#include <map>
#include <utility>
using namespace std;

#include <JANA/JFactory.h>
#include "TTAB/DTranslationTable.h"
#include "DBCALHit.h"

// store up/down-stream values for each detector cell
typedef pair<double,double> cell_calib_t;  
typedef vector<cell_calib_t>  bcal_digi_constants_t;

class DBCALHit_factory:public jana::JFactory<DBCALHit>{
	public:
		DBCALHit_factory(){
		  PRINTCALIBRATION = false;
		  if(gPARMS){
		    gPARMS->SetDefaultParameter("BCALHIT:PRINTCALIBRATION", PRINTCALIBRATION, "Print the calibration parameters.");
		  }
		};
		~DBCALHit_factory(){};

		bool PRINTCALIBRATION;

		// shortcut geometry factors
		// these should really be taken from
		// DBCALGeometry/DGeometry objects
		static const int BCAL_NUM_MODULES  = 48;
		static const int BCAL_NUM_LAYERS   =  4;
		static const int BCAL_NUM_SECTORS  =  4;
		static const int BCAL_MAX_CHANNELS =  1536;

		// overall scale factors
		double a_scale;
		double t_scale;
		double t_base;
		
		// constants tables
		bcal_digi_constants_t gains;
		bcal_digi_constants_t pedestals;
		bcal_digi_constants_t time_offsets;

		
		const int GetCalibIndex( int module, int layer, int sector ) const {
			return BCAL_NUM_LAYERS*BCAL_NUM_SECTORS*(module-1) + BCAL_NUM_SECTORS*(layer-1) + (sector-1);
		}

		const double GetConstant( const bcal_digi_constants_t &the_table,
					  const int in_module, const int in_layer,
					  const int in_sector, const int in_end) const;
		const double GetConstant( const bcal_digi_constants_t &the_table,
					  const DBCALDigiHit *the_digihit) const;
		const double GetConstant( const bcal_digi_constants_t &the_table,
					  const DBCALHit *the_hit) const;
		//const double GetConstant( const bcal_digi_constants_t &the_table,
		//			  const DTranslationTable *ttab,
		//			  const int in_rocid, const int in_slot, const int in_channel) const;

	private:
		jerror_t init(void);						///< Called once at program start.
		jerror_t brun(jana::JEventLoop *eventLoop, int runnumber);	///< Called everytime a new run number is detected.
		jerror_t evnt(jana::JEventLoop *eventLoop, int eventnumber);	///< Called every event.
		jerror_t erun(void);						///< Called everytime run number changes, provided brun has been called.
		jerror_t fini(void);						///< Called after last event of last event source has been processed.
		
		void FillCalibTable( bcal_digi_constants_t &table, 
				     const vector<double> &raw_table);
		    
};

#endif // _DBCALHit_factory_

