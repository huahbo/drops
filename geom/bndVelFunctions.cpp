/// \file bndVelFunctions.cpp
/// \brief collections of boundary functions for (Navier-)Stokes problem
/// \author LNM RWTH Aachen: Martin Horsky; SC RWTH Aachen:
/*
 * This file is part of DROPS.
 *
 * DROPS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DROPS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DROPS. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright 2009 LNM/SC RWTH Aachen, Germany
*/
#include "misc/container.h"
#include "levelset/params.h"
#include "misc/bndmap.h"
#include "num/discretize.h"

//========================================================================
//                         General Functions
//========================================================================

///brief returns vector of zero velocities
DROPS::Point3DCL ZeroVel( const DROPS::Point3DCL&, double) { return DROPS::Point3DCL(0.); }

//========================================================================
//                  Functions for twophasedrops.cpp
//========================================================================
/// \name inflow condition
DROPS::SVectorCL<3> InflowBrick( const DROPS::Point3DCL& p, double t)
{
	extern DROPS::ParamMesszelleNsCL C;
    DROPS::SVectorCL<3> ret(0.);
    const double x = p[0]*(2* C.exp_RadInlet -p[0]) / (C.exp_RadInlet*C.exp_RadInlet),
                 z = p[2]*(2*C.exp_RadInlet-p[2]) / (C.exp_RadInlet*C.exp_RadInlet);

    ret[1]= x * z * C.exp_InflowVel * (1-C.exp_InflowAmpl*std::cos(2*M_PI*C.exp_InflowFreq*t));

    return ret;
}


///microchannel (eindhoven)
DROPS::SVectorCL<3> InflowChannel( const DROPS::Point3DCL& p, double t)
{
	extern DROPS::ParamMesszelleNsCL C;
    DROPS::SVectorCL<3> ret(0.);
    const double y = p[1]*(2*25e-6-p[1]) / (25e-6*25e-6),
                 z = p[2]*(2*50e-6-p[2]) / (50e-6*50e-6);

    ret[0]= y * z * C.exp_InflowVel * (1-C.exp_InflowAmpl*std::cos(2*M_PI*C.exp_InflowFreq*t));

    return ret;
}

///mzelle_ns_adap.cpp + mzelle_instat.cpp
DROPS::SVectorCL<3> InflowCell( const DROPS::Point3DCL& p, double)
{
	extern DROPS::ParamMesszelleNsCL C;
    DROPS::SVectorCL<3> ret(0.);
    const double s2= C.exp_RadInlet*C.exp_RadInlet,
                 r2= p.norm_sq() - p[C.exp_FlowDir]*p[C.exp_FlowDir];
    ret[C.exp_FlowDir]= -(r2-s2)/s2*C.exp_InflowVel;

    return ret;
}

//========================================================================
//                       Functions for brick_transp.cpp
//========================================================================

DROPS::SVectorCL<3> InflowBrickTransp (const DROPS::Point3DCL& p, double)
{
	extern DROPS::ParamMesszelleNsCL C;
    DROPS::SVectorCL<3> ret(0.);
    const double x = p[0]*(2*C.exp_RadInlet-p[0]) / (C.exp_RadInlet*C.exp_RadInlet),
                 z = p[2]*(2*C.exp_RadInlet-p[2]) / (C.exp_RadInlet*C.exp_RadInlet);
    ret[1]= x * z * C.exp_InflowVel;
    return ret;
}

//========================================================================
//                        Functions for film.cpp
//========================================================================

DROPS::Point3DCL Inflow( const DROPS::Point3DCL& p, double t)
{
	extern DROPS::ParamFilmCL C;
    DROPS::Point3DCL ret(0.);
    const double d= p[1]/C.exp_Thickness;
    static const double u= C.mat_DensFluid*C.exp_Gravity[0]*C.exp_Thickness*C.exp_Thickness/C.mat_ViscFluid/2;
    ret[0]= d<=1 ? (2*d-d*d)*u * (1 + C.exp_PumpAmpl*std::sin(2*M_PI*t*C.exp_PumpFreq))
                 : (C.mcl_MeshSize[1]-p[1])/(C.mcl_MeshSize[1]-C.exp_Thickness)*u;
    return ret;
}

//========================================================================
//                      Registrierung der Funktionen
//========================================================================

static DROPS::RegisterVelFunction regvelbrick("InflowBrick", InflowBrick);
static DROPS::RegisterVelFunction regvelcell("InflowCell", InflowCell);
static DROPS::RegisterVelFunction regvelchannel("InflowChannel", InflowChannel);
static DROPS::RegisterVelFunction regvelzerovel("ZeroVel", ZeroVel);
static DROPS::RegisterVelFunction regvelbricktransp("InflowBrickTransp", InflowBrickTransp);

