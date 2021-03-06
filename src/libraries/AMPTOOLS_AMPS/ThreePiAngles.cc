
#include <stdlib.h>

#include <cassert>
#include <iostream>
#include <string>
#include <sstream>

#include "TLorentzVector.h"
#include "TLorentzRotation.h"

#include "IUAmpTools/AmpParameter.h"
#include "AMPTOOLS_AMPS/ThreePiAngles.h"
#include "AMPTOOLS_AMPS/clebschGordan.h"
#include "AMPTOOLS_AMPS/wignerD.h"
#include "AMPTOOLS_AMPS/breakupMomentum.h"

ThreePiAngles::ThreePiAngles( const vector< string >& args ) :
UserAmplitude< ThreePiAngles >( args )
{

  assert( args.size() == 11 );
	
	m_polBeam = atoi( args[0].c_str() ); // beam polarization component (X=0, Y=1)
  m_polFrac = AmpParameter( args[1] ); // fraction of polarization 0=0% 1=100%
	m_jX      = atoi( args[2].c_str() ); // total J of produced resonance
  m_parX    = atoi( args[3].c_str() ); // parity of produced resonance
	m_iX      = atoi( args[4].c_str() ); // total isospin of resonance
	m_lX      = atoi( args[5].c_str() ); // l between bachelor and isobar
	m_jI      = atoi( args[6].c_str() ); // total J of isobar
	m_iI      = atoi( args[7].c_str() ); // total isospin of isobar
  m_iZ0     = atoi( args[8].c_str() ); // z component of isospin of final state particle 0
  m_iZ1     = atoi( args[9].c_str() ); // z component of isospin of final state particle 1
  m_iZ2     = atoi( args[10].c_str() );// z component of isospin of final state particle 2

  assert( ( m_polBeam == 0 ) || ( m_polBeam == 1 ) );
  assert( ( m_polFrac >= 0 ) && ( m_polFrac <= 1 ) );
  assert( m_jX >= 0  );
  assert( abs( (double)m_parX ) == 1 );
  assert( abs( (double)m_iX )   <= 1 );
  assert( m_lX <= m_jX );
  assert( m_jI >= 0  );
  assert( abs( (double)m_iI )  <= 1 );
  assert( abs( (double)m_iZ0 ) <= 1 );
  assert( abs( (double)m_iZ1 ) <= 1 );
  assert( abs( (double)m_iZ2 ) <= 1 );
    
  registerParameter( m_polFrac );
  
  // the first two elements are the beam and recoil
  m_iZ.push_back( 0 );
  m_iZ.push_back( 0 );
  m_iZ.push_back( m_iZ0 );
  m_iZ.push_back( m_iZ1 );
  m_iZ.push_back( m_iZ2 );
  
}

complex< GDouble >
ThreePiAngles::calcAmplitude( GDouble** pKin ) const
{

  TLorentzVector beam   ( pKin[0][1], pKin[0][2], pKin[0][3], pKin[0][0] ); 
  TLorentzVector recoil ( pKin[1][1], pKin[1][2], pKin[1][3], pKin[1][0] ); 
  TLorentzVector p1     ( pKin[2][1], pKin[2][2], pKin[2][3], pKin[2][0] ); 
  TLorentzVector p2     ( pKin[3][1], pKin[3][2], pKin[3][3], pKin[3][0] ); 
  TLorentzVector p3     ( pKin[4][1], pKin[4][2], pKin[4][3], pKin[4][0] ); 
  
  TLorentzVector isobar = p1 + p2;
  TLorentzVector resonance = isobar + p3;
  
  // orientation of production plane in lab
  GDouble alpha = recoil.Vect().Phi();
  
  TLorentzRotation resRestBoost( -resonance.BoostVector() );
  
  TLorentzVector beam_res   = resRestBoost * beam;
  TLorentzVector recoil_res = resRestBoost * recoil;
  TLorentzVector p3_res     = resRestBoost * p3;

  TVector3 zRes = -recoil_res.Vect().Unit();
  TVector3 yRes = beam_res.Vect().Cross(zRes).Unit();
  TVector3 xRes = yRes.Cross(zRes);
  
  TVector3 anglesRes( (p3_res.Vect()).Dot(xRes),
                      (p3_res.Vect()).Dot(yRes),
                      (p3_res.Vect()).Dot(zRes) );

  GDouble cosThetaRes = anglesRes.CosTheta();
  GDouble phiRes = anglesRes.Phi();

  TLorentzRotation isoRestBoost( -isobar.BoostVector() );
  TLorentzVector p1_iso = isoRestBoost * p1;
    
  TVector3 anglesIso( (p1_iso.Vect()).Dot(xRes),
                      (p1_iso.Vect()).Dot(yRes),
                      (p1_iso.Vect()).Dot(zRes) );

  GDouble cosThetaIso = anglesIso.CosTheta();
  GDouble phiIso = anglesIso.Phi();
  
  GDouble k = breakupMomentum( resonance.M(), isobar.M(), p3.M() );
  GDouble q = breakupMomentum( isobar.M(), p1.M(), p2.M() );
  
  const vector< int >& perm = getCurrentPermutation();
  
  // get the z components of isospin (charges) for the pions
  int iZ0  = m_iZ[perm[2]];
  int iZ1  = m_iZ[perm[3]];
  int iZ2  = m_iZ[perm[4]];
            
  complex< GDouble > i( 0, 1 );
  complex< GDouble > ans( 0, 0 );
 
  // a prefactor the matrix elements that couple negative helicity
  // photons to the final state
  complex< GDouble > negResHelProd = ( m_polBeam == 0 ? 
     cos( 2 * alpha ) + i * sin( 2 * alpha ) : 
    -cos( 2 * alpha ) - i * sin( 2 * alpha ) );
  negResHelProd *= ( m_jX % 2 == 0 ? -m_parX : m_parX  );
 
  // in general we also need a sum over resonance helicities here
  // however, we assume a production mechanism that only produces
  // resonance helicities +-1
  
  for( int mL = -m_lX; mL <= m_lX; ++mL ){
    
    complex< GDouble > term( 0, 0 );
    
    for( int mI = -m_jI; mI <= m_jI; ++mI ){
              
      term += Y( m_jI, mI, cosThetaIso, phiIso ) *
       ( negResHelProd * clebschGordan( m_jI, m_lX, mI, mL, m_jX, -1 ) +
         clebschGordan( m_jI, m_lX, mI, mL, m_jX, 1 ) );
    }
    
    term *= Y( m_lX, mL, cosThetaRes, phiRes );
    ans += term;
  }

  ans *= ( m_polBeam == 0 ? ( 1 + m_polFrac ) / 4 : ( 1 - m_polFrac ) / 4 );

  ans *= clebschGordan( 1, 1, iZ0, iZ1, m_iI, iZ0 + iZ1 )    *
         clebschGordan( m_iI, 1, iZ0 + iZ1, iZ2, m_iX, iZ0 + iZ1 + iZ2 ) *
         pow( k, m_lX ) * pow( q, m_jI );
    
  return ans;
}

#ifdef GPU_ACCELERATION

void
ThreePiAngles::launchGPUKernel( dim3 dimGrid, dim3 dimBlock, GPU_AMP_PROTO ) const {

  const vector< int >& perm = getCurrentPermutation();
  
  // get the z components of isospin (charges) for the pions
  int iZ0  = m_iZ[perm[2]];
  int iZ1  = m_iZ[perm[3]];
  int iZ2  = m_iZ[perm[4]];

  GPUThreePiAngles_exec( dimGrid, dimBlock, GPU_AMP_ARGS,
                         m_polBeam, m_polFrac, m_jX, m_parX, m_iX, m_lX, 
                         m_jI, m_iI, iZ0, iZ1, iZ2 );
  
  
}

#endif
