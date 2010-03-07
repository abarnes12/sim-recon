// $Id: DFCALPhoton_factory.cc 2280 2006-12-07 17:10:06Z davidl $
//
//    File: DFCALPhoton_factory.cc
// Created: Tue May 17 11:57:50 EST 2005
// Creator: remitche (on Linux mantrid00 2.4.20-18.8smp i686)
//

#include <math.h>
#include <DVector3.h>
#include <TLorentzVector.h>
using namespace std;

#include "DFCALPhoton_factory.h"
#include "DFCALGeometry.h"
//#include "DFCALCluster.h"
#include "DFCALHit.h"
#include <JANA/JEvent.h>
#include <JANA/JApplication.h>
using namespace jana;

//----------------
// Constructor
//----------------
DFCALPhoton_factory::DFCALPhoton_factory()
{

// Set of coefficients for non-linear energy corrections 

  VERTEX_X = 0.;
  VERTEX_Y = 0.;
  VERTEX_Z = 65.;
  
  //Regular Lead Glass Fit values for 30cm RHG radius
  NON_LIN_COEF_A1 = 0.53109;
  NON_LIN_COEF_B1 = 2.66426; 
  NON_LIN_COEF_C1 = 2.70763;
  NON_LIN_COEF_alfa1 = 1+0.0191858;

  //Radiation Hard Lead Glass for 30cm radius
  NON_LIN_COEF_A2 = 0.463044;
  NON_LIN_COEF_B2 = 2.4628; 
  NON_LIN_COEF_C2 = 2.39377;
  NON_LIN_COEF_alfa2 = 1+0.03614;

  
  BUFFER_RADIUS = 8.0;   //transition region buffer
  RHG_RADIUS = 30.0; //RHG radius

// Parameters to make shower-depth correction taken from Radphi, 
// slightly modifed to match photon-polar angle
        FCAL_RADIATION_LENGTH = 3.1;
        FCAL_CRITICAL_ENERGY = 0.035;
        FCAL_SHOWER_OFFSET = 1.0;

        gPARMS->SetDefaultParameter("FCAL:VERTEX_X",VERTEX_X);
        gPARMS->SetDefaultParameter("FCAL:VERTEX_Y",VERTEX_Y);
        gPARMS->SetDefaultParameter("FCAL:VERTEX_Z",VERTEX_Z);

	gPARMS->SetDefaultParameter("FCAL:NON_LIN_COEF_A1", NON_LIN_COEF_A1);
	gPARMS->SetDefaultParameter("FCAL:NON_LIN_COEF_B1", NON_LIN_COEF_B1);
	gPARMS->SetDefaultParameter("FCAL:NON_LIN_COEF_C1", NON_LIN_COEF_C1);
	gPARMS->SetDefaultParameter("FCAL:NON_LIN_COEF_alfa1", NON_LIN_COEF_alfa1);

	gPARMS->SetDefaultParameter("FCAL:NON_LIN_COEF_A2", NON_LIN_COEF_A2);
	gPARMS->SetDefaultParameter("FCAL:NON_LIN_COEF_B2", NON_LIN_COEF_B2);
	gPARMS->SetDefaultParameter("FCAL:NON_LIN_COEF_C2", NON_LIN_COEF_C2);
	gPARMS->SetDefaultParameter("FCAL:NON_LIN_COEF_alfa2", NON_LIN_COEF_alfa2);

	gPARMS->SetDefaultParameter("FCAL:BUFFER_RADIUS",BUFFER_RADIUS);
	gPARMS->SetDefaultParameter("FCAL:RHG_RADIUS",RHG_RADIUS);
	
	gPARMS->SetDefaultParameter("FCAL:FCAL_RADIATION_LENGTH", FCAL_RADIATION_LENGTH);
	gPARMS->SetDefaultParameter("FCAL:FCAL_CRITICAL_ENERGY", FCAL_CRITICAL_ENERGY);
	gPARMS->SetDefaultParameter("FCAL:FCAL_SHOWER_OFFSET", FCAL_SHOWER_OFFSET);
	
}

//------------------
// brun
//------------------
jerror_t DFCALPhoton_factory::brun(JEventLoop *loop, int runnumber)
{
	// Get calibration constants
	map<string, double> cluster_merging;
	loop->GetCalib("FCAL/cluster_merging", cluster_merging);
	if(cluster_merging.find("MIN_CLUSTER_SEPARATION")!=cluster_merging.end()){
		MIN_CLUSTER_SEPARATION = cluster_merging["MIN_CLUSTER_SEPARATION"];
		if(debug_level>0)jout<<"MIN_CLUSTER_SEPARATION = "<<MIN_CLUSTER_SEPARATION<<endl;
	}else{
		jerr<<"Unable to get from MIN_CLUSTER_SEPARATION FCAL/cluster_merging in Calib database!"<<endl;
		loop->GetJApplication()->Quit();
	}

	return NOERROR;
}

//------------------
// evnt
//------------------
jerror_t DFCALPhoton_factory::evnt(JEventLoop *eventLoop, int eventnumber)
{
	vector<const DFCALCluster*> fcalClusters;
	eventLoop->Get(fcalClusters);

	// Return immediately if there isn't even one cluster
	if(fcalClusters.size()<1)return NOERROR;
	
	// Here we try and merge FCAL clusters simply by looking to see if they are
	// within a certain distance of one another. We do this by first looping
	// through the list of clusters as many times as is needed until we get
	// a set of lists for which all clusters are at least MIN_CLUSTER_SEPARATION
	// from all clusters in all other lists.
	
	// Start by filling out out lists with one cluster each
	vector<vector<const DFCALCluster*> > merge_lists;
	for ( unsigned int i = 0; i < fcalClusters.size(); i++ ) {
		vector<const DFCALCluster*> merge_list;
		merge_list.push_back(fcalClusters[i]);
		merge_lists.push_back(merge_list);
	}
	
	// Loop until we find no more lists to merge
	bool clusters_were_merged;
	do{
		clusters_were_merged = false;
		
		// Loop over all pairs of cluster lists
		for(unsigned int i=0; i<merge_lists.size(); i++){
			vector<const DFCALCluster*> &merge_list1 = merge_lists[i];
			for(unsigned int j=i+1; j<merge_lists.size(); j++){
				vector<const DFCALCluster*> &merge_list2 = merge_lists[j];
				
				// Loop over all elements of both lists to see if any are
				// within MIN_CLUSTER_SEPARATION.
				for(unsigned int k=0; k<merge_list1.size(); k++){
					DVector3 pos1 = merge_list1[k]->getCentroid();
					for(unsigned int m=0; m<merge_list2.size(); m++){
						DVector3 pos2 = merge_list2[m]->getCentroid();
						
						double separation_xy = (pos1-pos2).Perp();
						if(separation_xy<MIN_CLUSTER_SEPARATION){
							// Phew! if we got here then we need to merge the 2 clusters
							// The easiest way to do this is to just add all clusters
							// from merge_list2 to merge_list1 and clear merge_list2.
							// Then, we ignore empty lists below.
							merge_list1.insert(merge_list1.end(), merge_list2.begin(), merge_list2.end());
							merge_list2.clear();
							clusters_were_merged = true;
						}
					}
					if(clusters_were_merged)break; // we'll need to do the outer "do" loop again anyway so bail now
				}
				if(clusters_were_merged)break; // we'll need to do the outer "do" loop again anyway so bail now
			}
			if(clusters_were_merged)break; // we'll need to do the outer "do" loop again anyway so bail now
		}
	
	}while(clusters_were_merged);

	// Now we loop over the lists of clusters to merge and make a
	// DFCALPhoton from the list. Note that it may well be that each
	// list is still only 1 element long!
	for ( unsigned int i = 0; i < merge_lists.size(); i++ ) {
		vector<const DFCALCluster*> &merge_list = merge_lists[i];
		if(merge_list.size()<1)continue; // ignore empty lists (see comment above)
	
		DFCALPhoton* fcalPhoton = makePhoton( merge_list );

		if ( fcalPhoton->getEnergy() <= 0  ) {
		  cout << "Deleting fcalPhoton " << endl;
		  delete fcalPhoton; 
		  continue;
		}else {
			_data.push_back(fcalPhoton);
		}
	} 

	return NOERROR;
}


//--------------------------------
// makePhoton
//--------------------------------
DFCALPhoton* DFCALPhoton_factory::makePhoton(vector<const DFCALCluster*> &clusters) 
{

	// Loop over list of DFCALCluster objects and calculate the "Non-linear" corrected
	// energy and position for each. We'll use a logarithmic energy-weighting to 
	// find the final position and error.
	double earliest_time = 1.0E6; // initialize to something large
	double Ecorrected_total = 0.0;
	DVector3 posInCal(0.0, 0.0, 0.0);
	DVector3 posErr2(0.0, 0.0, 0.0);
	double weight_sum = 0.0;
	vector<const DFCALCluster*> clusters_used; // we may exclude some clusters with very small weights
	for(unsigned int i=0; i<clusters.size(); i++){
	
		// Get earliest time (is this the right thing to do?)
		double cTime = clusters[i]->getTime();
		if(cTime<earliest_time)earliest_time=cTime;
 		
		// Add errors in quadrature using weight
		double errX = clusters[i]->getRMS_x();
		double errY = clusters[i]->getRMS_y();
		double errZ;  // will be filled by call to GetCorrectedEnergyAndPosition()
		
		// Get corrected energy, position, and errZ
		double Ecorrected;
		DVector3 pos_corrected;
		GetCorrectedEnergyAndPosition(clusters[i], Ecorrected, pos_corrected, errZ);
		
		double weight = log(Ecorrected/0.001);
		if(weight<0.0)continue; // ignore clusters with less than 1MeV
		
		Ecorrected_total += Ecorrected;
		posInCal += weight*pos_corrected;
		posErr2 += weight*DVector3(errX*errX, errY*errY, errZ*errZ);
		weight_sum += weight;
		clusters_used.push_back(clusters[i]);
	}
	
	posInCal *= (1.0/weight_sum);
	posErr2 *= (1.0/weight_sum);
	
	// posErr2 contains errors squared, convert to errors using sqrt
	DVector3 posErr(sqrt(posErr2.X()), sqrt(posErr2.Z()), sqrt(posErr2.Z()));
	
	// Make the DFCALPhoton object
	DFCALPhoton* photon = new DFCALPhoton;

	photon->setEnergy( Ecorrected_total );
	photon->setMom3( Ecorrected_total, posInCal - DVector3(VERTEX_X, VERTEX_Y, VERTEX_Z) );
	photon->setMom4();
	photon->setPosition( posInCal );   
	photon->setPosError(posErr.X(), posErr.Y(), posErr.Z() );
	photon->setTime ( earliest_time );

	return photon;
}

//--------------------------------
// GetCorrectedEnergyAndPosition
//
// Non-linear and depth corrections should be fixed within DFCALPhoton member functions
//--------------------------------
void DFCALPhoton_factory::GetCorrectedEnergyAndPosition(const DFCALCluster* cluster, double &Ecorrected, DVector3 &pos_corrected, double &errZ)
{
// Non-linar energy correction are done here
       int MAXITER = 1000;

        DVector3  posInCal = cluster->getCentroid();
       float x0 = posInCal.Px();
       float y0 = posInCal.Py();
       float hrad = sqrt(x0*x0+y0*y0);
       float x;
       float y;
       float ef;
       
       double A=0.;
       double B=0.;
       double C=0.;
       double alfa=0.;
       
       const vector<DFCALCluster::DFCALClusterHit_t> clust_hits = cluster->GetHits();
       int blocks = clust_hits.size();
       
       double Eclust = cluster->getEnergy();
       float Ein=0;
       float Eout=0;
       float Etot=0;
       float erad;
       
       //transition region
       if(fabs(RHG_RADIUS-hrad) < BUFFER_RADIUS ){
	 for (int h=0;h<blocks;h++){
	   ef= clust_hits[h].E;
	   x = clust_hits[h].x;
	   y = clust_hits[h].y;
	   erad = sqrt(x*x+y*y);
	   if(erad<RHG_RADIUS){
	     
	     Ein=Ein+ef;
   
	   }
	   else{
	     
	     Eout = Eout+ef;

	   }
	 }
	 
   	 Etot=Eout+Ein;
         if ( Etot > 0  ) { 

	    A  = Eout/Etot*NON_LIN_COEF_A1+Ein/Etot*NON_LIN_COEF_A2;
	    B  = Eout/Etot*NON_LIN_COEF_B1+Ein/Etot*NON_LIN_COEF_B2;
	    C  = Eout/Etot*NON_LIN_COEF_C1+Ein/Etot*NON_LIN_COEF_C2;
	    alfa  = Eout/Etot*NON_LIN_COEF_alfa1+Ein/Etot*NON_LIN_COEF_alfa2;

	 }
         else {

            cout << "Warning: invalid cluster_hits energy " << Etot <<endl;

         }

       }
       
       //Inner region
       else if(hrad<RHG_RADIUS){
	 
	 A  = NON_LIN_COEF_A2;
	 B  = NON_LIN_COEF_B2;
	 C  = NON_LIN_COEF_C2;
	 alfa  = NON_LIN_COEF_alfa2;
	 
       }
       
       //Outer Region
       else{
	 
	 A  = NON_LIN_COEF_A1;
	 B  = NON_LIN_COEF_B1;
	 C  = NON_LIN_COEF_C1;
	 alfa  = NON_LIN_COEF_alfa1;
	 
       }

       double Egamma = 0.;

       if ( A > 0 ) { 
 
  	  Egamma = Eclust/A;

          for ( int niter=0; 1; niter++) {

              double energy = Egamma;
              double non_lin_part = pow(Egamma,1+alfa)/(B+C*Egamma);
              Egamma = Eclust/A - non_lin_part;
              if ( fabs( (Egamma-energy)/energy ) < 0.001 ) {

                 break;

              }
              else if ( niter > MAXITER ) {

                 cout << " Iteration failed for cluster energy " << Eclust << endl;
                 Egamma  = 0;
               
                 break;

              }
          
          }

       } 
       else {

          cout << "Warning: DFCALPhoton : parameter A" << A << " is not valid" << endl; 
       }


// then depth corrections 

       if ( Egamma > 0 ) { 

           float xV = VERTEX_X;
           float yV = VERTEX_Y;
           float zV = VERTEX_Z;
           double z0 = DFCALGeometry::fcalFaceZ() - zV;
           double zMax = (FCAL_RADIATION_LENGTH*(
                       FCAL_SHOWER_OFFSET + log(Egamma/FCAL_CRITICAL_ENERGY)));
           double zed = z0;
           double zed1 = z0 + zMax;

           double r0 = sqrt( (x0-xV)*(x0-xV) + (y0-yV)*(y0-yV) );

           int niter;
           for ( niter=0; niter<100; niter++) {

               double tt = r0/zed1;
               zed = z0 + zMax/sqrt( 1 + tt*tt );
               if ( fabs( (zed-zed1) ) < 0.001) {
                  break;
               }
               zed1 = zed;
           }
    
           posInCal.SetZ( zed + zV );
			  errZ = zed - zed1;
		}

		Ecorrected = Egamma;
		pos_corrected = posInCal;
}



