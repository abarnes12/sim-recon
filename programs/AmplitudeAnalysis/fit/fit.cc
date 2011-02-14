
#include <iostream>
#include <fstream>
#include <complex>
#include <string>
#include <vector>
#include <utility>
#include <map>

#include "AMPTOOLS_DATAIO/ROOTDataReader.h"
#include "AMPTOOLS_AMPS/TwoPSAngles.h"
#include "AMPTOOLS_AMPS/ThreePiAngles.h"
#include "AMPTOOLS_AMPS/BreitWigner.h"

#include "MinuitInterface/MinuitMinimizationManager.h"
#include "IUAmpTools/AmplitudeManager.h"
#include "IUAmpTools/Kinematics.h"
#include "IUAmpTools/NormIntInterface.h"
#include "IUAmpTools/ConfigFileParser.h"
#include "IUAmpTools/ConfigurationInfo.h"
#include "IUAmpTools/ParameterManager.h"
#include "IUAmpTools/LikelihoodCalculator.h"

using std::complex;
using namespace std;
using namespace CLHEP;

int main( int argc, char* argv[] ){
	
  // set default parameters
  
  string configfile( "" );
  string treeName( "kin" );
  
  // parse command line
  
  for (int i = 1; i < argc; i++){
    
    string arg(argv[i]);
    
    if (arg == "-c"){  
      if ((i+1 == argc) || (argv[i+1][0] == '-')) arg = "-h";
      else  configfile = argv[++i]; }
    if (arg == "-h"){
      cout << endl << " Usage for: " << argv[0] << endl << endl;
      cout << "\t -c <file>\t Config file" << endl;
      exit(1);}
  }
  
  if (configfile.size() == 0){
    cout << "No config file specified" << endl;
    exit(1);
  }
  
  ConfigFileParser parser(configfile);
  ConfigurationInfo* cfgInfo = parser.getConfigurationInfo();
  cfgInfo->display();
  
  string fitname = cfgInfo->fitName();
  
  vector< ReactionInfo* > reactions = cfgInfo->reactionList();
  
  vector<AmplitudeManager*>     ampManagers;
  vector<NormIntInterface*>     normInts;
  vector<string>                normIntFiles;
  vector<ROOTDataReader*>       dataReaders;
  vector<ROOTDataReader*>       accMCReaders;
  vector<ROOTDataReader*>       genMCReaders;
  vector<LikelihoodCalculator*> likCalcs;
    
  for( unsigned int i = 0; i < reactions.size(); i++){
    
    string fsName = reactions[i]->reactionName();
        
    vector<string> fsParticles  = reactions[i]->particleList();
    vector<string> dataFiles    = reactions[i]->dataFiles();
    vector<string> accMCFiles   = reactions[i]->accMCFiles();
    vector<string> genMCFiles   = reactions[i]->genMCFiles();
    
    ampManagers.push_back( new AmplitudeManager(fsParticles,fsName) );
    ampManagers[i]->registerAmplitudeFactor( BreitWigner() );
    ampManagers[i]->registerAmplitudeFactor( TwoPSAngles() );
    ampManagers[i]->registerAmplitudeFactor( ThreePiAngles() );
    ampManagers[i]->setupFromConfigurationInfo( cfgInfo );
    
    dataReaders.push_back( new ROOTDataReader( dataFiles[0], treeName ) );
    accMCReaders.push_back( new ROOTDataReader( accMCFiles[0], treeName ) );
    genMCReaders.push_back( new ROOTDataReader( genMCFiles[0], treeName ) );

    normInts.push_back( new NormIntInterface( genMCReaders[i],
                                              accMCReaders[i],
                                              *(ampManagers[i]) ) );
    normIntFiles.push_back( reactions[i]->normIntFile() );
  }
  
  // once the amplitude manageres are setup -- configure the minimization
  // and parameter managers 
  
  MinuitMinimizationManager* fitManager = new MinuitMinimizationManager(100);
  fitManager->setPrecision( 1E-13 );
  
  ParameterManager parManager( *fitManager, ampManagers );
  parManager.setupFromConfigurationInfo( cfgInfo );
  
  for ( unsigned int i = 0; i < ampManagers.size(); i++){
        
    likCalcs.push_back( new LikelihoodCalculator( *ampManagers[i], 
                                                  *normInts[i], 
                                                  *dataReaders[i],
                                                  parManager ) );
  }
  
  cout << "LIKELIHOODS BEFORE MINIMIZATION:  " << endl;
  for ( unsigned int i = 0; i < likCalcs.size(); i++){
    cout << "   -2 ln L:  " << (*(likCalcs[i]))() << endl;}
    
  cout << "STARTING MINIMIZATION..." << endl;
  
  fitManager->setStrategy( 1 );
  fitManager->migradMinimization();
  
  if( fitManager->status() != 0 && fitManager->eMatrixStatus() != 3 ){
   
    cout << "ERROR: fit failed..." << endl;
    return 1;
  }

  cout << "LIKELIHOODS AFTER MINIMIZATION:  " << endl;
  for ( unsigned int i = 0; i < likCalcs.size(); i++){
    cout << "   -2 ln L:  " << (*(likCalcs[i]))() << endl;}

  cout << "WRITING PARAMETERS..." << endl;
  
  string outputFile("fit.");  outputFile += fitname;  outputFile += ".txt";
  ofstream outFile(outputFile.c_str());
  parManager.writeParameters( outFile );
  
  cout << "WRITING FINAL NORMALIZATION INTEGRALS.." << endl;
  
  for( unsigned int i = 0; i < normInts.size(); ++i ){

    if( ampManagers[i]->hasAmpWithFreeParam() ) normInts[i]->forceCacheUpdate();
    normInts[i]->exportNormIntCache( normIntFiles[i] );
  }
    
  // cleanup allocated memory
  for (vector<AmplitudeManager*>::iterator itr = ampManagers.begin();
       itr != ampManagers.end(); itr++){ delete *itr; }
  for (vector<NormIntInterface*>::iterator itr = normInts.begin();
       itr != normInts.end(); itr++) { delete *itr; }
  for (vector<ROOTDataReader*>::iterator itr = dataReaders.begin();
       itr != dataReaders.end(); itr++) { delete *itr; }
  for (vector<ROOTDataReader*>::iterator itr = accMCReaders.begin();
       itr != accMCReaders.end(); itr++) { delete *itr; }
  for (vector<ROOTDataReader*>::iterator itr = genMCReaders.begin();
       itr != genMCReaders.end(); itr++) { delete *itr; }
  for (vector<LikelihoodCalculator*>::iterator itr = likCalcs.begin();
       itr != likCalcs.end(); itr++) { delete *itr; }
  
  delete fitManager;
  
  return 0;
}

