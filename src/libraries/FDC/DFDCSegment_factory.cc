//************************************************************************
// DFDCSegment_factory.cc - factory producing track segments from pseudopoints
//************************************************************************

#include "DFDCSegment_factory.h"
#include "DANA/DApplication.h"
//#include "HDGEOMETRY/DLorentzMapCalibDB.h"
#include <math.h>

#define HALF_CELL 0.5
#define MAX_DEFLECTION 0.15
#define EPS 1e-8
#define KILL_RADIUS 5.0 
#define Z_TARGET 65.0
#define Z_VERTEX_CUT 25.0
#define MATCH_RADIUS 5.0
#define ADJACENT_MATCH_RADIUS 1.0
#define SIGN_CHANGE_CHISQ_CUT 10.0
#define BEAM_VARIANCE 0.01 // cm^2
#define USED_IN_SEGMENT 0x8
#define CORRECTED 0x10
#define MAX_ITER 10
#define TARGET_LENGTH 30.0 //cm
#define MIN_TANL 2.0

// Variance for position along wire using PHENIX angle dependence, transverse
// diffusion, and an intrinsic resolution of 127 microns.
inline double fdc_y_variance(double alpha,double x){
  return 0.00060+0.0064*tan(alpha)*tan(alpha)+0.0004*fabs(x);
}

bool DFDCSegment_package_cmp(const DFDCPseudo* a, const DFDCPseudo* b) {
  return a->wire->layer>b->wire->layer;
}

DFDCSegment_factory::DFDCSegment_factory() {
        _log = new JStreamLog(std::cout, "FDCSEGMENT >>");
        *_log << "File initialized." << endMsg;
}


///
/// DFDCSegment_factory::~DFDCSegment_factory():
/// default destructor -- closes log file
///
DFDCSegment_factory::~DFDCSegment_factory() {
        delete _log;
}
///
/// DFDCSegment_factory::brun():
/// Initialization: read in deflection map, get magnetic field map
///
jerror_t DFDCSegment_factory::brun(JEventLoop* eventLoop, int eventNo) { 
  DApplication* dapp=dynamic_cast<DApplication*>(eventLoop->GetJApplication());
  bfield = dapp->GetBfield();
  lorentz_def=dapp->GetLorentzDeflections();

  *_log << "Table of Lorentz deflections initialized." << endMsg;
  return NOERROR;
}

///
/// DFDCSegment_factory::evnt():
/// Routine where pseudopoints are combined into track segments
///
jerror_t DFDCSegment_factory::evnt(JEventLoop* eventLoop, int eventNo) {
  myeventno=eventNo;

  vector<const DFDCPseudo*>pseudopoints;
  eventLoop->Get(pseudopoints);  
  
  // Copy into local vector
  vector<DFDCPseudo*>points;
  for (vector<const DFDCPseudo*>::iterator i=pseudopoints.begin();
       i!=pseudopoints.end();i++){
    DFDCPseudo *temp=new DFDCPseudo();
    *temp=*(*i);
    temp->ds=0.;  //initialize correction along wire
    temp->dw=0.;  // initialize correction transverse to wire
    points.push_back(temp);
  }
  
  // Skip segment finding if there aren't enough points to form a sensible 
  // segment 
  if (pseudopoints.size()>=3){
    // Group pseudopoints by package
    vector<DFDCPseudo*>package[4];
    for (vector<DFDCPseudo*>::iterator i=points.begin();i!=points.end();i++){
      package[((*i)->wire->layer-1)/6].push_back(*i);
    }
    
    // Find the segments in each package
    for (int j=0;j<4;j++){
      std::sort(package[j].begin(),package[j].end(),DFDCSegment_package_cmp);
      // We need at least 3 points to define a circle, so skip if we don't 
      // have enough points.
      if (package[j].size()>2) FindSegments(package[j]);
    } 
  } // pseudopoints>2
  
  return NOERROR;
}

// Riemann Line fit: linear regression of s on z to determine the tangent of 
// the dip angle and the z position of the closest approach to the beam line.
// Also returns predicted positions along the helical path.
//
jerror_t DFDCSegment_factory::RiemannLineFit(vector<DFDCPseudo *>points,
					     DMatrix CR,DMatrix &XYZ){
  unsigned int n=points.size()+1;
  vector<int>bad(n);  // Keep track of "bad" intersection points
  // Fill matrix of intersection points
  for (unsigned int m=0;m<n-1;m++){
    double r2=points[m]->x*points[m]->x+points[m]->y*points[m]->y;
    double x_int0,temp,y_int0;
    double denom= N[0]*N[0]+N[1]*N[1];
    double numer=dist_to_origin+r2*N[2];
   
    x_int0=-N[0]*numer/denom;
    y_int0=-N[1]*numer/denom;
    temp=denom*r2-numer*numer;
    if (temp<0){    
      bad[m]=1;
      XYZ(m,0)=x_int0;
      XYZ(m,1)=y_int0;
      continue; 
    }
    temp=sqrt(temp)/denom;
    
    // Choose sign of square root based on proximity to actual measurements
    double diffx1=x_int0+N[1]*temp;
    double diffy1=y_int0-N[0]*temp;
    double diffx2=x_int0-N[1]*temp;
    double diffy2=y_int0+N[0]*temp;
    if (m<n-1){
      diffx1-=points[m]->x;
      diffx2-=points[m]->x;
      diffy1-=points[m]->y;
      diffy2-=points[m]->y;		       
    }
    
    if (diffx1*diffx1+diffy1*diffy1 > diffx2*diffx2+diffy2*diffy2){
      XYZ(m,0)=x_int0-N[1]*temp;
      XYZ(m,1)=y_int0+N[0]*temp;
    }
    else{
      XYZ(m,0)=x_int0+N[1]*temp;
      XYZ(m,1)=y_int0-N[0]*temp;
    }
  }
  // Fake target point
  XYZ(n-1,0)=XYZ(n-1,1)=0.;
  
  // All arc lengths are measured relative to some reference plane with a hit.
  // Don't use a "bad" hit for the reference...
  unsigned int start=0;
  for (unsigned int i=0;i<bad.size();i++){
    if (!bad[i]){
      start=i;
      break;
    }
  }
  if ((start!=0 && ref_plane==0) || (start!=2&&ref_plane==2)) ref_plane=start;

  // Linear regression to find z0, tanl   
  double sumv=0.,sumx=0.;
  double sumy=0.,sumxx=0.,sumxy=0.;
  double sperp=0.,sperp_old=0.,chord,ratio,Delta;
  double z=0,zlast=0;
  //for (unsigned int k=start;k<n-1;k++){
  for (unsigned int k=start;k<n;k++){
    zlast=z;
    sperp_old=sperp;
    if (!bad[k]){
      double diffx=XYZ(k,0)-XYZ(start,0);
      double diffy=XYZ(k,1)-XYZ(start,1);
      chord=sqrt(diffx*diffx+diffy*diffy);
      ratio=chord/2./rc; 
      // Make sure the argument for the arcsin does not go out of range...
      if (ratio>1.) 
	sperp=2.*rc*(M_PI/2.);
      else
	sperp=2.*rc*asin(ratio);
      z=XYZ(k,2);
      // Assume errors in s dominated by errors in R 
      sumv+=1./CR(k,k);
      sumy+=sperp/CR(k,k);
      sumx+=XYZ(k,2)/CR(k,k);
      sumxx+=XYZ(k,2)*XYZ(k,2)/CR(k,k);
      sumxy+=sperp*XYZ(k,2)/CR(k,k);
    }
  }
  Delta=sumv*sumxx-sumx*sumx;
  // Track parameters z0 and tan(lambda)
  tanl=-Delta/(sumv*sumxy-sumy*sumx); 
  //z0=(sumxx*sumy-sumx*sumxy)/Delta*tanl;
  // Error in tanl 
  var_tanl=sumv/Delta*(tanl*tanl*tanl*tanl);
  
  if (tanl<0){ // a negative tanl makes no sense for FDC segments if we 
    // assume that the particle came from the target
    tanl*=-1.;
  }
  sperp-=sperp_old; 
  zvertex=zlast-tanl*sperp;

  return NOERROR;
}

// Use predicted positions (propagating from plane 1 using a helical model) to
// update the R and RPhi covariance matrices.
//
jerror_t DFDCSegment_factory::UpdatePositionsAndCovariance(unsigned int n,
     double r1sq,DMatrix &XYZ,DMatrix &CRPhi,DMatrix &CR){
  double delta_x=XYZ(ref_plane,0)-xc; 
  double delta_y=XYZ(ref_plane,1)-yc;
  double r1=sqrt(r1sq);
  double denom=delta_x*delta_x+delta_y*delta_y;

  // Auxiliary matrices for correcting for non-normal track incidence to FDC
  // The correction is 
  //    CRPhi'= C*CRPhi*C+S*CR*S, where S(i,i)=R_i*kappa/2
  //                                and C(i,i)=sqrt(1-S(i,i)^2)  
  DMatrix C(n,n);
  DMatrix S(n,n);

  // Predicted positions
  Phi1=atan2(delta_y,delta_x);
  double z1=XYZ(ref_plane,2);
  double y1=XYZ(ref_plane,1);
  double x1=XYZ(ref_plane,0);
  double var_R1=CR(ref_plane,ref_plane);
  for (unsigned int k=0;k<n;k++){       
    double sperp=charge*(XYZ(k,2)-z1)/tanl;
    double sinp=sin(Phi1+sperp/rc);
    double cosp=cos(Phi1+sperp/rc);
    XYZ(k,0)=xc+rc*cosp;
    XYZ(k,1)=yc+rc*sinp;
    if (k<n-1) // Exclude target point...
      fdc_track[k].s=(XYZ(k,2)-zvertex)/sin(atan(tanl)); // path length 
 
    // Error analysis.  We ignore errors in N because there doesn't seem to 
    // be any obvious established way to estimate errors in eigenvalues for 
    // small eigenvectors.  We assume that most of the error comes from 
    // the measurement for the reference plane radius and the dip angle anyway.
    double Phi=atan2(XYZ(k,1),XYZ(k,0));
    double sinPhi=sin(Phi);
    double cosPhi=cos(Phi);
    double dRPhi_dx=Phi*cosPhi-sinPhi;
    double dRPhi_dy=Phi*sinPhi+cosPhi;
    
    double dx_dx1=rc*sinp*delta_y/denom;
    double dx_dy1=-rc*sinp*delta_x/denom;
    double dx_dtanl=sinp*sperp/tanl;
    
    double dy_dx1=-rc*cosp*delta_y/denom;
    double dy_dy1=rc*cosp*delta_x/denom;
    double dy_dtanl=-cosp*sperp/tanl;
 
    double dRPhi_dx1=dRPhi_dx*dx_dx1+dRPhi_dy*dy_dx1;
    double dRPhi_dy1=dRPhi_dx*dx_dy1+dRPhi_dy*dy_dy1;
    double dRPhi_dtanl=dRPhi_dx*dx_dtanl+dRPhi_dy*dy_dtanl;

    double dR_dx1=cosPhi*dx_dx1+sinPhi*dy_dx1;
    double dR_dy1=cosPhi*dx_dy1+sinPhi*dy_dy1;
    double dR_dtanl=cosPhi*dx_dtanl+sinPhi*dy_dtanl;
      	
    double cdist=dist_to_origin+r1sq*N[2];
    double n2=N[0]*N[0]+N[1]*N[1];

    double ydenom=y1*n2+N[1]*cdist;
    double dy1_dr1=-r1*(2.*N[1]*N[2]*y1+2.*N[2]*cdist-N[0]*N[0])/ydenom;
    double var_y1=dy1_dr1*dy1_dr1*var_R1;

    double xdenom=x1*n2+N[0]*cdist;
    double dx1_dr1=-r1*(2.*N[0]*N[2]*x1+2.*N[2]*cdist-N[1]*N[1])/xdenom;
    double var_x1=dx1_dr1*dx1_dr1*var_R1;

    CRPhi(k,k)=dRPhi_dx1*dRPhi_dx1*var_x1+dRPhi_dy1*dRPhi_dy1*var_y1
      +dRPhi_dtanl*dRPhi_dtanl*var_tanl;
    CR(k,k)=dR_dx1*dR_dx1*var_x1+dR_dy1*dR_dy1*var_y1
      +dR_dtanl*dR_dtanl*var_tanl;
    
    double rtemp=sqrt(XYZ(k,0)*XYZ(k,0)+XYZ(k,1)*XYZ(k,1)); 
    double stemp=rtemp/4./rc;
    double ctemp=1.-stemp*stemp;
    if (ctemp>0){
      S(k,k)=stemp;
      C(k,k)=sqrt(ctemp);
    }
    else{
      S(k,k)=0.;
      C(k,k)=1.;
    }
  }
  
#if 0  // Disable for now...
   // Correct the covariance matrices for contributions due to multiple
  // scattering
  DMatrix CRPhi_ms(n,n);
  DMatrix CR_ms(n,n);
  double lambda=atan(tanl);
  double cosl=cos(lambda);
  double sinl=sin(lambda);
  if (cosl<EPS) cosl=EPS;
  if (sinl<EPS) sinl=EPS;
  // We loop over all the points except the point at the target, which is left
  // out because the path length is too long to use the linear approximation
  // we are using to estimate the contributions due to multiple scattering.
  for (unsigned int m=0;m<n-1;m++){
    double Rm=sqrt(XYZ(m,0)*XYZ(m,0)+XYZ(m,1)*XYZ(m,1));
    double zm=XYZ(m,2);
    for (unsigned int k=m;k<n-1;k++){
      double Rk=sqrt(XYZ(k,0)*XYZ(k,0)+XYZ(k,1)*XYZ(k,1));
      double zk=XYZ(k,2);
      unsigned int imin=(k+1>m+1)?k+1:m+1;
      for (unsigned int i=imin;i<n-1;i++){
        double sigma2_ms=GetProcessNoise(i,XYZ);
	if (isnan(sigma2_ms)){
	  sigma2_ms=0.;
	}
        double Ri=sqrt(XYZ(i,0)*XYZ(i,0)+XYZ(i,1)*XYZ(i,1));
        double zi=XYZ(i,2);
        CRPhi_ms(m,k)+=sigma2_ms*(Rk-Ri)*(Rm-Ri)/cosl/cosl;
        CR_ms(m,k)+=sigma2_ms*(zk-zi)*(zm-zi)/sinl/sinl/sinl/sinl;
      }
      CRPhi_ms(k,m)=CRPhi_ms(m,k);
      CR_ms(k,m)=CR_ms(m,k);
    }
  }
  CRPhi+=CRPhi_ms;
  CR+=CR_ms;
 #endif

  // Correction for non-normal incidence of track on FDC 
  CRPhi=C*CRPhi*C+S*CR*S;
 
  return NOERROR;
}

// Variance of projected multiple scattering angle for a single layer 
double DFDCSegment_factory::GetProcessNoise(unsigned int i,DMatrix XYZ){
  double cosl=cos(atan(tanl));
  double sinl=sin(atan(tanl));
 
  // Try to prevent division-by-zero errors 
  if (sinl<EPS) sinl=EPS;
  if (cosl<EPS) cosl=EPS;

  // Get Bfield
  double x=XYZ(i,0);
  double y=XYZ(i,1);
  double z=XYZ(i,2);
  double Bx,By,Bz,B;
  bfield->GetField(x,y,z,Bx,By,Bz);
  B=sqrt(Bx*Bx+By*By+Bz*Bz);
   
  // Momentum
  double p=0.003*B*rc/cosl;
  // Assume pion
  double beta=p/sqrt(p*p+0.14*0.14);
 
  //Materials: copper, Kapton, Mylar, Air, Argon, CO2
  double thickness[6]={4e-4,50e-4,13e-4,1.0,0.4,0.6};
  double density[6]={8.96,1.42,1.39,1.2931e-3,1.782e-3,1.977e-3};
  double X0[6]={12.86,40.56,39.95,36.66,19.55,36.2};
  double material_sum=0.;
  for (unsigned int j=0;j<6;j++){
    material_sum+=thickness[j]*density[j]/X0[j];
  }
 
  // Variance from multiple scattering
  return (0.0136*0.0136/p/p/beta/beta*material_sum/sinl
	  *(1.+0.038*log(material_sum/sinl))
	  *(1.+0.038*log(material_sum/sinl)));
}

// Riemann Circle fit:  points on a circle in x,y project onto a plane cutting
// the circular paraboloid surface described by (x,y,x^2+y^2).  Therefore the
// task of fitting points in (x,y) to a circle is transormed to the taks of
// fitting planes in (x,y, w=x^2+y^2) space
//
jerror_t DFDCSegment_factory::RiemannCircleFit(vector<DFDCPseudo *>points,
					       DMatrix CRPhi){
  unsigned int n=points.size()+1;
  DMatrix X(n,3);
  DMatrix Xavg(1,3);
  DMatrix A(3,3);
  double B0,B1,B2,Q,Q1,R,sum,diff;
  double theta,lambda_min;
  // Column and row vectors of ones
  DMatrix Ones(n,1),OnesT(1,n);
  DMatrix W_sum(1,1);
  DMatrix W(n,n);

  // The goal is to find the eigenvector corresponding to the smallest 
  // eigenvalue of the equation
  //            lambda=n^T (X^T W X - W_sum Xavg^T Xavg)n
  // where n is the normal vector to the plane slicing the cylindrical 
  // paraboloid described by the parameterization (x,y,w=x^2+y^2),
  // and W is the weight matrix, assumed for now to be diagonal.
  // In the absence of multiple scattering, W_sum is the sum of all the 
  // diagonal elements in W.

  for (unsigned int i=0;i<n-1;i++){
    X(i,0)=points[i]->x;
    X(i,1)=points[i]->y;
    X(i,2)=X(i,0)*X(i,0)+X(i,1)*X(i,1);
    Ones(i,0)=OnesT(0,i)=1.;
  }
  Ones(n-1,0)=OnesT(0,n-1)=1.;

  // Check that CRPhi is invertible 
  TDecompLU lu(CRPhi);
  if (lu.Decompose()==false){
    *_log << "RiemannCircleFit: Singular matrix,  event "<< myeventno << endMsg;
    
    return UNRECOVERABLE_ERROR; // error placeholder
  }
  W=DMatrix(DMatrix::kInverted,CRPhi);
  W_sum=OnesT*(W*Ones);
  Xavg=(1./W_sum(0,0))*(OnesT*(W*X));
  // Store in private array for use in other routines
  xavg[0]=Xavg(0,0);
  xavg[1]=Xavg(0,1);
  xavg[2]=Xavg(0,2);
  var_avg=1./W_sum(0,0);
  
  A=DMatrix(DMatrix::kTransposed,X)*(W*X)
    -W_sum(0,0)*(DMatrix(DMatrix::kTransposed,Xavg)*Xavg);
  if(!A.IsValid())return UNRECOVERABLE_ERROR;

  // The characteristic equation is 
  //   lambda^3+B2*lambda^2+lambda*B1+B0=0 
  //
  B2=-(A(0,0)+A(1,1)+A(2,2));
  B1=A(0,0)*A(1,1)-A(1,0)*A(0,1)+A(0,0)*A(2,2)-A(2,0)*A(0,2)+A(1,1)*A(2,2)
    -A(2,1)*A(1,2);
  B0=-A.Determinant();
  if(B0==0 || !finite(B0))return UNRECOVERABLE_ERROR;

  // The roots of the cubic equation are given by 
  //        lambda1= -B2/3 + S+T
  //        lambda2= -B2/3 - (S+T)/2 + i sqrt(3)/2. (S-T)
  //        lambda3= -B2/3 - (S+T)/2 - i sqrt(3)/2. (S-T)
  // where we define some temporary variables:
  //        S= (R+sqrt(Q^3+R^2))^(1/3)
  //        T= (R-sqrt(Q^3+R^2))^(1/3)
  //        Q=(3*B1-B2^2)/9
  //        R=(9*B2*B1-27*B0-2*B2^3)/54
  //        sum=S+T;
  //        diff=i*(S-T)
  // We divide Q and R by a safety factor to prevent multiplying together 
  // enormous numbers that cause unreliable results.

  Q=(3.*B1-B2*B2)/9.e4; 
  R=(9.*B2*B1-27.*B0-2.*B2*B2*B2)/54.e6;
  Q1=Q*Q*Q+R*R;
  if (Q1<0) Q1=sqrt(-Q1);
  else{
    return VALUE_OUT_OF_RANGE;
  }

  // DeMoivre's theorem for fractional powers of complex numbers:  
  //      (r*(cos(theta)+i sin(theta)))^(p/q)
  //                  = r^(p/q)*(cos(p*theta/q)+i sin(p*theta/q))
  //
  double temp=100.*pow(R*R+Q1*Q1,0.16666666666666666667);
  theta=atan2(Q1,R)/3.;
  sum=2.*temp*cos(theta);
  diff=-2.*temp*sin(theta);
  // Third root
  lambda_min=-B2/3.-sum/2.+sqrt(3.)/2.*diff;

  // Normal vector to plane
  N[0]=1.;
  N[1]=(A(1,0)*A(0,2)-(A(0,0)-lambda_min)*A(1,2))
    /(A(0,1)*A(2,1)-(A(1,1)-lambda_min)*A(0,2));
  N[2]=(A(2,0)*(A(1,1)-lambda_min)-A(1,0)*A(2,1))
    /(A(1,2)*A(2,1)-(A(2,2)-lambda_min)*(A(1,1)-lambda_min));
  
  // Normalize: n1^2+n2^2+n3^2=1
  sum=0.;
  for (int i=0;i<3;i++){
    sum+=N[i]*N[i];
  }
  for (int i=0;i<3;i++){
    N[i]/=sqrt(sum);
  }
      
  // Distance to origin
  dist_to_origin=-(N[0]*Xavg(0,0)+N[1]*Xavg(0,1)+N[2]*Xavg(0,2));

  // Center and radius of the circle
  xc=-N[0]/2./N[2];
  yc=-N[1]/2./N[2];
  rc=sqrt(1.-N[2]*N[2]-4.*dist_to_origin*N[2])/2./fabs(N[2]);

  return NOERROR;
}

// Riemann Helical Fit based on transforming points on projection to x-y plane 
// to a circular paraboloid surface combined with a linear fit of the arc 
// length versus z. Uses RiemannCircleFit and RiemannLineFit.
//
jerror_t DFDCSegment_factory::RiemannHelicalFit(vector<DFDCPseudo*>points,
						DMatrix &CR,DMatrix &XYZ){
  double Phi;
  unsigned int num_points=points.size()+1;
  DMatrix CRPhi(num_points,num_points); 
 
  // Clear supplemental track info vector
  fdc_track.clear();
  fdc_track_t temp;
  temp.hit_id=0;
  temp.dx=temp.dy=temp.s=temp.chi2=0.;
  fdc_track.assign(num_points-1,temp);
   
  // Fill initial matrices for R and RPhi measurements
  XYZ(num_points-1,2)=Z_TARGET;
  for (unsigned int m=0;m<points.size();m++){
    XYZ(m,2)=points[m]->wire->origin(2);

    Phi=atan2(points[m]->y,points[m]->x);
    CRPhi(m,m)
      =(Phi*cos(Phi)-sin(Phi))*(Phi*cos(Phi)-sin(Phi))*points[m]->covxx
      +(Phi*sin(Phi)+cos(Phi))*(Phi*sin(Phi)+cos(Phi))*points[m]->covyy
      +2.*(Phi*sin(Phi)+cos(Phi))*(Phi*cos(Phi)-sin(Phi))*points[m]->covxy;

    CR(m,m)=cos(Phi)*cos(Phi)*points[m]->covxx
      +sin(Phi)*sin(Phi)*points[m]->covyy
      +2.*sin(Phi)*cos(Phi)*points[m]->covxy;
  }
  CR(points.size(),points.size())=BEAM_VARIANCE;
  CRPhi(points.size(),points.size())=BEAM_VARIANCE;

  // Reference track:
  jerror_t error=NOERROR;  
  // First find the center and radius of the projected circle
  error=RiemannCircleFit(points,CRPhi); 
  if (error!=NOERROR) return error;
  
  // Get reference track estimates for z0 and tanl and intersection points
  // (stored in XYZ)
  error=RiemannLineFit(points,CR,XYZ);
  if (error!=NOERROR) return error;

  // Guess particle charge (+/-1);
  charge=GetCharge(points.size(),XYZ,CR,CRPhi);

  double r1sq=XYZ(ref_plane,0)*XYZ(ref_plane,0)
    +XYZ(ref_plane,1)*XYZ(ref_plane,1);
  UpdatePositionsAndCovariance(num_points,r1sq,XYZ,CRPhi,CR);
  
  // Preliminary circle fit 
  error=RiemannCircleFit(points,CRPhi); 
  if (error!=NOERROR) return error;

  // Preliminary line fit
  error=RiemannLineFit(points,CR,XYZ);
  if (error!=NOERROR) return error;

  // Guess particle charge (+/-1);
  charge=GetCharge(points.size(),XYZ,CR,CRPhi);
  
  r1sq=XYZ(ref_plane,0)*XYZ(ref_plane,0)+XYZ(ref_plane,1)*XYZ(ref_plane,1);
  UpdatePositionsAndCovariance(num_points,r1sq,XYZ,CRPhi,CR);
  
  // Final circle fit 
  error=RiemannCircleFit(points,CRPhi); 
  if (error!=NOERROR) return error;

  // Final line fit
  error=RiemannLineFit(points,CR,XYZ);
  if (error!=NOERROR) return error;

  // Guess particle charge (+/-1)
  charge=GetCharge(points.size(),XYZ,CR,CRPhi);
  
  // Final update to covariance matrices
  ref_plane=0;
  r1sq=XYZ(ref_plane,0)*XYZ(ref_plane,0)+XYZ(ref_plane,1)*XYZ(ref_plane,1);
  UpdatePositionsAndCovariance(num_points,r1sq,XYZ,CRPhi,CR);

  // Store residuals and path length for each measurement
  chisq=0.;
  for (unsigned int m=0;m<points.size();m++){
    fdc_track[m].hit_id=m;
    double sperp=charge*(XYZ(m,2)-XYZ(ref_plane,2))/tanl; 
    double sinp=sin(Phi1+sperp/rc);
    double cosp=cos(Phi1+sperp/rc);
    XYZ(m,0)=xc+rc*cosp;
    XYZ(m,1)=yc+rc*sinp;
    fdc_track[m].dx=XYZ(m,0)-points[m]->x; // residuals
    fdc_track[m].dy=XYZ(m,1)-points[m]->y;
    fdc_track[m].s=(XYZ(m,2)-zvertex)/sin(atan(tanl)); // path length 
    fdc_track[m].chi2=(fdc_track[m].dx*fdc_track[m].dx
                       +fdc_track[m].dy*fdc_track[m].dy)/CR(m,m);
    chisq+=fdc_track[m].chi2;
  }
  return NOERROR;
}


// DFDCSegment_factory::FindSegments
//  Associate nearest neighbors within a package with track segment candidates.
// Provide guess for (seed) track parameters
//
jerror_t DFDCSegment_factory::FindSegments(vector<DFDCPseudo*>points){
   // Put indices for the first point in each plane before the most downstream
  // plane in the vector x_list.
  double old_z=points[0]->wire->origin(2);
  vector<unsigned int>x_list;
  x_list.push_back(0);
  for (unsigned int i=0;i<points.size();i++){
    if (points[i]->wire->origin(2)!=old_z){
      x_list.push_back(i);
    }
    old_z=points[i]->wire->origin(2);
  }
  x_list.push_back(points.size()); 

  unsigned int start=0;
  // loop over the start indices, starting with the first plane
  while (start<x_list.size()-1){
    int used=0;
    // For each iteration, count how many hits we have used already in segments
    for (unsigned int i=0;i<points.size();i++){
      if (points[i]->status&USED_IN_SEGMENT) used++;
    }
    // Break out of loop if we don't have enough hits left to fit a circle
    if (points.size()-used<3) break;

    // Now loop over the list of track segment start points
    for (unsigned int i=x_list[start];i<x_list[start+1];i++){
      if ((points[i]->status&USED_IN_SEGMENT)==0){ 
	points[i]->status|=USED_IN_SEGMENT;   
	chisq=1.e8;

	// Clear track parameters
	kappa=tanl=D=z0=phi0=Phi1=xc=yc=rc=0.;
	charge=1.;
	
	// Point in the current plane in the package 
	double x=points[i]->x;
	double y=points[i]->y; 
	
	// Create list of nearest neighbors
	vector<DFDCPseudo*>neighbors;
	neighbors.push_back(points[i]);
	unsigned int match=0;
	double delta,delta_min=1000.,xtemp,ytemp;
	for (unsigned int k=0;k<x_list.size()-1;k++){
	  delta_min=1000.;
	  match=0;
	  for (unsigned int m=x_list[k];m<x_list[k+1];m++){
	    xtemp=points[m]->x;
	    ytemp=points[m]->y;
	    delta=sqrt((x-xtemp)*(x-xtemp)+(y-ytemp)*(y-ytemp));
	    if (delta<delta_min && delta<MATCH_RADIUS){
	      delta_min=delta;
	      match=m;
	    }
	  }	
	  if (match!=0 && (points[match]->status&USED_IN_SEGMENT)==0 ){
	    x=points[match]->x;
	    y=points[match]->y;
	    points[match]->status|=USED_IN_SEGMENT;
	    neighbors.push_back(points[match]);	  
	  }
	}
	unsigned int num_neighbors=neighbors.size();
	
	// Skip to next segment seed if we don't have enough points to fit a 
	// circle
	if (num_neighbors<3) continue;    

	bool do_sort=false;
	// Look for hits adjacent to the ones we have in our segment candidate
	for (unsigned int k=0;k<points.size();k++){
	  for (unsigned int j=0;j<num_neighbors;j++){
	    xtemp=points[k]->x;
	    ytemp=points[k]->y;
	    x=neighbors[j]->x;
	    y=neighbors[j]->y;
	    delta=sqrt((x-xtemp)*(x-xtemp)+(y-ytemp)*(y-ytemp));
	    if (delta<ADJACENT_MATCH_RADIUS && 
		abs(neighbors[j]->wire->wire-points[k]->wire->wire)==1
		&& neighbors[j]->wire->origin(2)==points[k]->wire->origin(2)){
	      points[k]->status|=USED_IN_SEGMENT;
	      neighbors.push_back(points[k]);
	      do_sort=true;
	    }      
	  }
	}
	// Sort if we added another point
	if (do_sort)
	  std::sort(neighbors.begin(),neighbors.end(),DFDCSegment_package_cmp);
    
	// Matrix of points on track 
	DMatrix XYZ(neighbors.size()+1,3);
	DMatrix CR(neighbors.size()+1,neighbors.size()+1);
   	
	// Arc lengths in helical model are referenced relative to the plane
	// ref_plane within a segment.  For a 6 hit segment, ref_plane=2 is 
	// roughly in the center of the segment.
	ref_plane=2; 
	
	// Perform the Riemann Helical Fit on the track segment
	jerror_t error=RiemannHelicalFit(neighbors,CR,XYZ);   /// initial hit-based fit
		
	if (error==NOERROR){  
	  // guess for curvature
	  kappa=charge/2./rc;  
	  
	  // Estimate for azimuthal angle
	  phi0=atan2(-xc,yc); 
	  if (charge<0) phi0+=M_PI;
	  
	  // Look for distance of closest approach nearest to target
	  D=-charge*rc-xc/sin(phi0);
	  
	  // Creat a new segment
	  DFDCSegment *segment = new DFDCSegment;	
	  DMatrix Seed(5,1);
	  DMatrix Cov(5,5);	  
	  // Initialize seed track parameters
	  Seed(0,0)=kappa;    // Curvature 
	  Seed(1,0)=phi0;      // Phi
	  Seed(2,0)=D;       // D=distance of closest approach to origin   
	  Seed(3,0)=tanl;     // tan(lambda), lambda=dip angle
	  Seed(4,0)=zvertex;       // z-position at closest approach to origin
	  for (unsigned int i=0;i<5;i++) Cov(i,i)=1.;
	  
	  segment->S.ResizeTo(Seed);
	  segment->S=Seed;
	  segment->cov.ResizeTo(Cov);
	  segment->cov=Cov;
	  segment->hits=neighbors;
	  segment->track=fdc_track;
	  segment->xc=xc;
	  segment->yc=yc;
	  segment->rc=rc;
	  segment->Phi1=Phi1;
	  segment->chisq=chisq;
	  
	  _data.push_back(segment);
	}
      }
    }
    // Look for a new plane to start looking for a segment
    while (start<x_list.size()-1){
      if ((points[x_list[start]]->status&USED_IN_SEGMENT)==0) break;
      start++;
    }
  } //while loop over x_list planes

  return NOERROR;
}

// Track position using Riemann Helical fit parameters
jerror_t DFDCSegment_factory::GetHelicalTrackPosition(double z,
                                            const DFDCSegment *segment,
                                                      double &xpos,
                                                      double &ypos){
  double charge=segment->S(0,0)/fabs(segment->S(0,0));
  double sperp=charge*(z-segment->hits[0]->wire->origin(2))/segment->S(3,0);
 
  xpos=segment->xc+segment->rc*cos(segment->Phi1+sperp/segment->rc);
  ypos=segment->yc+segment->rc*sin(segment->Phi1+sperp/segment->rc);
 
  return NOERROR;
}

// Linear regression to find charge
double DFDCSegment_factory::GetCharge(unsigned int n,DMatrix XYZ, DMatrix CR, 
				      DMatrix CRPhi){
  double var=0.; 
  double sumv=0.;
  double sumy=0.;
  double sumx=0.;
  double sumxx=0.,sumxy=0,Delta;
  double slope,r2;
  double phi_old=atan2(XYZ(0,1),XYZ(0,0));
  for (unsigned int k=0;k<n;k++){   
    double tempz=XYZ(k,2);
    double phi_z=atan2(XYZ(k,1),XYZ(k,0));
    // Check for problem regions near +pi and -pi
    if (fabs(phi_z-phi_old)>M_PI){  
      if (phi_old<0) phi_z-=2.*M_PI;
      else phi_z+=2.*M_PI;
    }
    r2=XYZ(k,0)*XYZ(k,0)+XYZ(k,1)*XYZ(k,1);
    var=(CRPhi(k,k)+phi_z*phi_z*CR(k,k))/r2;
    sumv+=1./var;
    sumy+=phi_z/var;
    sumx+=tempz/var;
    sumxx+=tempz*tempz/var;
    sumxy+=phi_z*tempz/var;
    phi_old=phi_z;
  }
  Delta=sumv*sumxx-sumx*sumx;
  slope=(sumv*sumxy-sumy*sumx)/Delta; 
  
  // Guess particle charge (+/-1);
  if (slope<0.) return -1.;
  return 1.;
}

//----------------------------------------------------------------------------
// The following routine is no longer in use: 
// Correct avalanche position along wire and incorporate drift data for 
// coordinate away from the wire using results of preliminary hit-based fit
//
//#define R_START 7.6
//#define Z_TOF 617.4
//#include "HDGEOMETRY/DLorentzMapCalibDB.h
//#define SC_V_EFF 15.
//#define SC_LIGHT_GUIDE     140.
//#define SC_CENTER          38.75   
//#define TOF_BAR_LENGTH      252.0 
//#define TOF_V_EFF 15.
//#define FDC_X_RESOLUTION 0.028
//#define FDC_Y_RESOLUTION 0.02 //cm
/*
jerror_t DFDCSegment_factory::CorrectPoints(vector<DFDCPseudo*>points,
					    DMatrix XYZ){
  // dip angle
  double lambda=atan(tanl);  
  double alpha=M_PI/2.-lambda;
  
  if (alpha == 0. || rc==0.) return VALUE_OUT_OF_RANGE;

  // Get Bfield, needed to guess particle momentum
  double Bx,By,Bz,B;
  double x=XYZ(ref_plane,0);
  double y=XYZ(ref_plane,1);
  double z=XYZ(ref_plane,2);
  
  bfield->GetField(x,y,z,Bx,By,Bz);
  B=sqrt(Bx*Bx+By*By+Bz*Bz);

  // Momentum and beta
  double p=0.002998*B*rc/cos(lambda);
  double beta=p/sqrt(p*p+0.140*0.140);

  // Correct start time for propagation from (0,0)
  double my_start_time=0.;
  if (use_tof){
    //my_start_time=ref_time-(Z_TOF-Z_TARGET)/sin(lambda)/beta/29.98;
    // If we need to use the tof, the angle relative to the beam line is 
    // small enough that sin(lambda) ~ 1.
    my_start_time=ref_time-(Z_TOF-Z_TARGET)/beta/29.98;
    //my_start_time=0;
  }
  else{
    double ratio=R_START/2./rc;
    if (ratio<=1.)
      my_start_time=ref_time
	-2.*rc*asin(R_START/2./rc)*(1./cos(lambda)/beta/29.98);
    else
      my_start_time=ref_time
	-rc*M_PI*(1./cos(lambda)/beta/29.98);
    
  }
  //my_start_time=0.;

  for (unsigned int m=0;m<points.size();m++){
    DFDCPseudo *point=points[m];

    double cosangle=point->wire->udir(1);
    double sinangle=point->wire->udir(0);
    x=XYZ(m,0);
    y=XYZ(m,1);
    z=point->wire->origin(2);
    double delta_x=0,delta_y=0;   
    // Variances based on expected resolution
    double sigx2=FDC_X_RESOLUTION*FDC_X_RESOLUTION;
   
    // Find difference between point on helical path and wire
    double w=x*cosangle-y*sinangle-point->w;
     // .. and use it to determine which sign to use for the drift time data
    double sign=(w>0?1.:-1.);

    // Correct the drift time for the flight path and convert to distance units
    // assuming the particle is a pion
    delta_x=sign*(point->time-fdc_track[m].s/beta/29.98-my_start_time)*55E-4;

    // Variance for position along wire. Includes angle dependence from PHENIX
    // and transverse diffusion
    double sigy2=fdc_y_variance(alpha,delta_x);
    
    // Next find correction to y from table of deflections
    delta_y=lorentz_def->GetLorentzCorrection(x,y,z,alpha,delta_x);

    // Fill x and y elements with corrected values
    point->ds =-delta_y;     
    point->dw =delta_x;
    point->x=(point->w+point->dw)*cosangle+(point->s+point->ds)*sinangle;
    point->y=-(point->w+point->dw)*sinangle+(point->s+point->ds)*cosangle;
    point->covxx=sigx2*cosangle*cosangle+sigy2*sinangle*sinangle;
    point->covyy=sigx2*sinangle*sinangle+sigy2*cosangle*cosangle;
    point->covxy=(sigy2-sigx2)*sinangle*cosangle;
    point->status|=CORRECTED;
  }
  return NOERROR;
}
*/
