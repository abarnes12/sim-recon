// $Id$
//
// Created June 10, 2014  David Lawrence

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
using namespace std;

#include <signal.h>
#include <time.h>
#include <stdlib.h>

#include <evioFileChannel.hxx>
#include <evioUtil.hxx>
using namespace evio;


#ifndef _DBG_
#define _DBG_ cout<<__FILE__<<":"<<__LINE__<<" "
#define _DBG__ cout<<__FILE__<<":"<<__LINE__<<endl
#endif

void ParseCommandLineArguments(int narg, char* argv[]);
void Usage(void);
void ctrlCHandle(int x);
void Process(unsigned int &NEvents, unsigned int &NEvents_read);
uint64_t FindEventNumber(evioDOMTree *evt, uint64_t &block_size);


vector<char*> INFILENAMES;
char *OUTFILENAME = NULL;
int QUIT = 0;
unsigned int EVENTS_TO_SKIP = 0;
unsigned int EVENTS_TO_KEEP = 1;
unsigned int SPECIFIC_OFFSET_TO_KEEP = 0;
unsigned int SPECIFIC_EVENT_TO_KEEP = 0;
unsigned int BUFFER_SIZE = 20000000;
bool EVENT_TO_KEEP_MODE = false;



//-----------
// main
//-----------
int main(int narg,char* argv[])
{
	// Set up to catch SIGINTs for graceful exits
	signal(SIGINT,ctrlCHandle);

	ParseCommandLineArguments(narg, argv);
	
	if(SPECIFIC_EVENT_TO_KEEP>0) {
		cout<<"Searching for event " << SPECIFIC_EVENT_TO_KEEP << " ..." << endl;
	}else{
		cout<<"Skipping "<<EVENTS_TO_SKIP<<endl;
		cout<<"Keeping  "<<EVENTS_TO_KEEP<<endl;
	}
	unsigned int NEvents = 0;
	unsigned int NEvents_read = 0;

	// Process all events
	Process(NEvents, NEvents_read);
	
	cout<<endl;
	cout<<" "<<NEvents_read<<" events read, "<<NEvents<<" events written"<<endl;

	return 0;
}

//-----------
// ParseCommandLineArguments
//-----------
void ParseCommandLineArguments(int narg, char* argv[])
{
	INFILENAMES.clear();

	for(int i=1; i<narg; i++){
		char *ptr = argv[i];
		
		if(ptr[0] == '-'){
			switch(ptr[1]){
				case 'h': Usage();  break;
				case 'o': OUTFILENAME=&ptr[2];  break;
				case 'b': BUFFER_SIZE=atoi(&ptr[2]); break;
				case 's': EVENTS_TO_SKIP=atoi(&ptr[2]); break;
				case 'k': EVENTS_TO_KEEP=atoi(&ptr[2]); break;
				case 'e': SPECIFIC_OFFSET_TO_KEEP=atoi(&ptr[2]); break;
				case 'E': SPECIFIC_EVENT_TO_KEEP=atoi(&ptr[2]); EVENT_TO_KEEP_MODE=true;
// 					// WE DON'T SUPPORT THIS YET!!!!
// 					cout << "The -E option is not yet supported. Sorry! Contact davidl@jlab.org if you need this functionality." <<endl;
// 					exit(-2);
					break;
			}
		}else{
			INFILENAMES.push_back(argv[i]);
		}
	}

	if(INFILENAMES.size()==0){
		cout<<endl<<"You must enter a filename!"<<endl<<endl;
		Usage();
	}
	
	if(SPECIFIC_OFFSET_TO_KEEP>0){
		EVENTS_TO_KEEP=1;
		EVENTS_TO_SKIP=SPECIFIC_OFFSET_TO_KEEP-1;
	}

	if(SPECIFIC_EVENT_TO_KEEP>0){
		EVENTS_TO_KEEP=1;
		EVENTS_TO_SKIP=1000000000; // Large number of events to read in while looking for the specified event
	}

	if(OUTFILENAME==NULL){
		if(SPECIFIC_OFFSET_TO_KEEP>0){
			OUTFILENAME = new char[256];
			sprintf(OUTFILENAME,"evt%d.evio", SPECIFIC_OFFSET_TO_KEEP);
		}else if(SPECIFIC_EVENT_TO_KEEP>0){
			OUTFILENAME = new char[256];
			sprintf(OUTFILENAME,"Evt%d.evio", SPECIFIC_EVENT_TO_KEEP);

		}else{
			OUTFILENAME = (char*)"culled.evio";
		}
	}
}


//-----------
// Usage
//-----------
void Usage(void)
{
	cout<<endl<<"Usage:"<<endl;
	cout<<"     evio_cull_events [-oOutputfile] [-sNeventsToSkip] [-kNeventsToKeep] file1.evio file2.evio ..."<<endl;
	cout<<endl;
	cout<<"options:"<<endl;
	cout<<"    -oOutputfile  Set output filename (def. merged_files.evio)"<<endl;
	cout<<"    -sNeventsToSkip  Set number of events to skip (def. 0)"<<endl;
	cout<<"    -kNeventsToKeep  Set number of events to keep (def. 1)"<<endl;
	cout<<"    -eSingleEvent    Keep only the single, specified event (file pos.)"<<endl;
	cout<<"    -ESingleEvent    Keep only the single, specified event (event number)"<<endl;
	cout<<"    -bBufferSize     Size of EVIO input buffer in bytes (def. " << (BUFFER_SIZE>>20) << "MB)" << endl;
	cout<<endl;
	cout<<" This will copy a continguous set of events from the combined event streams"<<endl;
	cout<<" into a seperate output file. The primary use for this would be to copy"<<endl;
	cout<<" a single, problematic event into a seperate file for easier debugging."<<endl;
	cout<<endl;
	cout<<" If the -eNNN option is used then only a single event is extracted"<<endl;
	cout<<" (the NNN-th event) and written to a file with the name evtNNN.hddm."<<endl;
	cout<<" Note that the event is counted from the begining of the file, starting"<<endl;
	cout<<" with \"1\". This does NOT look at the event number stored in the event itself."<<endl;
	cout<<" "<<endl;
	cout<<" If the -ENNN option is used then only a single event is extracted"<<endl;
	cout<<" (the specified event number) and written to a file with the name EvtNNN.hddm."<<endl;
	cout<<" "<<endl;
	cout<<endl;

	exit(0);
}

//-----------------------------------------------------------------
// ctrlCHandle
//-----------------------------------------------------------------
void ctrlCHandle(int x)
{
	QUIT++;
	cerr<<endl<<"SIGINT received ("<<QUIT<<")....."<<endl;
	if(QUIT>=3){
		cerr << "3 or more SIGINTs received. exiting..." << endl;
		exit(-1);
	}
}

//-----------
// Process
//-----------
void Process(unsigned int &NEvents, unsigned int &NEvents_read)
{
	NEvents = 0;
	NEvents_read = 0;
	time_t last_time = time(NULL);;

	// Output file
	cout<<" output file: "<<OUTFILENAME<<endl;
	evioFileChannel ochan(OUTFILENAME, "w");
	ochan.open();
	
	// Loop over input files
	for(unsigned int i=0; i<INFILENAMES.size(); i++){
		try{
			cout << "Opening input file : \"" << INFILENAMES[i] << "\"" << endl;
			evioFileChannel *ichan = new evioFileChannel(INFILENAMES[i], "r", BUFFER_SIZE);
			ichan->open();
			while( ichan->read() ){
				NEvents_read++;
				
				evioDOMTree *dom = NULL;
				bool write_event = false;
				if(SPECIFIC_EVENT_TO_KEEP>0){

					// User specified a specific event by event number within file
					dom = new evioDOMTree(ichan);
					uint64_t block_size=0;
					uint64_t eventnumber = FindEventNumber(dom, block_size);
					uint64_t min_event = eventnumber;
					uint64_t max_event = min_event + block_size - 1;
					if( (SPECIFIC_EVENT_TO_KEEP >= min_event) && (SPECIFIC_EVENT_TO_KEEP <= max_event) ){
						write_event = true;
						if(min_event != max_event){
							cout << endl;
							cout << "WARNING: The CODA block size for this data is not \"1\"!" << endl;
							cout << "The entire block of " << block_size << " events is being written" << endl;
							cout << "that contains the requested event." << endl;
							cout << "Events " << min_event << " - " << max_event << " will be written." << endl;
						}
					}
					
				} else if(NEvents_read > EVENTS_TO_SKIP){

					// User specified event range or specific event by offset
					write_event = true;

				}
			
				if(write_event){
					
					if(!dom) dom = new evioDOMTree(ichan);
					ochan.write(dom);
					if(dom) delete dom;
					NEvents++;
				}
				
				if(NEvents >= EVENTS_TO_KEEP){
					QUIT = true;
					break;
				}

				// print ticker
				time_t t = time(NULL);
				if((t-last_time) >= 2){
					cout << "  " << NEvents_read << " event read  " << NEvents << " events written       \r";
					cout.flush();
					last_time = t;
				}
			}
			cout << endl;
			ichan->close();
			delete ichan;
		}catch(evioException e){
			cerr << e.what() << endl;
			// QUIT=true;
			break;
		}

		if(QUIT) break;
	}
	
	// Close output file
	ochan.close();

}

//----------------
// FindEventNumber
//----------------
uint64_t FindEventNumber(evioDOMTree *evt, uint64_t &block_size)
{
	evioDOMNodeListP bankList = evt->getNodeList(typeIs<uint64_t>());
	evioDOMNodeList::iterator iter = bankList->begin();
	for(; iter!=bankList->end(); iter++){

		// Get pointer to bank data
		evioDOMNodeP bankPtr = *iter;
		const vector<uint64_t> *vec = bankPtr->getVector<uint64_t>();
		if(!vec) continue; // this should never actually happen
		const uint32_t *iptr = (const uint32_t *)&(*vec)[0];
	
		// Check that parent bank is built trigger bank header
		evioDOMNodeP trigger_bank = bankPtr->getParent();
		if( trigger_bank == NULL ) continue;
		if((trigger_bank->tag & 0xFF20) != 0xFF20) continue;
	
		// Check that grand-parent (Physics Event Bank) exists
		evioDOMNodeP physics_event_bank = trigger_bank->getParent();
		if( physics_event_bank == NULL ) continue;

		// Bomb-proof
		if(vec->size() < 2) continue;
		
		// Copy number of events in block
		block_size = (uint64_t)physics_event_bank->num;
	
		// Extract event number
		uint64_t loevent_num = iptr[0];
		uint64_t hievent_num = iptr[1];
		uint64_t event_num = loevent_num + (hievent_num<<32);
	
		return event_num;
	}

	return 0; // couldn't find the Trigger bank bank
}

