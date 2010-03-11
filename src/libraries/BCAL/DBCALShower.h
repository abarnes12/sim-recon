#ifndef _DBCALShower_
#define _DBCALShower_

#include <JANA/JObject.h>
#include <JANA/JFactory.h>
using namespace jana;

class DBCALShower:public JObject{
	public:
		JOBJECT_PUBLIC(DBCALShower);

    float E;    
    float Ecorr;
    float x;
    float y;
    float z;   
    float t;
    int N_cell;
    int total_layer_cluster;
    float Apx_x;
    float Apx_y;
    float Apx_z;
    float error_Apx_x;
    float error_Apx_y;
    float error_Apx_z;
    float Cx;
    float Cy;
    float Cz;
    float error_Cx;
    float error_Cy;
    float error_Cz;
    float t_rms_a;
    float t_rms_b;

	void toStrings(vector<pair<string,string> > &items)const{
			AddString(items, "x", "%5.2f", x);
			AddString(items, "y", "%5.2f", y);
			AddString(items, "z", "%5.2f", z);
			AddString(items, "t", "%5.2f", t);
			AddString(items, "E", "%5.2f", E);
	}
};

#endif // _DBCALShower_

