//**************************************************************************
// File:    params.cpp                                                     *
// Content: read parameters from file                                      *
// Author:  Sven Gross, Joerg Peters, Volker Reichelt, IGPM RWTH Aachen    *
//**************************************************************************

#include "levelset/params.h"

namespace DROPS
{

void ParamMesszelleCL::RegisterParams()
{
    rp_.BeginGroup("Time");
    rp_.RegInt( num_steps,    "NumSteps");
    rp_.RegDouble( dt,        "StepSize");
    rp_.RegDouble( theta,     "Theta");
    rp_.EndGroup();

    rp_.BeginGroup("Stokes");
    rp_.RegInt( inner_iter,   "InnerIter");
    rp_.RegInt( outer_iter,   "OuterIter");
    rp_.RegDouble( inner_tol, "InnerTol");
    rp_.RegDouble( outer_tol, "OuterTol");
    rp_.EndGroup();

    rp_.BeginGroup("Levelset");
    rp_.RegInt( lset_iter,    "Iter");
    rp_.RegDouble( lset_tol,  "Tol");
    rp_.RegDouble( lset_SD,   "SD");
    rp_.EndGroup();

    rp_.BeginGroup("Reparam");
    rp_.RegInt( RepFreq,      "Freq");
    rp_.RegInt( RepSteps,     "NumSteps");
    rp_.RegDouble( RepTau,    "StepSize");
    rp_.RegDouble( RepDiff,   "Diffusion");
    rp_.EndGroup();

    rp_.BeginGroup("Mat");
    rp_.RegDouble( rhoD,      "DensDrop");
    rp_.RegDouble( muD,       "ViscDrop");
    rp_.RegDouble( rhoF,      "DensFluid");
    rp_.RegDouble( muF,       "ViscFluid");
    rp_.RegDouble( sm_eps,    "SmoothZone");
    rp_.RegDouble( sigma,     "SurfTension");
    rp_.EndGroup();
    
    rp_.BeginGroup("Exp");
    rp_.RegDouble( Radius,    "RadDrop");
    rp_.RegCoord( Mitte,      "PosDrop");
    rp_.RegCoord( g,          "Gravity");
    rp_.RegDouble( Anstroem,  "InflowVel");
    rp_.RegDouble( r_inlet,   "RadInlet");
    rp_.RegInt( flow_dir,     "FlowDir");
    rp_.EndGroup();
    
    rp_.RegInt( num_dropref,  "NumDropRef");
    rp_.RegInt( FPsteps,      "CouplingSteps");
    rp_.RegString( EnsCase,   "EnsightCase");
    rp_.RegString( meshfile,  "MeshFile");
}

void ParamMesszelleNsCL::RegisterParams()
{
    rp_.BeginGroup( "NavStokes");
    rp_.RegInt( scheme,       "TimeIntScheme");
    rp_.RegDouble( nonlinear, "AmountNonlinear");
    rp_.EndGroup();
}

} // end of namespace DROPS
    


