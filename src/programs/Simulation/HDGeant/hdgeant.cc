
#include <stdlib.h>

#include <iostream>
#include <string>
using namespace std;


// These are defined in copytoplusplus.cc
extern string INFILE;
extern string OUTFILE;
extern bool POSTSMEAR;
extern string MCSMEAROPTS;
extern bool DELETEUNSMEARED;

// Declare routines callable from FORTRAN
extern "C" int hdgeant_(void); // define in hdgeant_f.F


int main(int narg, char *argv[])
{
	// Run hdgeant proper
	int res = hdgeant_();

	// Optionally smear the resulting output file
	if(POSTSMEAR && res == 0){
		string cmd = "mcsmear "+MCSMEAROPTS+" "+OUTFILE;
		cout<<endl;
		cout<<"Smearing data with:"<<endl;
		cout<<endl;
		cout<<cmd<<endl;
		cout<<endl;
		res = system(cmd.c_str());
		
		if(DELETEUNSMEARED && res == 0){
			cmd = "rm -f "+OUTFILE;
			cout<<endl;
			cout<<"Deleting unsmeared file:"<<endl;
			cout<<"   "<<cmd<<endl;
			res = system(cmd.c_str());
                }else if(DELETEUNSMEARED){
			cout<<endl;
			cout<<"Not deleting unsmeared file ";
			cout<<"because of problems with the smearing.";
			cout<<endl;
		}
	}else if(POSTSMEAR){
		cout<<endl;
		cout<<"Skipping smearing step because of problems ";
                cout<<"with the hdgeant simulation"<<endl;
		cout<<endl;
	}else{
		if(DELETEUNSMEARED){
			cerr<<endl;
			cerr<<"The DELETEUNSMEARED flag is set indicating that you want the output file"<<endl;
			cerr<<"\""<<OUTFILE<<"\" to be deleted. However, the POSTSMEAR flag is not set"<<endl;
			cerr<<"so no smeared data file was created so deleting the unsmeared file is"<<endl;
			cerr<<"most likely NOT what you want to do. Therefore, I'm ignoring the"<<endl;
			cerr<<"DELETEUNSMEARED flag. "<<__FILE__<<":"<<__LINE__<<endl;
		}
	}

	return res;
}
