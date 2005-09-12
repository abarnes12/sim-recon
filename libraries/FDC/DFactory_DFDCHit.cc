// $Id$
//
//    File: DFactory_DFDCHit.cc
// Created: Thu Jun  9 10:25:22 EDT 2005
// Creator: davidl (on Darwin wire129.jlab.org 7.8.0 powerpc)
//

#include "DFactory_DFDCHit.h"

//------------------
// evnt
//------------------
derror_t DFactory_DFDCHit::evnt(DEventLoop *eventLoop, int eventnumber)
{
	/// Place holder for now. 

	return NOERROR;
}

//------------------
// Extract_HDDM
//------------------
derror_t DFactory_DFDCHit::Extract_HDDM(s_HDDM_t *hddm_s, vector<void*> &v)
{
	/// Copies the data from the given hddm_s structure. This is called
	/// from DEventSourceHDDM::GetObjects.
	
	v.clear();

	// Loop over Physics Events
	s_PhysicsEvents_t* PE = hddm_s->physicsEvents;
	if(!PE) return NOERROR;
	
	for(unsigned int i=0; i<PE->mult; i++){
		s_Chambers_t *chambers = NULL;
		if(PE->in[i].hitView)
			if(PE->in[i].hitView->forwardDC)
				chambers = PE->in[i].hitView->forwardDC->chambers;
		if(!chambers)continue;
		
		for(unsigned int j=0;j<chambers->mult;j++){
			int  layer = chambers->in[j].layer;
			int  module = chambers->in[j].module;

			s_AnodePlanes_t *anodeplanes = chambers->in[j].anodePlanes;
			if(anodeplanes){
			
				for(unsigned int k=0;k<anodeplanes->mult;k++){
					float tau = anodeplanes->in[k].tau;
					float z = anodeplanes->in[k].z;
					s_Wires_t *wires = anodeplanes->in[k].wires;
					if(!wires)continue;
				
					for(unsigned int m=0;m<wires->mult;m++){
						float u = wires->in[m].u;
						s_Hits_t *hits = wires->in[m].hits;
						if(!hits)continue;
						for(unsigned int n=0;n<hits->mult;n++){
							float dE = hits->in[n].dE;
							float t = hits->in[n].t;
					
							DFDCHit *fdchit = new DFDCHit;
							fdchit->layer = layer;
							fdchit->module = module;
							fdchit->tau = tau;
							fdchit->z = z;
							fdchit->u = u;
							fdchit->dE = dE;
							fdchit->t = t;
							fdchit->type = 0;
							v.push_back(fdchit);
						}
					}
				}
			}

			s_CathodePlanes_t *cathodeplanes = chambers->in[j].cathodePlanes;
			if(cathodeplanes){
			
				for(unsigned int k=0;k<cathodeplanes->mult;k++){
					float tau = cathodeplanes->in[k].tau;
					float z = cathodeplanes->in[k].z;
					s_Strips_t *strips = cathodeplanes->in[k].strips;
					if(!strips)continue;
				
					for(unsigned int m=0;m<strips->mult;m++){
						float u = strips->in[m].u;
						s_Hits_t *hits=strips->in[m].hits;
						if (!hits) continue;

						for (unsigned int n=0;n<hits->mult;n++){
					
						  DFDCHit *fdchit = new DFDCHit;
						  fdchit->layer = layer;
						  fdchit->module = module;
						  fdchit->tau = tau;
						  fdchit->z = z;
						  fdchit->u = u;
						  fdchit->dE = hits->in[n].dE;
						  fdchit->t = 0;
						  fdchit->type = 1;

						  v.push_back(fdchit);
						}
					}
				}
			}
		}
	}

	return NOERROR;
}

//------------------
// toString
//------------------
const string DFactory_DFDCHit::toString(void)
{
	// Ensure our Get method has been called so _data is up to date
	Get();
	if(_data.size()<=0)return string(); // don't print anything if we have no data!

	printheader("row: layer: module: tau(rad):    z(cm):  u(cm):  dE(MeV):   t(ns):   type:");
	
	for(unsigned int i=0; i<_data.size(); i++){
		DFDCHit *fdchit = _data[i];
		
		printnewrow();
		printcol("%d",	i);
		printcol("%d", fdchit->layer);
		printcol("%d", fdchit->module);
		printcol("%3.1f", fdchit->tau);
		printcol("%3.1f", fdchit->z);
		printcol("%2.3f", fdchit->u);
		if(!fdchit->type){
			printcol("%1.3f", fdchit->dE*1000.0);
			printcol("%4.0f", fdchit->t);
		}else{
			printcol("");
			printcol("");
		}
		printcol("%s", fdchit->type ? "cathode":"anode");
		printrow();
	}

	return _table;
}
