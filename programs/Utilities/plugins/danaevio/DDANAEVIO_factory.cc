// $Id$
//
//    File: DDANAEVIO_factory.cc
// Created: Mon Mar 15 09:08:37 EDT 2010
// Creator: wolin (on Linux stan.jlab.org 2.6.18-164.el5 x86_64)
//
//
//
// JANA factory plugin creates EVIO DOM tree from DANA objects
//
//
// Implements JANA command-line parameters:
//
//    EVIO:DANAEVIO         specify which objects to add to DOM tree, see below for defaults
//    EVIO:DANADICT         use custom XML tag/num dictionary instead of built-in tag/num scheme
//
//
//  Specifying which objects to add to tree:
//    
//    Comma-separated, case-insensitive list (best not to included embedded whitespace)
//    Accepts "all", "none", "truth" and "hits", as well as individual DANA object names
//    Use prefix "-" to invert selection, "+" also accepted
//
//
//  default dictionary is dana_evio_dict.xml
//
//
//
//  Elliott Wolin, 31-Mar-2010
//
//
//
// still to do:
//    optimize multi-threading
//    add evio to external packages
//
//  currently working on:
//    vector<string> and obj nameTag's



#include <iostream>
#include <iomanip>
#include <map>
using namespace std;

#include <expat.h>

#include <JANA/JEventProcessor.h>
#include <JANA/JFactory_base.h>
#include <JANA/JApplication.h>
#include <JANA/JEventLoop.h>
using namespace jana;


#include "DANA/DApplication.h"

#include "TRACKING/DMCThrown.h"
#include "TRACKING/DMCTrackHit.h"
#include "TRACKING/DTrackWireBased.h"
#include "TRACKING/DTrackTimeBased.h"
#include "TRACKING/DMCTrajectoryPoint.h"

#include "FCAL/DFCALTruthShower.h"
#include "FCAL/DFCALHit.h"

#include "BCAL/DBCALTruthShower.h"
#include "BCAL/DHDDMBCALHit.h"

#include "TOF/DTOFTruth.h"
#include "TOF/DHDDMTOFHit.h"

#include "CDC/DCDCHit.h"

#include "FDC/DFDCHit.h"

#include "PID/DBeamPhoton.h"
#include "PID/DPhoton.h"
#include "PID/DChargedTrack.h"

#include "START_COUNTER/DSCHit.h"
#include "START_COUNTER/DSCTruthHit.h"

#include "SplitString.h"


#include "evioFileChannel.hxx"
#include "evioUtil.hxx"
using namespace evio;


#include "DDANAEVIO_factory.h"


// contains encoding of the default XML tag/num definition file in a C++ string
#include "dana_evio_dict.h"


// for one-time initialization
static pthread_mutex_t initMutex = PTHREAD_MUTEX_INITIALIZER;
static bool first_time = true;



//--------------------------------------------------------------------------------------


DDANAEVIO_factory::DDANAEVIO_factory() {


  // get mutex and initialize once only
  pthread_mutex_lock(&initMutex);
  if(first_time) {
    first_time=false;
    
    // fill evioMap based on EVIO:DANAEVIO parameter
    decode_DANAEVIO_parameter();

    // parse file (using EVIO:DANADICT parameter) or default string to get tag/num pairs
    get_tagNum_dictionary();
  }
  pthread_mutex_unlock(&initMutex);
  
}
                                       

//--------------------------------------------------------------------------------------


void DDANAEVIO_factory::decode_DANAEVIO_parameter(void) {

  // decodes JANA parameters    

  // parameters are comma-separated and case-insensitive
  // "-" means invert
  // "+" is ignored
  // also supported: "all", "none", "truth" "hits", "tracks"
  // otherwise parameter must be the name/tag of a DANA object that is processed by this program
    

  // check for parameter:  EVIO:DANAEVIO
  map< string, set<string> >::iterator iter;   
  string danaevio= "";
  gPARMS->SetDefaultParameter("EVIO:DANAEVIO",danaevio);
  if(danaevio!="") {

    vector<string> params;
    SplitString<string>(danaevio,params,",");
    for(unsigned int i=0; i<params.size(); i++) {

      std::transform(params[i].begin(), params[i].end(), params[i].begin(), (int(*)(int)) tolower);
      bool plus=(params[i][0]=='+');
      bool minus=(params[i][0]=='-');
      string value=params[i].substr((plus||minus)?1:0);

      if(value=="all") {
        for(iter=evioMap.begin(); iter!=evioMap.end(); iter++) if(!minus) iter->second.insert(""); else iter->second.erase("");

      } else if(value=="none") {
        for(iter=evioMap.begin(); iter!=evioMap.end(); iter++) if(minus)  iter->second.insert(""); else iter->second.erase("");

      } else if(value=="truth") {
        if(!minus) evioMap["dbeamphoton"].insert("");      else evioMap["dbeamphoton"].erase("");
        if(!minus) evioMap["dmcthrown"].insert("");        else evioMap["dmcthrown"].erase("");
        if(!minus) evioMap["dmctrackhit"].insert("");      else evioMap["dmctrackhit"].erase("");
        if(!minus) evioMap["dfcaltruthshower"].insert(""); else evioMap["dfcaltruthshower"].erase("");
        if(!minus) evioMap["dbcaltruthshower"].insert(""); else evioMap["dbcaltruthshower"].erase("");
        if(!minus) evioMap["dtoftruth"].insert("");        else evioMap["dtoftruth"].erase("");
        if(!minus) evioMap["dsctruth"].insert("");         else evioMap["dsctruth"].erase("");


      } else if(value=="hits") {
        if(!minus) evioMap["dcdchit"].insert("");          else evioMap["dcdchit"].erase("");
        if(!minus) evioMap["dfdchit"].insert("");          else evioMap["dfdchit"].erase("");
        if(!minus) evioMap["dfcalchit"].insert("");        else evioMap["dfcalchit"].erase("");
        if(!minus) evioMap["dhddmbcalchit"].insert("");    else evioMap["dhddmbcalchit"].erase("");
        if(!minus) evioMap["dhddmtofchit"].insert("");     else evioMap["dhddmtofchit"].erase("");
        if(!minus) evioMap["dschit"].insert("");           else evioMap["dschit"].erase("");

      } else if(value=="tracks") {
        if(!minus) evioMap["dtrackwirebased"].insert("");  else evioMap["dtrackwirebased"].erase("");
        if(!minus) evioMap["dtracktimebased"].insert("");  else evioMap["dtracktimebased"].erase("");
        if(!minus) evioMap["dchargedtrack"].insert("");    else evioMap["dchargedtrack"].erase("");
        if(!minus) evioMap["dphoton"].insert("");          else evioMap["dphoton"].erase("");
        if(!minus) evioMap["dcdctrackhit"].insert("");     else evioMap["dcdctrackhit"].erase("");
        if(!minus) evioMap["dfdcpseudo"].insert("");       else evioMap["dfdcpseudo"].erase("");

      } else {

        string::size_type colon = value.find(":");
        string name = value;
        string tag = "";
        if(colon!=string::npos) {
          name = value.substr(0,colon);
          tag  = value.substr(colon+1);
        }

        map< string, set<string> >::iterator found = evioMap.find(name);
        if(found!=evioMap.end()) {
          if(!minus)found->second.insert(tag); else found->second.erase(tag);
        } else {
          jerr << endl << "  ?unknown DANAEVIO parameter: " << params[i] << endl;
        }

      }
    }
  }


  // print factory/tag selection flags
  map<string, set<string> >::iterator iter1;
  set<string>::iterator iter2;
  jout << endl << endl << endl << "  DANA object output flags:" << endl << endl;
  for(iter1=evioMap.begin(); iter1!=evioMap.end(); iter1++) {
    jout << "     " << setiosflags(ios::left) << setw(20) << iter1->first+":";
    if(iter1->second.size()>0) {
      for(iter2=iter1->second.begin(); iter2!=iter1->second.end(); iter2++) {
        jout << setw(20) << string(((*iter2).size()>0)?(*iter2):("untagged")) << string("    ");
      }
    } else {
      jout << setw(20) << "(not selected)";
    }
    jout << endl;
  }
  jout << endl << endl;

}


//----------------------------------------------------------------------------


void DDANAEVIO_factory::get_tagNum_dictionary(void) {


  // init string parser
  XML_Parser xmlParser = XML_ParserCreate(NULL);
  XML_SetElementHandler(xmlParser,startElement,NULL);
      

  // set defaults by parsing internal string
  if(XML_Parse(xmlParser,dana_evio_dict_String.c_str(),dana_evio_dict_String.size(),true)==0) {
    jerr << endl << endl << endl << "  ?get_tagNum_dictionary...parse error for default string"
         << endl << endl << XML_ErrorString(XML_GetErrorCode(xmlParser))
         << endl << endl << endl;
  } else {
    jout << endl << endl << "  Successfully loaded default tag/num dictionary" << endl << endl << endl;
  }
  XML_ParserFree(xmlParser);


  // parse user-supplied dictionary file if specified
  string danadict = "";
  gPARMS->SetDefaultParameter("EVIO:DANADICT",danadict);
  if(danadict!="") {

    const int bufSize = 10000;
    char buf[bufSize];

    // init file parser
    XML_Parser xmlParser2 = XML_ParserCreate(NULL);
    XML_SetElementHandler(xmlParser2,startElement,NULL);

    // open dictionary file
    FILE *f = fopen(danadict.c_str(),"r");
    if(f!=NULL) {
      
      int status,len;
      bool done;
      do {
        len  = fread(buf,1,bufSize,f);
        done = len!=bufSize;
        status=XML_Parse(xmlParser2,buf,len,done);
        if((!done)&&(status==0)) {
          jerr << endl << endl << endl << "  ?read_custom_dictionary...parseXMLFile parse error for " << danadict 
               << endl << endl << XML_ErrorString(XML_GetErrorCode(xmlParser2))
               << endl << endl << endl;
          fclose(f);
          XML_ParserFree(xmlParser2);
          return;
        }
      } while (!done);
      fclose(f);
      
    } else {
      jerr << endl << endl << endl << "  ?Unable to read custom tag/num dictionary:  " << danadict 
           << endl << endl << endl;
      XML_ParserFree(xmlParser2);
      return;
    }

    // successfully parsed dictionary file
    jout << endl << endl << "  Using custom tag/num dictionary:  " << danadict << endl << endl << endl;
    XML_ParserFree(xmlParser2);

  }

}


//----------------------------------------------------------------------------


void DDANAEVIO_factory::startElement(void *userData, const char *xmlname, const char **atts) {
  

  // only process dictionary entries
  if( (strcasecmp(xmlname,"xmldumpDictEntry")!=0)   && 
      (strcasecmp(xmlname,"evioDictEntry")!=0)      &&
      (strcasecmp(xmlname,"danaevioDictEntry")!=0)
      ) return;


  string name = "";
  int tag = 0;
  int num = 0;
  for (int i=0; atts[i]; i+=2) {
    if(strcasecmp(atts[i],"name")==0) {
      name = string(atts[i+1]);
    } else if(strcasecmp(atts[i],"tag")==0) {
      tag = atoi(atts[i+1]);
    } else if(strcasecmp(atts[i],"num")==0) {
      num = atoi(atts[i+1]);
    }
  }

  // add pair to dictionary
  tagMap[name]=pair<int,int>(tag,num);
}
    

//--------------------------------------------------------------------------


jerror_t DDANAEVIO_factory::evnt(JEventLoop *loop, int eventnumber) {



  // clear global object id map
  objIdMap.clear();


  // create single evio tree object and add to factory vector (may create more than one tree in the future)
  pair<int,int> p = tagMap["DanaEvent"];
  DDANAEVIODOMTree  *myDDANAEVIODOMTree = new DDANAEVIODOMTree(p.first,p.second);
  _data.push_back(myDDANAEVIODOMTree);


  // add selected DANA banks to event tree
  if(evioMap["dmctrackhit"        ].size()>0)  addDMCTrackHit(        eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dbeamphoton"        ].size()>0)  addDBeamPhoton(        eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dmcthrown"          ].size()>0)  addDMCThrown(          eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dfcaltruthshower"   ].size()>0)  addDFCALTruthShower(   eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dbcaltruthshower"   ].size()>0)  addDBCALTruthShower(   eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dtoftruth"          ].size()>0)  addDTOFTruth(          eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dsctruthhit"        ].size()>0)  addDSCTruthHit(        eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dmctrajectorypoint" ].size()>0)  addDMCTrajectoryPoint( eventLoop, myDDANAEVIODOMTree->tree);
  
  if(evioMap["dcdchit"            ].size()>0)  addDCDCHit(            eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dfdchit"            ].size()>0)  addDFDCHit(            eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dfcalhit"           ].size()>0)  addDFCALHit(           eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dhddmbcalhit"       ].size()>0)  addDHDDMBCALHit(       eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dhddmtofhit"        ].size()>0)  addDHDDMTOFHit(        eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dschit"             ].size()>0)  addDSCHit(             eventLoop, myDDANAEVIODOMTree->tree);
  
  if(evioMap["dcdctrackhit"       ].size()>0)  addDCDCTrackHit(       eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dfdcpseudo"         ].size()>0)  addDFDCPseudo(         eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dtrackwirebased"    ].size()>0)  addDTrackWireBased(    eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dtracktimebased"    ].size()>0)  addDTrackTimeBased(    eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dchargedtrack"      ].size()>0)  addDChargedTrack(      eventLoop, myDDANAEVIODOMTree->tree);
  if(evioMap["dphoton"            ].size()>0)  addDPhoton(            eventLoop, myDDANAEVIODOMTree->tree);
  

  // add global object id bank
  addObjIdBank(myDDANAEVIODOMTree->tree);


  return NOERROR;
}


//--------------------------------------------------------------------------------------


void DDANAEVIO_factory::addObjIdBank(evioDOMTree &tree) {

  
  // create objIdBank and add to event tree
  evioDOMNodeP objIdBank = createContainerNode("objIdBank");
  tree << objIdBank;
  
  
  // create data banks and add to objIdBank
  evioDOMNodeP idBank       =  createLeafNode<uint64_t>    ("objIdBank.id");
  //    evioDOMNodeP nameTagBank  = createLeafNode<string> ("objIdBank.nameTag");
  //    *objIdBank << idBank << nameTagBank;
  *objIdBank << idBank;
  
  
  // add collected id's and name/tags to data banks
  map<int,string>::iterator iter;
  for(iter=objIdMap.begin(); iter!=objIdMap.end(); iter++) {
    *idBank      << iter->first;
    //      *nameTagBank << iter->second;
  }
  
}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDMCThrown(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create mcthrown bank and add to event tree
  evioDOMNodeP mcthrown = createContainerNode("DMCThrown");
  tree << mcthrown;
  
  
  // create data banks and add to mcthrown
  evioDOMNodeP objIdBank     =  createLeafNode<uint64_t>  ("DMCThrown.objId");
  evioDOMNodeP typeBank      =  createLeafNode<int>       ("DMCThrown.type");
  evioDOMNodeP pdgtypeBank   =  createLeafNode<int>       ("DMCThrown.pdgtype");
  evioDOMNodeP myidBank      =  createLeafNode<int>       ("DMCThrown.myid");
  evioDOMNodeP parentidBank  =  createLeafNode<int>       ("DMCThrown.parentid");
  evioDOMNodeP mechBank      =  createLeafNode<int>       ("DMCThrown.mech");
  evioDOMNodeP xBank         =  createLeafNode<float>     ("DMCThrown.x");
  evioDOMNodeP yBank         =  createLeafNode<float>     ("DMCThrown.y");
  evioDOMNodeP zBank         =  createLeafNode<float>     ("DMCThrown.z");
  evioDOMNodeP pxBank        =  createLeafNode<float>     ("DMCThrown.x");
  evioDOMNodeP pyBank        =  createLeafNode<float>     ("DMCThrown.py");
  evioDOMNodeP pzBank        =  createLeafNode<float>     ("DMCThrown.pz");
  evioDOMNodeP energyBank    =  createLeafNode<float>     ("DMCThrown.energy");
  *mcthrown << objIdBank << typeBank << pdgtypeBank << myidBank << parentidBank << mechBank 
            << xBank << yBank << zBank << pxBank << pyBank << pzBank << energyBank;
  

  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DMCThrown.assocObjectBanks");
  *mcthrown << assocBank;


  // loop over each requested factory, add data to banks
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dmcthrown"].begin(); iter!=evioMap["dmcthrown"].end(); iter++) {
    
    vector<const DMCThrown*> mcthrowns; 
    eventLoop->Get(mcthrowns,(*iter).c_str());
    if(mcthrowns.size()<=0)continue;


    // add track data to banks
    for(unsigned int i=0; i<mcthrowns.size(); i++) {
      *objIdBank    << mcthrowns[i]->id;
      *typeBank     << mcthrowns[i]->type;
      *pdgtypeBank  << mcthrowns[i]->pdgtype;
      *myidBank     << mcthrowns[i]->myid;
      *parentidBank << mcthrowns[i]->parentid;
      *mechBank     << mcthrowns[i]->mech;

      DVector3 pos = mcthrowns[i]->position();
      *xBank        << pos.X();
      *yBank        << pos.Y();
      *zBank        << pos.Z();

      DVector3 mom = mcthrowns[i]->momentum();
      *pxBank       << mom.X();
      *pyBank       << mom.Y();
      *pzBank       << mom.Z();
      
      *energyBank   << mcthrowns[i]->energy();

      objIdMap[mcthrowns[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DMCThrown.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      mcthrowns[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDMCTrackHit(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create mctrackhit bank and add to event tree
  evioDOMNodeP mctrackhit = createContainerNode("DMCTrackHit");
  tree << mctrackhit;


  // create data banks and add to mctrackhit bank
  evioDOMNodeP objIdBank   = createLeafNode<uint64_t>  ("DMCTrackHit.objId");
  evioDOMNodeP rBank       = createLeafNode<float>     ("DMCTrackHit.r");
  evioDOMNodeP phiBank     = createLeafNode<float>     ("DMCTrackHit.phi");
  evioDOMNodeP zBank       = createLeafNode<float>     ("DMCTrackHit.z");
  evioDOMNodeP trackBank   = createLeafNode<int>       ("DMCTrackHit.track");
  evioDOMNodeP primaryBank = createLeafNode<int>       ("DMCTrackHit.primary");
  evioDOMNodeP ptypeBank   = createLeafNode<int>       ("DMCTrackHit.ptype");
  evioDOMNodeP systemBank  = createLeafNode<int>       ("DMCTrackHit.system");
  *mctrackhit << objIdBank << rBank << phiBank << zBank << trackBank << primaryBank 
              << ptypeBank << systemBank;
      
      
  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DMCTrackHit.assocObjectBanks");
  *mctrackhit << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dmctrackhit"].begin(); iter!=evioMap["dmctrackhit"].end(); iter++) {

    // is there any data
    vector<const DMCTrackHit*> mctrackhits; 
    eventLoop->Get(mctrackhits,(*iter).c_str()); 
    if(mctrackhits.size()<=0)continue;


    // add track data to banks
    for(unsigned int i=0; i<mctrackhits.size(); i++) {
      *objIdBank    << mctrackhits[i]->id;
      *rBank        << mctrackhits[i]->r;
      *phiBank      << mctrackhits[i]->phi;
      *zBank        << mctrackhits[i]->z;
      *trackBank    << mctrackhits[i]->track;
      *primaryBank  << mctrackhits[i]->primary;
      *ptypeBank    << mctrackhits[i]->ptype;
      *systemBank   << mctrackhits[i]->system;

      objIdMap[mctrackhits[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DMCTrackHit.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      mctrackhits[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDTOFTruth(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create toftruth bank and add to event tree
  evioDOMNodeP toftruth = createContainerNode("DTOFTruth");
  tree << toftruth;


  // create data banks and add to toftruth bank
  evioDOMNodeP objIdBank   =  createLeafNode<uint64_t>   ("DTOFTruth.objId");
  evioDOMNodeP trackBank   =  createLeafNode<int>        ("DTOFTruth.track");
  evioDOMNodeP primaryBank =  createLeafNode<int>        ("DTOFTruth.primary");
  evioDOMNodeP xBank       =  createLeafNode<float>      ("DTOFTruth.x");
  evioDOMNodeP yBank       =  createLeafNode<float>      ("DTOFTruth.y");
  evioDOMNodeP zBank       =  createLeafNode<float>      ("DTOFTruth.z");
  evioDOMNodeP pxBank      =  createLeafNode<float>      ("DTOFTruth.px");
  evioDOMNodeP pyBank      =  createLeafNode<float>      ("DTOFTruth.py");
  evioDOMNodeP pzBank      =  createLeafNode<float>      ("DTOFTruth.pz");
  evioDOMNodeP tBank       =  createLeafNode<float>      ("DTOFTruth.t");
  evioDOMNodeP EBank       =  createLeafNode<float>      ("DTOFTruth.E");
  evioDOMNodeP ptypeBank   =  createLeafNode<int>        ("DTOFTruth.ptype");
  *toftruth << objIdBank << trackBank << primaryBank << xBank << yBank << zBank 
            << pxBank << pyBank << pzBank << tBank << EBank << ptypeBank;

    
  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DTOFTruth.assocObjectBanks");
  *toftruth << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dtoftruth"].begin(); iter!=evioMap["dtoftruth"].end(); iter++) {

    // is there any data
    vector<const DTOFTruth*> toftruths; 
    eventLoop->Get(toftruths); 
    if(toftruths.size()<=0)continue;
    
    
    // add track data to banks
    for(unsigned int i=0; i<toftruths.size(); i++) {
      *objIdBank   << toftruths[i]->id;
      *trackBank   << toftruths[i]->track;
      *primaryBank << toftruths[i]->primary;
      *xBank       << toftruths[i]->x;
      *yBank       << toftruths[i]->y;
      *zBank       << toftruths[i]->z;
      *pxBank      << toftruths[i]->px;
      *pyBank      << toftruths[i]->py;
      *pzBank      << toftruths[i]->pz;
      *tBank       << toftruths[i]->t;
      *EBank       << toftruths[i]->E;
      *ptypeBank   << toftruths[i]->ptype;
      
      objIdMap[toftruths[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DTOFTruth.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      toftruths[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------



void DDANAEVIO_factory::addDFCALTruthShower(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create fcaltruthshower bank and add to event tree
  evioDOMNodeP fcaltruthshower = createContainerNode("DFCALTruthShower");
  tree << fcaltruthshower;


  // create data banks and add to fcaltruthshower
  evioDOMNodeP objIdBank   = createLeafNode<uint64_t>   ("DFCALTruthShower.objId");
  evioDOMNodeP xBank       = createLeafNode<float>      ("DFCALTruthShower.x");
  evioDOMNodeP yBank       = createLeafNode<float>      ("DFCALTruthShower.y");
  evioDOMNodeP zBank       = createLeafNode<float>      ("DFCALTruthShower.z");
  evioDOMNodeP tBank       = createLeafNode<float>      ("DFCALTruthShower.t");
  evioDOMNodeP pxBank      = createLeafNode<float>      ("DFCALTruthShower.px");
  evioDOMNodeP pyBank      = createLeafNode<float>      ("DFCALTruthShower.py");
  evioDOMNodeP pzBank      = createLeafNode<float>      ("DFCALTruthShower.pz");
  evioDOMNodeP EBank       = createLeafNode<float>      ("DFCALTruthShower.E");
  evioDOMNodeP primaryBank = createLeafNode<float>      ("DFCALTruthShower.primary");
  evioDOMNodeP trackBank   = createLeafNode<float>      ("DFCALTruthShower.track");
  evioDOMNodeP typeBank    = createLeafNode<float>      ("DFCALTruthShower.type");
  *fcaltruthshower << objIdBank << xBank << yBank << zBank << tBank << pxBank << pyBank<< pzBank 
                   << EBank << primaryBank<< trackBank<< typeBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DFCALTruthShower.assocObjectBanks");
  *fcaltruthshower<< assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dfcaltruthshower"].begin(); iter!=evioMap["dfcaltruthshower"].end(); iter++) {

    // is there any data
    vector<const DFCALTruthShower*> fcaltruthshowers; 
    eventLoop->Get(fcaltruthshowers); 
    if(fcaltruthshowers.size()<=0)continue;
    
    
    // add track data to banks
    for(unsigned int i=0; i<fcaltruthshowers.size(); i++) {
      *objIdBank   << fcaltruthshowers[i]->id;
      *xBank       << fcaltruthshowers[i]->x();
      *yBank       << fcaltruthshowers[i]->y();
      *zBank       << fcaltruthshowers[i]->z();
      *tBank       << fcaltruthshowers[i]->t();
      *pxBank      << fcaltruthshowers[i]->px();
      *pyBank      << fcaltruthshowers[i]->py();
      *pzBank      << fcaltruthshowers[i]->pz();
      *EBank       << fcaltruthshowers[i]->E();
      *primaryBank << fcaltruthshowers[i]->primary();
      *trackBank   << fcaltruthshowers[i]->track();
      *typeBank    << fcaltruthshowers[i]->type();
      
      objIdMap[fcaltruthshowers[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DFCALTruthShower.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      fcaltruthshowers[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }    
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDBCALTruthShower(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create bcaltruthshower bank and add to event tree
  evioDOMNodeP bcaltruthshower = createContainerNode("DBCALTruthShower");
  tree << bcaltruthshower;


  // create data banks and add to bcaltruthshower
  evioDOMNodeP objIdBank    =  createLeafNode<uint64_t>    ("DBCALTruthShower.objId");
  evioDOMNodeP trackBank    =  createLeafNode<int>         ("DBCALTruthShower.track");
  evioDOMNodeP primaryBank  =  createLeafNode<int>         ("DBCALTruthShower.primary");
  evioDOMNodeP phiBank      =  createLeafNode<float>       ("DBCALTruthShower.phi");
  evioDOMNodeP rBank        =  createLeafNode<float>       ("DBCALTruthShower.r");
  evioDOMNodeP zBank        =  createLeafNode<float>       ("DBCALTruthShower.z");
  evioDOMNodeP tBank        =  createLeafNode<float>       ("DBCALTruthShower.t");
  evioDOMNodeP EBank        =  createLeafNode<float>       ("DBCALTruthShower.E");
  *bcaltruthshower << objIdBank << trackBank << primaryBank << phiBank << rBank 
                   << zBank << tBank << EBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DBCALTruthShower.assocObjectBanks");
  *bcaltruthshower<< assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dbcaltruthshower"].begin(); iter!=evioMap["dbcaltruthshower"].end(); iter++) {
    
    // is there any data
    vector<const DBCALTruthShower*> bcaltruthshowers; 
    eventLoop->Get(bcaltruthshowers); 
    if(bcaltruthshowers.size()<=0)continue;
    
    
    // add track data to banks
    for(unsigned int i=0; i<bcaltruthshowers.size(); i++) {
      *objIdBank   << bcaltruthshowers[i]->id;
      *trackBank   << bcaltruthshowers[i]->track;
      *primaryBank << bcaltruthshowers[i]->primary;
      *phiBank     << bcaltruthshowers[i]->phi;
      *rBank       << bcaltruthshowers[i]->r;
      *zBank       << bcaltruthshowers[i]->z;
      *tBank       << bcaltruthshowers[i]->t;
      *EBank       << bcaltruthshowers[i]->E;
      
      objIdMap[bcaltruthshowers[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DBCALTruthShower.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      bcaltruthshowers[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDCDCHit(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create cdchit bank and add to event tree
  evioDOMNodeP cdchit = createContainerNode("DCDCHit");
  tree << cdchit;


  // create data banks and add to cdchit bank
  evioDOMNodeP objIdBank = createLeafNode<uint64_t>   ("DCDCHit.objId");
  evioDOMNodeP ringBank  = createLeafNode<int>        ("DCDCHit.ring");
  evioDOMNodeP strawBank = createLeafNode<int>        ("DCDCHit.straw");
  evioDOMNodeP dEBank    = createLeafNode<float>      ("DCDCHit.dE");
  evioDOMNodeP tBank     = createLeafNode<float>      ("DCDCHit.t");
  *cdchit << objIdBank << ringBank << strawBank << dEBank << tBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DCDCHit.assocObjectBanks");
  *cdchit << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dcdchit"].begin(); iter!=evioMap["dcdchit"].end(); iter++) {

    // is there any data
    vector<const DCDCHit*> cdchits; 
    eventLoop->Get(cdchits); 
    if(cdchits.size()<=0)continue;
    
    
    // add track data to banks
    for(unsigned int i=0; i<cdchits.size(); i++) {
      *objIdBank << cdchits[i]->id;
      *ringBank  << cdchits[i]->ring;
      *strawBank << cdchits[i]->straw;
      *dEBank    << cdchits[i]->dE;
      *tBank     << cdchits[i]->t;
      
      objIdMap[cdchits[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DCDCHit.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      cdchits[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDMCTrajectoryPoint(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create bank and add to event tree
  evioDOMNodeP mctrajectorypoint = createContainerNode("DMCTrajectoryPoint");
  tree << mctrajectorypoint;


  // create data banks and add to bank
  evioDOMNodeP objIdBank          = createLeafNode<uint64_t>    ("DMCTrajectoryPoint.objId");
  evioDOMNodeP xBank              = createLeafNode<float>       ("DMCTrajectoryPoint.x");
  evioDOMNodeP yBank              = createLeafNode<float>       ("DMCTrajectoryPoint.y");
  evioDOMNodeP zBank              = createLeafNode<float>       ("DMCTrajectoryPoint.z");
  evioDOMNodeP pxBank             = createLeafNode<float>       ("DMCTrajectoryPoint.px");
  evioDOMNodeP pyBank             = createLeafNode<float>       ("DMCTrajectoryPoint.py");
  evioDOMNodeP pzBank             = createLeafNode<float>       ("DMCTrajectoryPoint.pz");
  evioDOMNodeP EBank              = createLeafNode<float>       ("DMCTrajectoryPoint.E");
  evioDOMNodeP dEBank             = createLeafNode<float>       ("DMCTrajectoryPoint.dE");
  evioDOMNodeP primary_trackBank  = createLeafNode<int>         ("DMCTrajectoryPoint.primary_track");
  evioDOMNodeP trackBank          = createLeafNode<int>         ("DMCTrajectoryPoint.track");
  evioDOMNodeP partBank           = createLeafNode<int>         ("DMCTrajectoryPoint.part");
  evioDOMNodeP radlenBank         = createLeafNode<float>       ("DMCTrajectoryPoint.radlen");
  evioDOMNodeP stepBank           = createLeafNode<float>       ("DMCTrajectoryPoint.step");
  evioDOMNodeP mechBank           = createLeafNode<int>         ("DMCTrajectoryPoint.mech");
  *mctrajectorypoint << objIdBank << xBank << yBank << zBank << pxBank << pyBank << pzBank << EBank << dEBank
                     << primary_trackBank << trackBank << partBank << radlenBank << stepBank << mechBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DMCTrajectoryPoint.assocObjectBanks");
  *mctrajectorypoint << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dmctrajectorypoint"].begin(); iter!=evioMap["dmctrajectorypoint"].end(); iter++) {

    // is there any data
    vector<const DMCTrajectoryPoint*> mctrajectorypoints; 
    eventLoop->Get(mctrajectorypoints); 
    if(mctrajectorypoints.size()<=0)continue;
    
    
    // add track data to banks
    for(unsigned int i=0; i<mctrajectorypoints.size(); i++) {
      *objIdBank          << mctrajectorypoints[i]->id;
      *xBank              << mctrajectorypoints[i]->x;
      *yBank              << mctrajectorypoints[i]->y;
      *zBank              << mctrajectorypoints[i]->z;
      *pxBank             << mctrajectorypoints[i]->px;
      *pyBank             << mctrajectorypoints[i]->py;
      *pzBank             << mctrajectorypoints[i]->pz;
      *EBank              << mctrajectorypoints[i]->E;
      *dEBank             << mctrajectorypoints[i]->dE;
      *primary_trackBank  << mctrajectorypoints[i]->primary_track;
      *trackBank          << mctrajectorypoints[i]->track;
      *partBank           << mctrajectorypoints[i]->part;
      *radlenBank         << mctrajectorypoints[i]->radlen;
      *stepBank           << mctrajectorypoints[i]->step;
      *mechBank           << mctrajectorypoints[i]->mech;
      
      objIdMap[mctrajectorypoints[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DMCTrajectoryPoint.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      mctrajectorypoints[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDFDCHit(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create fdchit bank and add to event tree
  evioDOMNodeP fdchit = createContainerNode("DFDCHit");
  tree << fdchit;


  // create data banks and add to fdchit bank
  evioDOMNodeP objIdBank   = createLeafNode<uint64_t>  ("DFDCHit.objId");
  evioDOMNodeP layerBank   = createLeafNode<int>       ("DFDCHit.layer");
  evioDOMNodeP moduleBank  = createLeafNode<int>       ("DFDCHit.module");
  evioDOMNodeP elementBank = createLeafNode<int>       ("DFDCHit.element");
  evioDOMNodeP planeBank   = createLeafNode<int>       ("DFDCHit.plane");
  evioDOMNodeP gPlaneBank  = createLeafNode<int>       ("DFDCHit.gPlane");
  evioDOMNodeP gLayerBank  = createLeafNode<int>       ("DFDCHit.gLayer");
  evioDOMNodeP qBank       = createLeafNode<float>     ("DFDCHit.q");
  evioDOMNodeP tBank       = createLeafNode<float>     ("DFDCHit.t");
  evioDOMNodeP rBank       = createLeafNode<float>     ("DFDCHit.r");
  evioDOMNodeP typeBank    = createLeafNode<int>       ("DFDCHit.type");
  *fdchit << objIdBank << layerBank << moduleBank << elementBank << planeBank 
          << gPlaneBank << gLayerBank << qBank << tBank << rBank << typeBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DFDCHits.assocObjectBanks");
  *fdchit << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dfdchit"].begin(); iter!=evioMap["dfdchit"].end(); iter++) {

    // is there any data
    vector<const DFDCHit*> fdchits; 
    eventLoop->Get(fdchits); 
    if(fdchits.size()<=0)continue;
    
    
    // add track data to banks
    for(unsigned int i=0; i<fdchits.size(); i++) {
      *objIdBank    << fdchits[i]->id;
      *layerBank    << fdchits[i]->layer;
      *moduleBank   << fdchits[i]->module;
      *elementBank  << fdchits[i]->element;
      *planeBank    << fdchits[i]->plane;
      *gPlaneBank   << fdchits[i]->gPlane;
      *gLayerBank   << fdchits[i]->gLayer;
      *qBank        << fdchits[i]->q;
      *tBank        << fdchits[i]->t;
      *rBank        << fdchits[i]->r;
      *typeBank     << fdchits[i]->type;
      
      objIdMap[fdchits[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DFDCHits.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      fdchits[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//----------------------------------------------------------------------------


void DDANAEVIO_factory::addDBeamPhoton(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create bank and add to event tree
  evioDOMNodeP beamphoton = createContainerNode("DBeamPhoton");
  tree << beamphoton;


  // create data banks and add to bank
  evioDOMNodeP objIdBank = createLeafNode<uint64_t>  ("DBeamPhoton.objId");
  evioDOMNodeP xBank     = createLeafNode<float>     ("DBeamPhoton.x");
  evioDOMNodeP yBank     = createLeafNode<float>     ("DBeamPhoton.y");
  evioDOMNodeP zBank     = createLeafNode<float>     ("DBeamPhoton.z");
  evioDOMNodeP pxBank    = createLeafNode<float>     ("DBeamPhoton.px");
  evioDOMNodeP pyBank    = createLeafNode<float>     ("DBeamPhoton.py");
  evioDOMNodeP pzBank    = createLeafNode<float>     ("DBeamPhoton.pz");
  evioDOMNodeP tBank     = createLeafNode<int>       ("DBeamPhoton.t");
  *beamphoton << objIdBank << xBank<< yBank<< zBank<< pxBank<< pyBank<< pzBank << tBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DBeamPhoton.assocObjectBanks");
  *beamphoton << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dbeamphoton"].begin(); iter!=evioMap["dbeamphoton"].end(); iter++) {

    // is there any data
    vector<const DBeamPhoton*> beamphotons; 
    eventLoop->Get(beamphotons); 
    if(beamphotons.size()<=0)continue;
    

    // add track data to banks
    for(unsigned int i=0; i<beamphotons.size(); i++) {
      *objIdBank << beamphotons[i]->id;
      

      DVector3 pos = beamphotons[i]->position();
      *xBank   <<  pos.X();
      *yBank   <<  pos.Y();
      *zBank   <<  pos.Z();
      
      DVector3 mom = beamphotons[i]->momentum();
      *pxBank  <<  mom.X();
      *pyBank  <<  mom.Y();
      *pzBank  <<  mom.Z();
      
      *tBank  << beamphotons[i]->t;
      
      objIdMap[beamphotons[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DBeamPhoton.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      beamphotons[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDSCTruthHit(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create bank and add to event tree
  evioDOMNodeP sctruthhit = createContainerNode("DSCTruthHit");
  tree << sctruthhit;


  // create data banks and add to bank
  evioDOMNodeP objIdBank    = createLeafNode<uint64_t>  ("DSCTruthHit.objId");
  evioDOMNodeP dEdxBank     = createLeafNode<float>     ("DSCTruthHit.dEdx");
  evioDOMNodeP primaryBank  = createLeafNode<int8_t>    ("DSCTruthHit.primary");
  evioDOMNodeP trackBank    = createLeafNode<int>       ("DSCTruthHit.track");
  evioDOMNodeP ptypeBank    = createLeafNode<int>       ("DSCTruthHit.ptype");
  evioDOMNodeP rBank        = createLeafNode<float>     ("DSCTruthHit.r");
  evioDOMNodeP phiBank      = createLeafNode<float>     ("DSCTruthHit.phi");
  evioDOMNodeP zBank        = createLeafNode<float>     ("DSCTruthHit.z");
  evioDOMNodeP tBank        = createLeafNode<float>     ("DSCTruthHit.t");
  evioDOMNodeP sectorBank   = createLeafNode<int>       ("DSCTruthHit.sector");
  *sctruthhit << objIdBank << dEdxBank << primaryBank << trackBank << ptypeBank << rBank << phiBank 
              << zBank << tBank << sectorBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DSCTruthHit.assocObjectBanks");
  *sctruthhit << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dsctruthhit"].begin(); iter!=evioMap["dsctruthhit"].end(); iter++) {

    // is there any data
    vector<const DSCTruthHit*> sctruthhits;
    eventLoop->Get(sctruthhits); 
    if(sctruthhits.size()<=0)continue;
    

    // add track data to banks
    for(unsigned int i=0; i<sctruthhits.size(); i++) {
      *objIdBank    << sctruthhits[i]->id;
      *dEdxBank     << sctruthhits[i]->dEdx;
      *primaryBank  << (int8_t)sctruthhits[i]->primary;
      *trackBank    << sctruthhits[i]->track;
      *ptypeBank    << sctruthhits[i]->ptype;
      *rBank        << sctruthhits[i]->r;
      *phiBank      << sctruthhits[i]->phi;
      *zBank        << sctruthhits[i]->z;
      *tBank        << sctruthhits[i]->t;
      *sectorBank   << sctruthhits[i]->sector;
      
      objIdMap[sctruthhits[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DSCTruthHit.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      sctruthhits[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDFCALHit(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create bank and add to event tree
  evioDOMNodeP fcalhit = createContainerNode("DFCALHit");
  tree << fcalhit;


  // create data banks and add to bank
  evioDOMNodeP objIdBank   = createLeafNode<uint64_t>  ("DFCALHit.objId");
  evioDOMNodeP rowBank     = createLeafNode<int>       ("DFCALHit.row");
  evioDOMNodeP columnBank  = createLeafNode<int>       ("DFCALHit.column");
  evioDOMNodeP xBank       = createLeafNode<float>     ("DFCALHit.x");
  evioDOMNodeP yBank       = createLeafNode<float>     ("DFCALHit.y");
  evioDOMNodeP EBank       = createLeafNode<float>     ("DFCALHit.E");
  evioDOMNodeP tBank       = createLeafNode<float>     ("DFCALHit.t");
  *fcalhit << objIdBank << rowBank << columnBank << xBank << yBank << EBank << tBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DFCALHit.assocObjectBanks");
  *fcalhit << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dfcalhit"].begin(); iter!=evioMap["dfcalhit"].end(); iter++) {

    // is there any data
    vector<const DFCALHit*> fcalhits;
    eventLoop->Get(fcalhits); 
    if(fcalhits.size()<=0)continue;
    
    
    // add track data to banks
    for(unsigned int i=0; i<fcalhits.size(); i++) {
      *objIdBank   << fcalhits[i]->id;
      *rowBank     << fcalhits[i]->row;
      *columnBank  << fcalhits[i]->column;
      *xBank       << fcalhits[i]->x;
      *yBank       << fcalhits[i]->y;
      *EBank       << fcalhits[i]->E;
      *tBank       << fcalhits[i]->t;
      
      objIdMap[fcalhits[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DFCALHit.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      fcalhits[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDHDDMBCALHit(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create hddm bcal bank and add to event tree
  evioDOMNodeP hddmbcalhit = createContainerNode("DHDDMBCALHit");
  tree << hddmbcalhit;


  // create data banks and add to bank
  evioDOMNodeP objIdBank   = createLeafNode<uint64_t>  ("DHDDMBCALHit.objId");
  evioDOMNodeP moduleBank  = createLeafNode<int>       ("DHDDMBCALHit.module");
  evioDOMNodeP layerBank   = createLeafNode<int>       ("DHDDMBCALHit.layer");
  evioDOMNodeP sectorBank  = createLeafNode<int>       ("DHDDMBCALHit.sector");
  evioDOMNodeP EBank       = createLeafNode<float>     ("DHDDMBCALHit.E");
  evioDOMNodeP tBank       = createLeafNode<float>     ("DHDDMBCALHit.t");
  evioDOMNodeP zLocalBank  = createLeafNode<float>     ("DHDDMBCALHit.zLocal");
  *hddmbcalhit << objIdBank << moduleBank << layerBank << sectorBank << EBank << tBank << zLocalBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DHDDMBCALHit.assocObjectBanks");
  *hddmbcalhit << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dhddmbcalhit"].begin(); iter!=evioMap["dhddmbcalhit"].end(); iter++) {

    // is there any data
    vector<const DHDDMBCALHit*> hddmbcalhits;
    eventLoop->Get(hddmbcalhits); 
    if(hddmbcalhits.size()<=0)continue;


    // add track data to banks
    for(unsigned int i=0; i<hddmbcalhits.size(); i++) {
      *objIdBank   << hddmbcalhits[i]->id;
      *moduleBank  << hddmbcalhits[i]->module;
      *layerBank   << hddmbcalhits[i]->layer;
      *sectorBank  << hddmbcalhits[i]->sector;
      *EBank       << hddmbcalhits[i]->E;
      *tBank       << hddmbcalhits[i]->t;
      *zLocalBank  << hddmbcalhits[i]->zLocal;
      
      objIdMap[hddmbcalhits[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DHDDMBCALHit.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      hddmbcalhits[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDHDDMTOFHit(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create bank and add to event tree
  evioDOMNodeP hddmtofhit = createContainerNode("DHDDMTOFHit");
  tree << hddmtofhit;


  // create data banks and add to bank
  evioDOMNodeP objIdBank    = createLeafNode<uint64_t>   ("DHDDMTOFHit.objId");
  evioDOMNodeP planeBank    = createLeafNode<int>        ("DHDDMTOFHit.plane");
  evioDOMNodeP barBank      = createLeafNode<int>        ("DHDDMTOFHit.bar");
  evioDOMNodeP ptypeBank    = createLeafNode<int>        ("DHDDMTOFHit.ptype");
  evioDOMNodeP t_northBank  = createLeafNode<float>      ("DHDDMTOFHit.t_north");
  evioDOMNodeP dE_northBank = createLeafNode<float>      ("DHDDMTOFHit.dE_north");
  evioDOMNodeP t_southBank  = createLeafNode<float>      ("DHDDMTOFHit.t_south");
  evioDOMNodeP dE_southBank = createLeafNode<float>      ("DHDDMTOFHit.dE_south");
  evioDOMNodeP xBank        = createLeafNode<float>      ("DHDDMTOFHit.x");
  evioDOMNodeP yBank        = createLeafNode<float>      ("DHDDMTOFHit.y");
  evioDOMNodeP zBank        = createLeafNode<float>      ("DHDDMTOFHit.z");
  evioDOMNodeP pxBank       = createLeafNode<float>      ("DHDDMTOFHit.px");
  evioDOMNodeP pyBank       = createLeafNode<float>      ("DHDDMTOFHit.py");
  evioDOMNodeP pzBank       = createLeafNode<float>      ("DHDDMTOFHit.pz");
  evioDOMNodeP EBank        = createLeafNode<float>      ("DHDDMTOFHit.E");
  *hddmtofhit << objIdBank << planeBank << barBank << ptypeBank << t_northBank << dE_northBank 
              << t_southBank << dE_southBank << xBank << yBank << zBank 
              << pxBank << pyBank << pzBank<< EBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DHDDMTOFHit.assocObjectBanks");
  *hddmtofhit<< assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dhddmtofhit"].begin(); iter!=evioMap["dhddmtofhit"].end(); iter++) {
    
    // is there any data
    vector<const DHDDMTOFHit*> hddmtofhits;
    eventLoop->Get(hddmtofhits); 
    if(hddmtofhits.size()<=0)continue;


    // add track data to banks
    for(unsigned int i=0; i<hddmtofhits.size(); i++) {
      *objIdBank    << hddmtofhits[i]->id;
      *planeBank    << hddmtofhits[i]->plane;
      *barBank      << hddmtofhits[i]->bar;
      *ptypeBank    << hddmtofhits[i]->ptype;
      *t_northBank  << hddmtofhits[i]->t_north;
      *dE_northBank << hddmtofhits[i]->dE_north;
      *t_southBank  << hddmtofhits[i]->t_south;
      *dE_southBank << hddmtofhits[i]->dE_south;
      *xBank        << hddmtofhits[i]->x;
      *yBank        << hddmtofhits[i]->y;
      *zBank        << hddmtofhits[i]->z;
      *pxBank       << hddmtofhits[i]->px;
      *pyBank       << hddmtofhits[i]->py;
      *pzBank       << hddmtofhits[i]->pz;
      *EBank        << hddmtofhits[i]->E;
      
      objIdMap[hddmtofhits[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DHDDMTOFHit.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      hddmtofhits[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDSCHit(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create bank and add to event tree
  evioDOMNodeP schit = createContainerNode("DSCHit");
  tree << schit;


  // create data banks and add to bank
  evioDOMNodeP objIdBank   = createLeafNode<uint64_t>  ("DSCHit.objId");
  evioDOMNodeP dEBank      = createLeafNode<float>     ("DSCHit.dE");
  evioDOMNodeP tBank       = createLeafNode<float>     ("DSCHit.t");
  evioDOMNodeP sectorBank  = createLeafNode<int>       ("DSCHit.sector");
  *schit << objIdBank << dEBank << tBank << sectorBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DSCHit.assocObjectBanks");
  *schit << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dschit"].begin(); iter!=evioMap["dschit"].end(); iter++) {

    // is there any data
    vector<const DSCHit*> schits;
    eventLoop->Get(schits); 
    if(schits.size()<=0)continue;


    // add track data to banks
    for(unsigned int i=0; i<schits.size(); i++) {
      *objIdBank   << schits[i]->id;
      *dEBank      << schits[i]->dE;
      *tBank       << schits[i]->t;
      *sectorBank  << schits[i]->sector;
      
      objIdMap[schits[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DSCHit.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      schits[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDTrackWireBased(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create wirebasedtrack bank and add to event tree
  evioDOMNodeP wirebasedtrack = createContainerNode("DTrackWireBased");
  tree << wirebasedtrack;


  // create data banks and add to bank (n.b. time based track has FOM, wire based doesn't)
  evioDOMNodeP objIdBank = createLeafNode<uint64_t>   ("DTrackWireBased.objId");
  evioDOMNodeP chisqBank = createLeafNode<float>      ("DTrackWireBased.chisq");
  evioDOMNodeP NdofBank  = createLeafNode<int>        ("DTrackWireBased.Ndof");
  evioDOMNodeP xBank     = createLeafNode<float>      ("DTrackWireBased.x");
  evioDOMNodeP yBank     = createLeafNode<float>      ("DTrackWireBased.y");
  evioDOMNodeP zBank     = createLeafNode<float>      ("DTrackWireBased.z");
  evioDOMNodeP pxBank    = createLeafNode<float>      ("DTrackWireBased.px");
  evioDOMNodeP pyBank    = createLeafNode<float>      ("DTrackWireBased.py");
  evioDOMNodeP pzBank    = createLeafNode<float>      ("DTrackWireBased.pz");
  evioDOMNodeP qBank     = createLeafNode<float>      ("DTrackWireBased.q");
  evioDOMNodeP EBank     = createLeafNode<float>      ("DTrackWireBased.E");
  evioDOMNodeP massBank  = createLeafNode<float>      ("DTrackWireBased.mass");
  *wirebasedtrack << objIdBank << chisqBank << NdofBank << xBank << yBank << zBank << pxBank << pyBank << pzBank 
                  << qBank << EBank << massBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DTrackWireBased.assocObjectBanks");
  *wirebasedtrack<< assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dtrackwirebased"].begin(); iter!=evioMap["dtrackwirebased"].end(); iter++) {

    // is there any data
    vector<const DTrackWireBased*> wirebasedtracks;
    eventLoop->Get(wirebasedtracks); 
    if(wirebasedtracks.size()<=0)continue;


    // add track data to banks
    for(unsigned int i=0; i<wirebasedtracks.size(); i++) {
      *objIdBank      << wirebasedtracks[i]->id;
      *chisqBank      << wirebasedtracks[i]->chisq;
      *NdofBank       << wirebasedtracks[i]->Ndof;
      *xBank          << wirebasedtracks[i]->x();
      *yBank          << wirebasedtracks[i]->y();
      *zBank          << wirebasedtracks[i]->z();
      *pxBank         << wirebasedtracks[i]->px();
      *pyBank         << wirebasedtracks[i]->py();
      *pzBank         << wirebasedtracks[i]->pz();
      *qBank          << wirebasedtracks[i]->charge();
      *EBank          << wirebasedtracks[i]->energy();
      *massBank       << wirebasedtracks[i]->mass();
      
      objIdMap[wirebasedtracks[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DTrackWireBased.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      wirebasedtracks[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDTrackTimeBased(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create timebasedtrack bank and add to event tree
  evioDOMNodeP timebasedtrack = createContainerNode("DTrackTimeBased");
  tree << timebasedtrack;


  // create data banks and add to bank
  evioDOMNodeP objIdBank = createLeafNode<uint64_t>   ("DTrackTimeBased.objId");
  evioDOMNodeP chisqBank = createLeafNode<float>      ("DTrackTimeBased.chisq");
  evioDOMNodeP NdofBank  = createLeafNode<int>        ("DTrackTimeBased.Ndof");
  evioDOMNodeP FOMBank   = createLeafNode<float>      ("DTrackTimeBased.FOM");
  evioDOMNodeP xBank     = createLeafNode<float>      ("DTrackTimeBased.x");
  evioDOMNodeP yBank     = createLeafNode<float>      ("DTrackTimeBased.y");
  evioDOMNodeP zBank     = createLeafNode<float>      ("DTrackTimeBased.z");
  evioDOMNodeP pxBank    = createLeafNode<float>      ("DTrackTimeBased.px");
  evioDOMNodeP pyBank    = createLeafNode<float>      ("DTrackTimeBased.py");
  evioDOMNodeP pzBank    = createLeafNode<float>      ("DTrackTimeBased.pz");
  evioDOMNodeP qBank     = createLeafNode<float>      ("DTrackTimeBased.q");
  evioDOMNodeP EBank     = createLeafNode<float>      ("DTrackTimeBased.E");
  evioDOMNodeP massBank  = createLeafNode<float>      ("DTrackTimeBased.mass");
  evioDOMNodeP t0Bank    = createLeafNode<float>      ("DTrackTimeBased.t0");
  *timebasedtrack << objIdBank << chisqBank << NdofBank << FOMBank << xBank << yBank << zBank 
                  << pxBank << pyBank << pzBank << qBank << EBank << massBank << t0Bank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DTrackTimeBased.assocObjectBanks");
  *timebasedtrack << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dtracktimebased"].begin(); iter!=evioMap["dtracktimebased"].end(); iter++) {

    // is there any data
    vector<const DTrackTimeBased*> timebasedtracks;
    eventLoop->Get(timebasedtracks); 
    if(timebasedtracks.size()<=0)continue;


    // add track data to banks
    for(unsigned int i=0; i<timebasedtracks.size(); i++) {
      *objIdBank      << timebasedtracks[i]->id;
      *chisqBank      << timebasedtracks[i]->chisq;
      *NdofBank       << timebasedtracks[i]->Ndof;
      *FOMBank        << timebasedtracks[i]->FOM;
      *xBank          << timebasedtracks[i]->x();
      *yBank          << timebasedtracks[i]->y();
      *zBank          << timebasedtracks[i]->z();
      *pxBank         << timebasedtracks[i]->px();
      *pyBank         << timebasedtracks[i]->py();
      *pzBank         << timebasedtracks[i]->pz();
      *qBank          << timebasedtracks[i]->charge();
      *EBank          << timebasedtracks[i]->energy();
      *massBank       << timebasedtracks[i]->mass();
      *t0Bank         << timebasedtracks[i]->t0();

      objIdMap[timebasedtracks[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DTrackTimeBased.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      timebasedtracks[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDChargedTrack(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create chargedtrack bank and add to event tree
  evioDOMNodeP chargedtrack = createContainerNode("DChargedTrack");
  tree << chargedtrack;


  // create data banks and add to chargedtrack bank
  evioDOMNodeP objIdBank      = createLeafNode<uint64_t>  ("DChargedTrack.objId");
  evioDOMNodeP hypothesisBank = createContainerNode       ("DChargedTrack.hypothesisBanks");
  *chargedtrack << objIdBank << hypothesisBank;
  

  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DChargedTrack.assocObjectBanks");
  *chargedtrack << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dchargedtrack"].begin(); iter!=evioMap["dchargedtrack"].end(); iter++) {
    
    // is there any data
    vector<const DChargedTrack*> chargedtracks;
    eventLoop->Get(chargedtracks); 
    if(chargedtracks.size()<=0)continue;
    
    
    // add track data to banks
    for(unsigned int i=0; i<chargedtracks.size(); i++) {
      *objIdBank << chargedtracks[i]->id;
      
      // create id bank for each charged track and add to hypotheses bank
      evioDOMNodeP hypotheses    = createLeafNode<uint64_t>  ("DChargedTrack.hypotheses");
      *hypothesisBank << hypotheses;
      for(unsigned int j=0; j<chargedtracks[i]->hypotheses.size(); j++) {
        *hypotheses << chargedtracks[i]->hypotheses[j]->id;
      }

      objIdMap[chargedtracks[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DChargedTrack.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      chargedtracks[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDPhoton(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create photon bank and add to event tree
  evioDOMNodeP photon = createContainerNode("DPhoton");
  tree << photon;


  // create data banks and add to photon bank
  evioDOMNodeP objIdBank = createLeafNode<uint64_t>  ("DPhoton.objId");
  evioDOMNodeP EBank     = createLeafNode<float>     ("DPhoton.E");
  evioDOMNodeP pxBank    = createLeafNode<float>     ("DPhoton.px");
  evioDOMNodeP pyBank    = createLeafNode<float>     ("DPhoton.py");
  evioDOMNodeP pzBank    = createLeafNode<float>     ("DPhoton.pz");
  evioDOMNodeP xBank     = createLeafNode<float>     ("DPhoton.x");
  evioDOMNodeP yBank     = createLeafNode<float>     ("DPhoton.y");
  evioDOMNodeP zBank     = createLeafNode<float>     ("DPhoton.z");
  evioDOMNodeP tBank     = createLeafNode<float>     ("DPhoton.t");
  evioDOMNodeP TagBank   = createLeafNode<int>       ("DPhoton.Tag");
  *photon << objIdBank << EBank << pxBank << pyBank << pzBank << xBank << yBank << zBank 
          << tBank << TagBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DPhoton.assocObjectBanks");
  *photon << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dphoton"].begin(); iter!=evioMap["dphoton"].end(); iter++) {

    // is there any data
    vector<const DPhoton*> photons;
    eventLoop->Get(photons); 
    if(photons.size()<=0)continue;


    // add track data to banks
    for(unsigned int i=0; i<photons.size(); i++) {
      *objIdBank  << photons[i]->id;
      *EBank      << photons[i]->energy();
      *pxBank     << photons[i]->px();
      *pyBank     << photons[i]->py();
      *pzBank     << photons[i]->pz();
      *xBank      << photons[i]->x();
      *yBank      << photons[i]->y();
      *zBank      << photons[i]->z();
      *tBank      << photons[i]->getTime();
      *TagBank    << photons[i]->getTag();
      
      objIdMap[photons[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DPhoton.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      photons[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDCDCTrackHit(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create cdctrackhit bank and add to event tree
  evioDOMNodeP cdctrackhit = createContainerNode("DCDCTrackHit");
  tree << cdctrackhit;


  // create data banks and add to cdctrackhit bank
  evioDOMNodeP objIdBank   = createLeafNode<uint64_t>  ("DCDCTrackHit.objId");
  evioDOMNodeP ringBank    = createLeafNode<int>       ("DCDCTrackHit.ring");
  evioDOMNodeP strawBank   = createLeafNode<int>       ("DCDCTrackHit.straw");
  evioDOMNodeP xBank       = createLeafNode<float>     ("DCDCTrackHit.x");
  evioDOMNodeP yBank       = createLeafNode<float>     ("DCDCTrackHit.y");
  evioDOMNodeP stereoBank  = createLeafNode<float>     ("DCDCTrackHit.stereo");
  evioDOMNodeP tdriftBank  = createLeafNode<float>     ("DCDCTrackHit.tdrift");
  evioDOMNodeP distBank    = createLeafNode<float>     ("DCDCTrackHit.dist");
  evioDOMNodeP dEBank      = createLeafNode<float>     ("DCDCTrackHit.dE");
  *cdctrackhit << objIdBank << ringBank << strawBank << xBank << yBank << stereoBank 
               << tdriftBank << distBank << dEBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DCDCTrackHit.assocObjectBanks");
  *cdctrackhit << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dcdctrackhit"].begin(); iter!=evioMap["dcdctrackhit"].end(); iter++) {

    // is there any data
    vector<const DCDCTrackHit*> cdctrackhits; 
    eventLoop->Get(cdctrackhits); 
    if(cdctrackhits.size()<=0)continue;


    // add track data to banks
    for(unsigned int i=0; i<cdctrackhits.size(); i++) {
      *objIdBank   << cdctrackhits[i]->id;
      *ringBank    << cdctrackhits[i]->wire->ring;
      *strawBank   << cdctrackhits[i]->wire->straw;
      *xBank       << cdctrackhits[i]->wire->origin.x();
      *yBank       << cdctrackhits[i]->wire->origin.y();
      *stereoBank  << cdctrackhits[i]->wire->stereo;
      *tdriftBank  << cdctrackhits[i]->tdrift;
      *distBank    << cdctrackhits[i]->dist;
      *dEBank      << cdctrackhits[i]->dE;
      
      objIdMap[cdctrackhits[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DCDCTrackHit.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      cdctrackhits[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//------------------------------------------------------------------------------


void DDANAEVIO_factory::addDFDCPseudo(JEventLoop *eventLoop, evioDOMTree &tree) {


  // create fdcpseudo bank and add to event tree
  evioDOMNodeP fdcpseudo = createContainerNode("DFDCPseudo");
  tree << fdcpseudo;


  // create data banks and add to fdcpseudo bank
  evioDOMNodeP objIdBank     = createLeafNode<uint64_t>  ("DFDCPseudo.objId");
  evioDOMNodeP wBank         = createLeafNode<float>     ("DFDCPseudo.w");
  evioDOMNodeP sBank         = createLeafNode<float>     ("DFDCPseudo.s");
  evioDOMNodeP layerBank     = createLeafNode<int>       ("DFDCPseudo.layer");
  evioDOMNodeP wireBank      = createLeafNode<int>       ("DFDCPseudo.wire");
  evioDOMNodeP timeBank      = createLeafNode<float>     ("DFDCPseudo.time");
  evioDOMNodeP distBank      = createLeafNode<float>     ("DFDCPseudo.dist");
  evioDOMNodeP statusBank    = createLeafNode<int>       ("DFDCPseudo.status");
  evioDOMNodeP xBank         = createLeafNode<float>     ("DFDCPseudo.x");
  evioDOMNodeP yBank         = createLeafNode<float>     ("DFDCPseudo.y");
  evioDOMNodeP dEBank        = createLeafNode<float>     ("DFDCPseudo.dE");
  *fdcpseudo << objIdBank << wBank << sBank << layerBank << wireBank << timeBank << distBank
             << statusBank << xBank << yBank << dEBank;


  // create associated object bank and add to main bank
  evioDOMNodeP assocBank = createContainerNode("DFDCPseudo.assocObjectBanks");
  *fdcpseudo << assocBank;


  // loop over each requested factory
  int assocCount = 0;
  set<string>::iterator iter;
  for(iter=evioMap["dfdcpseudo"].begin(); iter!=evioMap["dfdcpseudo"].end(); iter++) {

    // is there any data
    vector<const DFDCPseudo*> fdcpseudos; 
    eventLoop->Get(fdcpseudos); 
    if(fdcpseudos.size()<=0)continue;


    // add track data to banks
    for(unsigned int i=0; i<fdcpseudos.size(); i++) {
      *objIdBank    << fdcpseudos[i]->id;
      *wBank        << fdcpseudos[i]->w;
      *sBank        << fdcpseudos[i]->s;
      *layerBank    << fdcpseudos[i]->wire->layer;
      *wireBank     << fdcpseudos[i]->wire->wire;
      *timeBank     << fdcpseudos[i]->time;
      *distBank     << fdcpseudos[i]->dist;
      *statusBank   << fdcpseudos[i]->status;
      *xBank        << fdcpseudos[i]->x;
      *yBank        << fdcpseudos[i]->y;
      *dEBank       << fdcpseudos[i]->dE;
      
      objIdMap[fdcpseudos[i]->id]="???";


      // get associated object id bank and add to associated object bank
      evioDOMNodeP assocObjs = createLeafNode<uint64_t> ("DFDCPseudo.assocObjects");
      *assocBank << assocObjs;
      
      // get id's, add to id bank and to global object id map
      vector<const JObject*> objs;
      fdcpseudos[i]->GetT(objs); 
      for(unsigned int j=0; j<objs.size(); j++) {
        assocCount++;
        *assocObjs << objs[j]->id;
        objIdMap[objs[j]->id]="???";
      }
    }
  }
  if(assocCount==0)assocBank->cutAndDelete();

}


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

