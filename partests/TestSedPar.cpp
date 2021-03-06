/// \file TestSedPar.cpp
/// \brief Test refinements of the raising droplet experiment
/// \author LNM RWTH Aachen: ; SC RWTH Aachen: Oliver Fortmeier

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

#include "geom/multigrid.h"
#include "geom/builder.h"
#include "navstokes/instatnavstokes2phase.h"
#include "stokes/integrTime.h"
#include "out/output.h"
#include "out/ensightOut.h"
#include "out/vtkOut.h"
#include "levelset/coupling.h"
#include "levelset/params.h"
#include "levelset/adaptriang.h"
#include "levelset/mzelle_hdr.h"
#include "levelset/twophaseutils.h"
#include "levelset/surfacetension.h"
#include "poisson/transport2phase.h"
#include "surfactant/ifacetransp.h"
#include "misc/bndmap.h"

#include "num/stokessolverfactory.h"
#ifndef _PAR
#include "num/stokessolver.h"
#else
#include "num/parstokessolver.h"
#include "parallel/loadbal.h"
#include "parallel/parmultigrid.h"
#endif
#include <fstream>
#include <sstream>


DROPS::ParamMesszelleNsCL C;
// rho*du/dt - mu*laplace u + Dp = f + rho*g - okn
//                        -div u = 0
//                             u = u0, t=t0


double InflowLsetCell( const DROPS::Point3DCL& p, double)
{
    return DROPS::EllipsoidCL::DistanceFct(p);
}


typedef DROPS::BndDataCL<> cBndDataCL;
typedef cBndDataCL::bnd_val_fun  c_bnd_val_fun;
const DROPS::BndCondT c_bc[6]= {
    DROPS::OutflowBC, DROPS::OutflowBC, DROPS::OutflowBC,
    DROPS::OutflowBC, DROPS::OutflowBC, DROPS::OutflowBC
};
const c_bnd_val_fun c_bfun[6]= {0, 0, 0, 0, 0, 0};

double Initialcneg (const DROPS::Point3DCL& , double)
{
    return C.trp_IniCNeg;
}

double Initialcpos (const DROPS::Point3DCL& , double)
{
    return C.trp_IniCPos;
}

// Initial data and rhs for surfactant transport
const double a( -13./8.*std::sqrt( 35./M_PI));
double surf_rhs (const DROPS::Point3DCL& p, double)
{
    return a*(3.*p[0]*p[0]*p[1] - p[1]*p[1]*p[1]);
}
double surf_sol (const DROPS::Point3DCL& p, double)
{
    return 1. + std::sin( atan2( p[0] - C.exp_PosDrop[0], p[2] - C.exp_PosDrop[2]));
}

namespace DROPS // for Strategy
{

void Strategy( InstatNavierStokes2PhaseP2P1CL& Stokes, const LsetBndDataCL& lsetbnd, AdapTriangCL& adap)
// flow control
{
    typedef InstatNavierStokes2PhaseP2P1CL StokesProblemT;

    MultiGridCL& MG= Stokes.GetMG();
    sigma= Stokes.GetCoeff().SurfTens;
    eps= C.sft_JumpWidth;    lambda= C.sft_RelPos;    sigma_dirt_fac= C.sft_DirtFactor;
    instat_scalar_fun_ptr sigmap  = 0;
    instat_vector_fun_ptr gsigmap = 0;
    if (C.sft_VarTension)
    {
        sigmap  = &sigma_step;
        gsigmap = &gsigma_step;
    }
    else
    {
        sigmap  = &sigmaf;
        gsigmap = &gsigma;
    }
    SurfaceTensionCL sf( sigmap);
    LevelsetP2CL lset( MG, lsetbnd, sf, C.lvs_SD, C.lvs_CurvDiff);

    LevelsetRepairCL lsetrepair( lset);
    adap.push_back( &lsetrepair);
    VelocityRepairCL velrepair( Stokes);
    adap.push_back( &velrepair);
    PressureRepairCL prrepair( Stokes, lset);
    adap.push_back( &prrepair);

    IdxDescCL* lidx= &lset.idx;
    MLIdxDescCL* vidx= &Stokes.vel_idx;
    MLIdxDescCL* pidx= &Stokes.pr_idx;

    lset.CreateNumbering( MG.GetLastLevel(), lidx);
    lset.Phi.SetIdx( lidx);
    if (C.sft_VarTension)
        lset.SetSurfaceForce( SF_ImprovedLBVar);
    else
        lset.SetSurfaceForce( SF_ImprovedLB);

    if ( StokesSolverFactoryHelperCL<ParamMesszelleNsCL>().VelMGUsed(C))
        Stokes.SetNumVelLvl ( Stokes.GetMG().GetNumLevel());
    if ( StokesSolverFactoryHelperCL<ParamMesszelleNsCL>().PrMGUsed(C))
        Stokes.SetNumPrLvl  ( Stokes.GetMG().GetNumLevel());

    SetInitialLevelsetConditions( lset, MG, C);
    Stokes.CreateNumberingVel( MG.GetLastLevel(), vidx);
    Stokes.CreateNumberingPr(  MG.GetLastLevel(), pidx, 0, &lset);
    // For a two-level MG-solver: P2P1 -- P2P1X; comment out the preceding CreateNumberings
//     Stokes.SetNumVelLvl ( 2);
//     Stokes.SetNumPrLvl  ( 2);
//     Stokes.vel_idx.GetCoarsest().CreateNumbering( MG.GetLastLevel(), MG, Stokes.GetBndData().Vel);
//     Stokes.vel_idx.GetFinest().  CreateNumbering( MG.GetLastLevel(), MG, Stokes.GetBndData().Vel);
//     Stokes.pr_idx.GetCoarsest(). GetXidx().SetBound( 1e99);
//     Stokes.pr_idx.GetCoarsest(). CreateNumbering( MG.GetLastLevel(), MG, Stokes.GetBndData().Pr, 0, &lset.Phi);
//     Stokes.pr_idx.GetFinest().   CreateNumbering( MG.GetLastLevel(), MG, Stokes.GetBndData().Pr, 0, &lset.Phi);

    Stokes.SetIdx();
    Stokes.v.SetIdx  ( vidx);
    Stokes.p.SetIdx  ( pidx);
    Stokes.InitVel( &Stokes.v, DROPS::InVecMap::getInstance().find("ZeroVel")->second);
    SetInitialConditions( Stokes, lset, MG, C);
    DisplayDetailedGeom( MG);
    DisplayUnks(Stokes, lset, MG);

    const double Vol= EllipsoidCL::GetVolume();
    std::cout << "initial volume: " << lset.GetVolume()/Vol << std::endl;
    double dphi= lset.AdjustVolume( Vol, 1e-9);
    std::cout << "initial volume correction is " << dphi << std::endl;
    lset.Phi.Data+= dphi;
    std::cout << "new initial volume: " << lset.GetVolume()/Vol << std::endl;

    cBndDataCL Bnd_c( 6, c_bc, c_bfun);
    double D[2] = {C.trp_DiffPos, C.trp_DiffNeg};
    TransportP1CL massTransp( MG, Bnd_c, Stokes.GetBndData().Vel, C.trp_Theta, D, C.trp_HNeg/C.trp_HPos, &Stokes.v, lset, C.tm_StepSize, C.trp_Iter, C.trp_Tol);
    TransportRepairCL transprepair(massTransp, MG);
    if (C.trp_DoTransp)
    {
        adap.push_back(&transprepair);
        MLIdxDescCL* cidx= &massTransp.idx;
        massTransp.CreateNumbering( MG.GetLastLevel(), cidx);
        massTransp.ct.SetIdx( cidx);
        if (C.dmc_InitialCond != -1)
            massTransp.Init( &Initialcneg, &Initialcpos);
        else
        {
            ReadFEFromFile( massTransp.ct, MG, C.dmc_InitialFile+"concentrationTransf");
        }
        massTransp.Update();
        std::cout << massTransp.c.Data.size() << " concentration unknowns,\n";
    }

    /// \todo rhs beruecksichtigen
    SurfactantcGP1CL surfTransp( MG, Stokes.GetBndData().Vel, C.surf_Theta, C.surf_Visc, &Stokes.v, lset.Phi, lset.GetBndData(), C.tm_StepSize, C.surf_Iter, C.surf_Tol, C.surf_OmitBound);
    InterfaceP1RepairCL surf_repair( MG, lset.Phi, lset.GetBndData(), surfTransp.ic);
    if (C.surf_DoTransp)
    {
        adap.push_back( &surf_repair);
        surfTransp.idx.CreateNumbering( MG.GetLastLevel(), MG, &lset.Phi);
        std::cout << "Surfactant transport: NumUnknowns: " << surfTransp.idx.NumUnknowns() << std::endl;
        surfTransp.ic.SetIdx( &surfTransp.idx);
        surfTransp.Init( &surf_sol);
    }

    // Stokes-Solver
    StokesSolverFactoryCL<StokesProblemT, ParamMesszelleNsCL> stokessolverfactory(Stokes, C);
    StokesSolverBaseCL* stokessolver = stokessolverfactory.CreateStokesSolver();
//     StokesSolverAsPreCL pc (*stokessolver1, 1);
//     GCRSolverCL<StokesSolverAsPreCL> gcr(pc, C.stk_OuterIter, C.stk_OuterIter, C.stk_OuterTol, /*rel*/ false);
//     BlockMatrixSolverCL<GCRSolverCL<StokesSolverAsPreCL> >* stokessolver =
//             new BlockMatrixSolverCL<GCRSolverCL<StokesSolverAsPreCL> > (gcr);

    // Navier-Stokes-Solver
    NSSolverBaseCL<StokesProblemT>* navstokessolver = 0;
    if (C.ns_Nonlinear==0.0)
        navstokessolver = new NSSolverBaseCL<StokesProblemT>(Stokes, *stokessolver);
    else
        navstokessolver = new AdaptFixedPtDefectCorrCL<StokesProblemT>(Stokes, *stokessolver, C.ns_Iter, C.ns_Tol, C.ns_Reduction);

    // Level-Set-Solver
#ifndef _PAR
    SSORPcCL ssorpc;
    GMResSolverCL<SSORPcCL>* gm = new GMResSolverCL<SSORPcCL>( ssorpc, 100, C.lvs_Iter, C.lvs_Tol);
#else
    ParJac0CL jacparpc( *lidx);
    ParPreGMResSolverCL<ParJac0CL>* gm = new ParPreGMResSolverCL<ParJac0CL>
           (/*restart*/100, C.lvs_Iter, C.lvs_Tol, *lidx, jacparpc,/*rel*/true, /*acc*/ true, /*modGS*/false, LeftPreconditioning, /*parmod*/true);
#endif

    LevelsetModifyCL lsetmod( C.rpm_Freq, C.rpm_Method, C.rpm_MaxGrad, C.rpm_MinGrad, C.lvs_VolCorrection, Vol);

    // Time discretisation + coupling
    TimeDisc2PhaseCL* timedisc= CreateTimeDisc(Stokes, lset, navstokessolver, gm, C, lsetmod);
    if (C.tm_NumSteps != 0) timedisc->SetTimeStep( C.tm_StepSize);

    if (C.ns_Nonlinear!=0.0 || C.tm_NumSteps == 0) {
        stokessolverfactory.SetMatrixA( &navstokessolver->GetAN()->GetFinest());
            //for Stokes-MGM
        stokessolverfactory.SetMatrices( &navstokessolver->GetAN()->GetCoarsest(), &Stokes.B.Data.GetCoarsest(),
                                         &Stokes.M.Data.GetCoarsest(), &Stokes.prM.Data.GetCoarsest(), &Stokes.pr_idx.GetCoarsest());
    }
    else {
        stokessolverfactory.SetMatrixA( &timedisc->GetUpperLeftBlock()->GetFinest());
            //for Stokes-MGM
        stokessolverfactory.SetMatrices( &timedisc->GetUpperLeftBlock()->GetCoarsest(), &Stokes.B.Data.GetCoarsest(),
                                         &Stokes.M.Data.GetCoarsest(), &Stokes.prM.Data.GetCoarsest(), &Stokes.pr_idx.GetCoarsest());
    }

    UpdateProlongationCL PVel( Stokes.GetMG(), stokessolverfactory.GetPVel(), &Stokes.vel_idx, &Stokes.vel_idx);
    adap.push_back( &PVel);
    UpdateProlongationCL PPr ( Stokes.GetMG(), stokessolverfactory.GetPPr(), &Stokes.pr_idx, &Stokes.pr_idx);
    adap.push_back( &PPr);
    // For a two-level MG-solver: P2P1 -- P2P1X;
//     MakeP1P1XProlongation ( Stokes.vel_idx.NumUnknowns(), Stokes.pr_idx.NumUnknowns(),
//         Stokes.pr_idx.GetFinest().GetXidx().GetNumUnknownsStdFE(),
//         stokessolverfactory.GetPVel()->GetFinest(), stokessolverfactory.GetPPr()->GetFinest());

    std::ofstream* infofile = 0;
    IF_MASTER {
        infofile = new std::ofstream ((C.ens_EnsCase+".info").c_str());
    }
    IFInfo.Init(infofile);
    IFInfo.WriteHeader();

    if (C.tm_NumSteps == 0)
        SolveStatProblem( Stokes, lset, *navstokessolver);

    // for serialization of geometry and numerical data
    TwoPhaseStoreCL<StokesProblemT> ser(MG, Stokes, lset, C.trp_DoTransp ? &massTransp : 0, C.rst_Outputfile, C.rst_Overwrite);

    // Initialize Ensight6 output
    std::string ensf( C.ens_EnsDir + "/" + C.ens_EnsCase);
    Ensight6OutCL ensight( C.ens_EnsCase + ".case", (C.ens_EnsightOut ? C.tm_NumSteps/C.ens_EnsightOut+1 : 0), C.ens_Binary, C.ens_MasterOut);
    ensight.Register( make_Ensight6Geom      ( MG, MG.GetLastLevel(),   C.ens_GeomName,      ensf + ".geo", true));
    ensight.Register( make_Ensight6Scalar    ( lset.GetSolution(),      "Levelset",      ensf + ".scl", true));
    ensight.Register( make_Ensight6Scalar    ( Stokes.GetPrSolution(),  "Pressure",      ensf + ".pr",  true));
    ensight.Register( make_Ensight6Vector    ( Stokes.GetVelSolution(), "Velocity",      ensf + ".vel", true));
    ensight.Register( make_Ensight6Scalar    ( ScalarFunAsP2EvalCL( sigmap, 0., &MG, MG.GetLastLevel()),
                                                                        "Surfaceforce",  ensf + ".sf",  true));
    if (C.trp_DoTransp) {
        ensight.Register( make_Ensight6Scalar( massTransp.GetSolution(),"Concentration", ensf + ".c",   true));
        ensight.Register( make_Ensight6Scalar( massTransp.GetSolution( massTransp.ct),
                                                                        "TransConc",     ensf + ".ct",  true));
    }
    if (C.surf_DoTransp) {
        ensight.Register( make_Ensight6IfaceScalar( MG, surfTransp.ic,  "InterfaceSol",  ensf + ".sur", true));
    }

#ifndef _PAR
    if (Stokes.UsesXFEM())
        ensight.Register( make_Ensight6P1XScalar( MG, lset.Phi, Stokes.p, "XPressure",   ensf + ".pr", true));
#endif

    // writer for vtk-format
    VTKOutCL vtkwriter(adap.GetMG(), "DROPS data", (C.vtk_VTKOut ? C.tm_NumSteps/C.vtk_VTKOut+1 : 0),
                    std::string(C.vtk_VTKDir + "/" + C.vtk_VTKName), C.vtk_Binary);

    vtkwriter.Register( make_VTKVector( Stokes.GetVelSolution(), "velocity") );
    vtkwriter.Register( make_VTKScalar( Stokes.GetPrSolution(), "pressure") );
    vtkwriter.Register( make_VTKScalar( lset.GetSolution(), "level-set") );

    if (C.trp_DoTransp) {
        vtkwriter.Register( make_VTKScalar( massTransp.GetSolution(), "massTransport") );
    }

    if (C.ens_EnsightOut)
        ensight.Write( Stokes.v.t);
    if (C.vtk_VTKOut)
        vtkwriter.Write(Stokes.v.t);

    for (int step= 1; step<=C.tm_NumSteps; ++step)
    {
        std::cout << "============================================================ step " << step << std::endl;

        IFInfo.Update( lset, Stokes.GetVelSolution());
        IFInfo.Write(Stokes.v.t);
        if (C.surf_DoTransp) surfTransp.InitOld();
        //timedisc->DoStep( C.cpl_Iter);
        Point3DCL newCenter= EllipsoidCL::GetCenter(); newCenter[1]+= C.exp_InflowVel;
        EllipsoidCL::Init( newCenter, EllipsoidCL::GetRadius());
        lset.Init( EllipsoidCL::DistanceFct);

        if (C.trp_DoTransp) massTransp.DoStep( step*C.tm_StepSize);
        if (C.surf_DoTransp) {
            surfTransp.DoStep( step*C.tm_StepSize);
            BndDataCL<> ifbnd( 0);
            std::cout << "surfactant on \\Gamma: " << Integral_Gamma( MG, lset.Phi, lset.GetBndData(), make_P1Eval(  MG, ifbnd, surfTransp.ic)) << '\n';
        }

        // WriteMatrices( Stokes, step);

        // grid modification
        bool doGridMod= C.ref_Freq && step%C.ref_Freq == 0;
        if (doGridMod) {
            adap.UpdateTriang( lset);
            if (adap.WasModified()) {
//                timedisc->Update();
                if (C.trp_DoTransp) massTransp.Update();
                if (C.rst_Serialization)
                    ser.Write();
            }
        }

        if (C.ens_EnsightOut && step%C.ens_EnsightOut==0)
            ensight.Write( Stokes.v.t);
        if (C.vtk_VTKOut && step%C.vtk_VTKOut==0)
            vtkwriter.Write(Stokes.v.t);
    }
    IFInfo.Update( lset, Stokes.GetVelSolution());
    IFInfo.Write(Stokes.v.t);
    std::cout << std::endl;
    delete timedisc;
    delete navstokessolver;
    delete stokessolver;
    if (infofile) delete infofile;
//     delete stokessolver1;
}

} // end of namespace DROPS

int main (int argc, char** argv)
{
#ifdef _PAR
    DROPS::ProcInitCL procinit(&argc, &argv);
#endif
  try
  {
#ifdef _PAR
    DROPS::ParMultiGridInitCL pmginit;
#endif
    std::ifstream param;
    if (argc!=2)
    {
        std::cout << "Using default parameter file: risingdroplet.param\n";
        param.open( "risingdroplet.param");
    }
    else
        param.open( argv[1]);
    if (!param)
    {
        std::cerr << "error while opening parameter file\n";
        return 1;
    }
    param >> C;
    param.close();
    std::cout << C << std::endl;

    DROPS::MultiGridCL* mg= 0;
    DROPS::StokesBndDataCL* bnddata= 0;
    DROPS::LsetBndDataCL* lsetbnddata= 0;

    DROPS::BuildDomain( mg, C.dmc_MeshFile, C.dmc_GeomType, C.rst_Inputfile, C.exp_RadInlet);
    DROPS::BuildBoundaryData( mg, bnddata, C.dmc_BoundaryType, C.dmc_BoundaryFncs);

    // todo: reasonable implementation needed
    std::string lsetbndtype = "98" /*NoBC*/, lsetbndfun = "Zero";
    for( size_t i= 1; i<mg->GetBnd().GetNumBndSeg(); ++i) {
        lsetbndtype += "!98";
        lsetbndfun  += "!Zero";
    }

    DROPS::BuildBoundaryData( mg, lsetbnddata, lsetbndtype, lsetbndfun);

    std::cout << "Generated MG of " << mg->GetLastLevel() << " levels." << std::endl;

    DROPS::EllipsoidCL::Init( C.exp_PosDrop, C.exp_RadDrop);
    std::cout << "Taking Partitioner: " << C.ref_Partitioner << std::endl;
    DROPS::AdapTriangCL adap( *mg, C.ref_Width, C.ref_CoarsestLevel, C.ref_FinestLevel, ((C.rst_Inputfile == "none") ? C.ref_LoadBalStrategy : -C.ref_LoadBalStrategy), C.ref_Partitioner);
    // If we read the Multigrid, it shouldn't be modified;
    // otherwise the pde-solutions from the ensight files might not fit.
    if (C.rst_Inputfile == "none")
        adap.MakeInitialTriang( DROPS::EllipsoidCL::DistanceFct);

    std::cout << DROPS::SanityMGOutCL(*mg) << std::endl;
#ifdef _PAR
    if (DROPS::ProcCL::Check( CheckParMultiGrid( adap.GetPMG())))
        std::cout << "As far as I can tell the ParMultigridCl is sane\n";
#endif
    DROPS::InstatNavierStokes2PhaseP2P1CL prob( *mg, DROPS::TwoPhaseFlowCoeffCL(C), *bnddata, C.stk_XFEMStab<0 ? DROPS::P1_FE : DROPS::P1X_FE, C.stk_XFEMStab);

    Strategy( prob, *lsetbnddata, adap);    // do all the stuff

    delete mg;
    delete lsetbnddata;
    delete bnddata;
    return 0;
  }
  catch (DROPS::DROPSErrCL err) { err.handle(); }
}

