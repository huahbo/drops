//**************************************************************************
// File:    coupling.tpp                                                   *
// Content: coupling of levelset and (Navier-)Stokes equations             *
// Author:  Sven Gross, Joerg Peters, Volker Reichelt, IGPM RWTH Aachen    *
//**************************************************************************

#include "num/nssolver.h"

namespace DROPS
{

// ==============================================
//              CouplLevelsetBaseCL
// ==============================================

template <class StokesT>
CouplLevelsetBaseCL<StokesT>::CouplLevelsetBaseCL (StokesT& Stokes, LevelsetP2CL& ls, double theta)
  : _Stokes( Stokes), _LvlSet( ls), _b( &Stokes.b), _old_b( new VelVecDescCL),
    _cplM( new VelVecDescCL), _old_cplM( new VelVecDescCL), _curv( new VelVecDescCL),
    _mat( 0), _theta( theta)
{}

template <class StokesT>
CouplLevelsetBaseCL<StokesT>::~CouplLevelsetBaseCL()
{
    if (_old_b == &_Stokes.b)
        delete _b;
    else
        delete _old_b;
    delete _cplM; delete _old_cplM; delete _curv;
}

// ==============================================
//           LinThetaScheme2PhaseCL
// ==============================================

template <class StokesT, class SolverT>
LinThetaScheme2PhaseCL<StokesT,SolverT>::LinThetaScheme2PhaseCL
    ( StokesT& Stokes, LevelsetP2CL& ls, SolverT& solver, double theta, double nonlinear, bool implicitCurv, 
      bool usematMG, MGDataCL* matMG)
  : _base( Stokes, ls, theta), _solver( solver),
    _cplN( new VelVecDescCL), _old_cplN( new VelVecDescCL),
    _old_curv(new VelVecDescCL), _cplA(new VelVecDescCL), _nonlinear( nonlinear), implCurv_( implicitCurv),
    _usematMG(usematMG), _matMG( usematMG ? matMG : new MGDataCL)
{
    _rhs.resize( Stokes.b.RowIdx->NumUnknowns);
    _ls_rhs.resize( ls.Phi.RowIdx->NumUnknowns);
    _Stokes.SetLevelSet( ls);
    Update();
}

template <class StokesT, class SolverT>
LinThetaScheme2PhaseCL<StokesT,SolverT>::~LinThetaScheme2PhaseCL()
{
    delete _old_curv; delete _cplA;
    delete _cplN; delete _old_cplN;
    if (!_usematMG) delete _matMG;
}

template <class StokesT, class SolverT>
void LinThetaScheme2PhaseCL<StokesT,SolverT>::SolveLsNs()
// solve decoupled level set / Navier-Stokes system
{
    TimerCL time;
    time.Reset();

    // operators are computed for old level set
    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _old_b, _cplA, _cplM, _LvlSet, _Stokes.t);
    _Stokes.SetupPrStiff( &_Stokes.prA, _LvlSet);
    _Stokes.SetupPrMass( &_Stokes.prM, _LvlSet);

    if (!implCurv_) 
        _mat->LinComb( 1./_dt, _Stokes.M.Data, _theta, _Stokes.A.Data);        
    else // semi-implicit treatment of curvature term, cf. Baensch
    {
        MatrixCL mat0;
        mat0.LinComb( 1./_dt, _Stokes.M.Data, _theta, _Stokes.A.Data);        
        // The MatrixBuilderCL's method of determining when to reuse the pattern
        // is not save for matrix LB_
        LB_.Data.clear();
        _Stokes.SetupLB( &LB_, &cplLB_, _LvlSet, _Stokes.t);
        _mat->LinComb( 1., mat0, _dt, LB_.Data);
    }
    //TODO implicite curvature
    if (_usematMG) {
        for(MGDataCL::iterator it= _matMG->begin(); it!=_matMG->end(); ++it) {
            MGLevelDataCL& tmp= *it;
            MatDescCL A, M;
            A.SetIdx( &tmp.Idx, &tmp.Idx);
            M.SetIdx( &tmp.Idx, &tmp.Idx);
            tmp.A.SetIdx( &tmp.Idx, &tmp.Idx);
            std::cerr << "DoFPIter: Create StiffMatrix for "
                    << (&tmp.Idx)->NumUnknowns << " unknowns." << std::endl;
            if(&tmp != &_matMG->back()) {
                _Stokes.SetupMatrices1( &A, &M, _LvlSet, _Stokes.t);
                tmp.A.Data.LinComb( 1./_dt, M.Data, _theta, A.Data);
            }
        }
    }
    time.Stop();
    std::cerr << "Discretizing NavierStokes for old level set took "<<time.GetTime()<<" sec.\n";
    time.Reset();

    // setup system for levelset eq.
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _LvlSet.SetTimeStep( _dt); // does LinComb of system matrix L_
    _ls_rhs= (1./_dt)*(_LvlSet.E*_LvlSet.Phi.Data) - (1.-_theta)*(_LvlSet.H*_LvlSet.Phi.Data);

    time.Stop();
    std::cerr << "Discretizing Levelset took " << time.GetTime() << " sec.\n";
    time.Reset();

    _LvlSet.DoStep( _ls_rhs);

    time.Stop();
    std::cerr << "Solving Levelset took " << time.GetTime() << " sec.\n";

    time.Reset();

    _Stokes.t+= _dt;

    // rhs for new level set
    _curv->Clear();
    _LvlSet.AccumulateBndIntegral( *_curv);
    _Stokes.SetupRhs1( _b, _LvlSet, _Stokes.t);

    _rhs=  (1./_dt)*(_Stokes.M.Data*_Stokes.v.Data) + _theta*_b->Data + _cplA->Data
        + (1.-_theta)*(_old_b->Data - _Stokes.A.Data*_Stokes.v.Data - _nonlinear*(_Stokes.N.Data*_Stokes.v.Data - _old_cplN->Data));

    if (!implCurv_)
        _rhs+= _theta*_curv->Data + (1.-_theta)*_old_curv->Data; 
    else // semi-implicit treatment of curvature term, cf. Baensch
        _rhs+= _old_curv->Data + _dt*cplLB_.Data;

    time.Stop();
    std::cerr << "Discretizing Rhs/Curv took "<<time.GetTime()<<" sec.\n";

    time.Reset();
    _solver.Solve( *_mat, _Stokes.B.Data,
        _Stokes.v, _Stokes.p.Data,
        _rhs, *_cplN, _Stokes.c.Data, /*alpha*/ _theta*_nonlinear);
    time.Stop();
    std::cerr << "Solving NavierStokes: residual: " << _solver.GetResid()
            << "\titerations: " << _solver.GetIter()
            << "\ttime: " << time.GetTime() << "s\n";
}

template <class StokesT, class SolverT>
void LinThetaScheme2PhaseCL<StokesT,SolverT>::CommitStep()
{
    if (_Stokes.UsesXFEM()) { // update XFEM
        _Stokes.UpdateXNumbering( &_Stokes.pr_idx, _LvlSet, /*NumberingChanged*/ false);
        _Stokes.UpdatePressure( &_Stokes.p);
        _Stokes.c.SetIdx( &_Stokes.pr_idx);
        _Stokes.B.SetIdx( &_Stokes.pr_idx, &_Stokes.vel_idx);
        _Stokes.prA.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        _Stokes.prM.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        // The MatrixBuilderCL's method of determining when to reuse the pattern
        // is not save for P1X-elements.
        _Stokes.B.Data.clear();
        _Stokes.prA.Data.clear();
        _Stokes.prM.Data.clear();
        _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
    }
    else
        _Stokes.SetupRhs2( &_Stokes.c, _LvlSet, _Stokes.t);

    std::swap( _curv, _old_curv);
    std::swap( _cplN, _old_cplN);
}

template <class StokesT, class SolverT>
void LinThetaScheme2PhaseCL<StokesT,SolverT>::DoStep( int)
{
    SolveLsNs();
    CommitStep();
}

template <class StokesT, class SolverT>
void LinThetaScheme2PhaseCL<StokesT,SolverT>::Update()
{
    IdxDescCL* const vidx= &_Stokes.vel_idx;
    IdxDescCL* const pidx= &_Stokes.pr_idx;
    TimerCL time;
    time.Reset();
    time.Start();

    std::cerr << "Updating discretization...\n";
    if (implCurv_)
    {
        LB_.Data.clear();
        LB_.SetIdx( vidx, vidx);
        cplLB_.SetIdx( vidx);
    }
    _mat->clear();
    _Stokes.ClearMat();
    _LvlSet.ClearMat();

    if (_Stokes.UsesXFEM()) { // update XFEM
        _Stokes.UpdateXNumbering( &_Stokes.pr_idx, _LvlSet, /*NumberingChanged*/ false);
        _Stokes.UpdatePressure( &_Stokes.p);
        // The MatrixBuilderCL's method of determining when to reuse the pattern
        // is not save for P1X-elements.
        _Stokes.B.Data.clear();
        _Stokes.prA.Data.clear();
        _Stokes.prM.Data.clear();
    }
    // IndexDesc setzen
    _cplA->SetIdx( vidx);    _cplM->SetIdx( vidx);
    _b->SetIdx( vidx);       _old_b->SetIdx( vidx);
    _cplN->SetIdx( vidx);    _old_cplN->SetIdx( vidx);
    _curv->SetIdx( vidx);    _old_curv->SetIdx( vidx);
    _rhs.resize( vidx->NumUnknowns);
    _ls_rhs.resize( _LvlSet.idx.NumUnknowns);
    _Stokes.c.SetIdx( pidx);
    _Stokes.A.SetIdx( vidx, vidx);
    _Stokes.B.SetIdx( pidx, vidx);
    _Stokes.M.SetIdx( vidx, vidx);
    _Stokes.N.SetIdx( vidx, vidx);
    _Stokes.prA.SetIdx( pidx, pidx);
    _Stokes.prM.SetIdx( pidx, pidx);

    // Diskretisierung
    _LvlSet.AccumulateBndIntegral( *_old_curv);
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
    _Stokes.SetupNonlinear( &_Stokes.N, &_Stokes.v, _old_cplN, _LvlSet, _Stokes.t);
    // MG-Vorkonditionierer fuer Geschwindigkeiten; Indizes und Prolongationsmatrizen
    if (_usematMG) {
        _matMG->clear();
        MultiGridCL& mg= _Stokes.GetMG();
        IdxDescCL* c_idx= 0;
        for(Uint lvl= 0; lvl<=mg.GetLastLevel(); ++lvl) {
            _matMG->push_back( MGLevelDataCL());
            MGLevelDataCL& tmp= _matMG->back();
            std::cerr << "    Create indices on Level " << lvl << std::endl;
            tmp.Idx.Set( 3, 3);
            _Stokes.CreateNumberingVel( lvl, &tmp.Idx);
            if(lvl!=0) {
                std::cerr << "    Create Prolongation on Level " << lvl << std::endl;
                SetupP2ProlongationMatrix( mg, tmp.P, c_idx, &tmp.Idx);
//                std::cout << "    Matrix P " << tmp.P.Data << std::endl;
            }
            c_idx= &tmp.Idx;
        }
    }
    else {
        _matMG->clear();
        _matMG->push_back( MGLevelDataCL());
    }
    // _mat is always a pointer to _matMG->back().A.Data for efficiency.
    _mat= &_matMG->back().A.Data;

    time.Stop();
    std::cerr << "Discretizing took " << time.GetTime() << " sec.\n";
}

// ==============================================
//              CouplLevelsetStokesCL
// ==============================================

template <class StokesT, class SolverT>
CouplLevelsetStokesCL<StokesT,SolverT>::CouplLevelsetStokesCL
    ( StokesT& Stokes, LevelsetP2CL& ls, SolverT& solver, double theta)
  : _base(Stokes, ls, theta), _solver(solver),
    _old_curv(new VelVecDescCL)
{
    _mat= new MatrixCL();
    _rhs.resize( Stokes.b.RowIdx->NumUnknowns);
    _ls_rhs.resize( ls.Phi.RowIdx->NumUnknowns);
    _old_b->SetIdx( _b->RowIdx); _cplM->SetIdx( _b->RowIdx); _old_cplM->SetIdx( _b->RowIdx);
    _old_curv->SetIdx( _b->RowIdx); _curv->SetIdx( _b->RowIdx);
    _Stokes.SetupInstatRhs( _old_b, &_Stokes.c, _old_cplM, _Stokes.t, _old_b, _Stokes.t);
    ls.AccumulateBndIntegral( *_old_curv);
}

template <class StokesT, class SolverT>
CouplLevelsetStokesCL<StokesT,SolverT>::~CouplLevelsetStokesCL()
{
    delete _old_curv;
    delete _mat;
}

template <class StokesT, class SolverT>
void CouplLevelsetStokesCL<StokesT,SolverT>::InitStep()
{
// compute all terms that don't change during the following FP iterations

    _LvlSet.ComputeRhs( _ls_rhs);

    _Stokes.t+= _dt;
    _Stokes.SetupInstatRhs( _b, &_Stokes.c, _cplM, _Stokes.t, _b, _Stokes.t);

    _rhs=  _Stokes.A.Data * _Stokes.v.Data;
    _rhs*= (_theta-1.);
    _rhs+= (1./_dt)*(_Stokes.M.Data*_Stokes.v.Data + _cplM->Data - _old_cplM->Data)
         + _theta*_b->Data + (1.-_theta)*(_old_b->Data + _old_curv->Data);
}

template <class StokesT, class SolverT>
void CouplLevelsetStokesCL<StokesT,SolverT>::DoFPIter()
// perform fixed point iteration
// solving the level set equation first is faster than the other way round
{
    TimerCL time;
    time.Reset();
    time.Start();

    // setup system for levelset eq.
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _LvlSet.SetTimeStep( _dt);
    _LvlSet.DoStep( _ls_rhs);

    time.Stop();
    std::cerr << "Solving Levelset took "<<time.GetTime()<<" sec.\n";

    time.Reset();
    time.Start();

    _curv->Clear();
    _LvlSet.AccumulateBndIntegral( *_curv);
//    if (_Stokes.UsesXFEM()) {
//        _Stokes.UpdateXNumbering( &_Stokes.pr_idx, _LvlSet, /*NumberingChanged*/ false);
//        _Stokes.UpdatePressure( &_Stokes.p);
//        _Stokes.c.SetIdx( &_Stokes.pr_idx);
//        _Stokes.B.SetIdx( &_Stokes.pr_idx, &_Stokes.vel_idx);
//        _Stokes.prA.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
//        _Stokes.prM.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
//        _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
//    }
//    _Stokes.SetupPrStiff( &_Stokes.prA, _LvlSet);
//    _Stokes.SetupPrMass( &_Stokes.prM, _LvlSet);

    _solver.Solve( *_mat, _Stokes.B.Data, _Stokes.v.Data, _Stokes.p.Data,
                   VectorCL( _rhs + _theta*_curv->Data), _Stokes.c.Data);

    time.Stop();
    std::cerr << "Solving Stokes took "<<time.GetTime()<<" sec.\n";
}

template <class StokesT, class SolverT>
void CouplLevelsetStokesCL<StokesT,SolverT>::CommitStep()
{
    std::swap( _b, _old_b);
    std::swap( _cplM, _old_cplM);
    std::swap( _curv, _old_curv);
}

template <class StokesT, class SolverT>
void CouplLevelsetStokesCL<StokesT,SolverT>::DoStep( int maxFPiter)
{
    if (maxFPiter==-1)
        maxFPiter= 99;

    InitStep();
    for (int i=0; i<maxFPiter; ++i)
    {
        DoFPIter();
        if (_solver.GetIter()==0) // no change of vel -> no change of Phi
            break;
    }
    CommitStep();
}

// ==============================================
//              CouplLevelsetStokes2PhaseCL
// ==============================================

template <class StokesT, class SolverT>
CouplLevelsetStokes2PhaseCL<StokesT,SolverT>::CouplLevelsetStokes2PhaseCL
    ( StokesT& Stokes, LevelsetP2CL& ls, SolverT& solver,
      double theta, bool withProjection, bool usematMG, MGDataCL* matMG)
: _base(Stokes, ls, theta), _solver(solver), _old_curv(new VelVecDescCL), 
  withProj_( withProjection), _usematMG( usematMG), _matMG( usematMG ? matMG : new MGDataCL)
{
    Update();
}

template <class StokesT, class SolverT>
CouplLevelsetStokes2PhaseCL<StokesT,SolverT>::~CouplLevelsetStokes2PhaseCL()
{
    delete _old_curv;
    if (!_usematMG) delete _matMG;
}

template <class StokesT, class SolverT>
void CouplLevelsetStokes2PhaseCL<StokesT,SolverT>::InitStep()
{
// compute all terms that don't change during the following FP iterations

    _LvlSet.ComputeRhs( _ls_rhs);

    _Stokes.t+= _dt;

    _rhs=  _Stokes.A.Data * _Stokes.v.Data;
    _rhs*= (_theta-1.);
    _rhs+= (1./_dt)*(_Stokes.M.Data*_Stokes.v.Data - _old_cplM->Data)
         + (1.-_theta)*_old_b->Data;
         
    if (withProj_)
        DoProjectionStep();

    _rhs+= (1.-_theta)*_old_curv->Data;
}

template <class StokesT, class SolverT>
void CouplLevelsetStokes2PhaseCL<StokesT,SolverT>::DoProjectionStep()
// perform preceding projection step
{
    std::cerr << "~~~~~~~~~~~~~~~~ Projection step\n";
    TimerCL time;
    // solve Stokes eq.
    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _b, _b, _cplM, _LvlSet, _Stokes.t);

    if (_Stokes.UsesXFEM()) {
        _Stokes.UpdateXNumbering( &_Stokes.pr_idx, _LvlSet, /*NumberingChanged*/ false);
        _Stokes.UpdatePressure( &_Stokes.p);
        _Stokes.c.SetIdx( &_Stokes.pr_idx);
        _Stokes.B.SetIdx( &_Stokes.pr_idx, &_Stokes.vel_idx);
        // The MatrixBuilderCL's method of determining when to reuse the pattern
        // is not save for P1X-elements.
        _Stokes.B.Data.clear();
        _Stokes.prA.Data.clear();
        _Stokes.prM.Data.clear();
        _Stokes.prA.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        _Stokes.prM.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
    }
    _Stokes.SetupPrStiff( &_Stokes.prA, _LvlSet);
    _Stokes.SetupPrMass( &_Stokes.prM, _LvlSet);

    // The MatrixBuilderCL's method of determining when to reuse the pattern
    // is not save for matrix LB_
    LB_.Data.clear();
    _Stokes.SetupLB( &LB_, &cplLB_, _LvlSet, _Stokes.t);
    MatrixCL mat0;
    mat0.LinComb( 1./_dt, _Stokes.M.Data, _theta, _Stokes.A.Data);
    _mat->LinComb( 1., mat0, _dt, LB_.Data);
    mat0.clear();
    if (_usematMG) {
        for(MGDataCL::iterator it= _matMG->begin(); it!=_matMG->end(); ++it) {
            MGLevelDataCL& tmp= *it;
            MatDescCL A, M;
            A.SetIdx( &tmp.Idx, &tmp.Idx);
            M.SetIdx( &tmp.Idx, &tmp.Idx);
            tmp.A.SetIdx( &tmp.Idx, &tmp.Idx);
            std::cerr << "DoProjectionStep: Create (incomplete) StiffMatrix for "
                      << (&tmp.Idx)->NumUnknowns << " unknowns." << std::endl;
            if(&tmp != &_matMG->back()) {
                _Stokes.SetupMatrices1( &A, &M, _LvlSet, _Stokes.t);
                tmp.A.Data.LinComb( 1./_dt, M.Data, _theta, A.Data);
                // TODO: also introduce LB in linear combination. 
                // SetupLB(..) does not work on grids coarser than that for the level set function.
                // By the way: does SetupMatrices1(..) work for these coarser grids?
            }
        }
    }
    time.Stop();
    std::cerr << "Discretizing Stokes/Curv took "<<time.GetTime()<<" sec.\n";
    time.Reset();
    _solver.Solve( *_mat, _Stokes.B.Data, _Stokes.v.Data, _Stokes.p.Data,
                   VectorCL( _rhs + (1./_dt)*_cplM->Data + _theta*_b->Data + _old_curv->Data + _dt*cplLB_.Data), _Stokes.c.Data);
    time.Stop();
    std::cerr << "Solving Stokes: residual: " << _solver.GetResid()
              << "\titerations: " << _solver.GetIter()
              << "\ttime: " << time.GetTime() << "s\n";

    // solve level set eq.
    time.Reset();
    time.Start();
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _LvlSet.SetTimeStep( _dt);
    time.Stop();
    std::cerr << "Discretizing Levelset took "<<time.GetTime()<<" sec.\n";
    time.Reset();

    _LvlSet.DoStep( _ls_rhs);
    time.Stop();
    std::cerr << "Solving Levelset took "<<time.GetTime()<<" sec.\n";
    time.Reset();
}

template <class StokesT, class SolverT>
void CouplLevelsetStokes2PhaseCL<StokesT,SolverT>::DoFPIter()
// perform fixed point iteration
{
    TimerCL time;
    time.Reset();
    time.Start();
    // setup system for levelset eq.
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _LvlSet.SetTimeStep( _dt);
    time.Stop();
    std::cerr << "Discretizing Levelset took "<<time.GetTime()<<" sec.\n";
    time.Reset();

    _LvlSet.DoStep( _ls_rhs);
    time.Stop();
    std::cerr << "Solving Levelset took "<<time.GetTime()<<" sec.\n";
    time.Reset();

    _curv->Clear();
    _LvlSet.AccumulateBndIntegral( *_curv);
    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _b, _b, _cplM, _LvlSet, _Stokes.t);
    if (_Stokes.UsesXFEM()) {
        _Stokes.UpdateXNumbering( &_Stokes.pr_idx, _LvlSet, /*NumberingChanged*/ false);
        _Stokes.UpdatePressure( &_Stokes.p);
        _Stokes.c.SetIdx( &_Stokes.pr_idx);
        _Stokes.B.SetIdx( &_Stokes.pr_idx, &_Stokes.vel_idx);
        // The MatrixBuilderCL's method of determining when to reuse the pattern
        // is not save for P1X-elements.
        _Stokes.B.Data.clear();
        _Stokes.prA.Data.clear();
        _Stokes.prM.Data.clear();
        _Stokes.prA.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        _Stokes.prM.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
    }
    _Stokes.SetupPrStiff( &_Stokes.prA, _LvlSet);
    _Stokes.SetupPrMass( &_Stokes.prM, _LvlSet);

    _mat->LinComb( 1./_dt, _Stokes.M.Data, _theta, _Stokes.A.Data);
    if (_usematMG) {
        for(MGDataCL::iterator it= _matMG->begin(); it!=_matMG->end(); ++it) {
            MGLevelDataCL& tmp= *it;
            MatDescCL A, M;
            A.SetIdx( &tmp.Idx, &tmp.Idx);
            M.SetIdx( &tmp.Idx, &tmp.Idx);
            tmp.A.SetIdx( &tmp.Idx, &tmp.Idx);
            std::cerr << "DoFPIter: Create StiffMatrix for "
                      << (&tmp.Idx)->NumUnknowns << " unknowns." << std::endl;
            if(&tmp != &_matMG->back()) {
                _Stokes.SetupMatrices1( &A, &M, _LvlSet, _Stokes.t);
                tmp.A.Data.LinComb( 1./_dt, M.Data, _theta, A.Data);
            }
        }
    }
    time.Stop();
    std::cerr << "Discretizing Stokes/Curv took "<<time.GetTime()<<" sec.\n";
    time.Reset();
    _solver.Solve( *_mat, _Stokes.B.Data, _Stokes.v.Data, _Stokes.p.Data,
                   VectorCL( _rhs + (1./_dt)*_cplM->Data + _theta*(_curv->Data + _b->Data)), _Stokes.c.Data);
    time.Stop();
    std::cerr << "Solving Stokes: residual: " << _solver.GetResid()
              << "\titerations: " << _solver.GetIter()
              << "\ttime: " << time.GetTime() << "s\n";
}

template <class StokesT, class SolverT>
void CouplLevelsetStokes2PhaseCL<StokesT,SolverT>::CommitStep()
{
    std::swap( _b, _old_b);
    std::swap( _cplM, _old_cplM);
    std::swap( _curv, _old_curv);
}

template <class StokesT, class SolverT>
void CouplLevelsetStokes2PhaseCL<StokesT,SolverT>::DoStep( int maxFPiter)
{
    if (maxFPiter==-1)
        maxFPiter= 99;

    InitStep();
    for (int i=0; i<maxFPiter; ++i)
    {
        std::cerr << "~~~~~~~~~~~~~~~~ FP-Iter " << i+1 << '\n';
        DoFPIter();
        if (_solver.GetIter()==0 && _LvlSet.GetSolver().GetResid()<_LvlSet.GetSolver().GetTol()) // no change of vel -> no change of Phi
        {
            std::cerr << "Convergence after " << i+1 << " fixed point iterations!" << std::endl;
            break;
        }
    }
    CommitStep();
}

template <class StokesT, class SolverT>
void CouplLevelsetStokes2PhaseCL<StokesT,SolverT>::Update()
{
    IdxDescCL* const vidx= &_Stokes.vel_idx;
    IdxDescCL* const pidx= &_Stokes.pr_idx;
    TimerCL time;
    time.Reset();
    time.Start();

    std::cerr << "Updating discretization...\n";
    if (withProj_)
    {
        LB_.Data.clear();
        LB_.SetIdx( vidx, vidx);
        cplLB_.SetIdx( vidx);
    }
    _Stokes.ClearMat();
    _LvlSet.ClearMat();
    // IndexDesc setzen
    _b->SetIdx( vidx);       _old_b->SetIdx( vidx);
    _cplM->SetIdx( vidx);    _old_cplM->SetIdx( vidx);
    _curv->SetIdx( vidx);    _old_curv->SetIdx( vidx);
    _rhs.resize( vidx->NumUnknowns);
    _ls_rhs.resize( _LvlSet.idx.NumUnknowns);
    _Stokes.c.SetIdx( pidx);
    _Stokes.A.SetIdx( vidx, vidx);
    _Stokes.B.SetIdx( pidx, vidx);
    _Stokes.M.SetIdx( vidx, vidx);
    _Stokes.prA.SetIdx( pidx, pidx);
    _Stokes.prM.SetIdx( pidx, pidx);

    // Diskretisierung
    _LvlSet.AccumulateBndIntegral( *_old_curv);
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _old_b, _old_b, _old_cplM, _LvlSet, _Stokes.t);
    _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
    _Stokes.SetupPrStiff( &_Stokes.prA, _LvlSet);
    _Stokes.SetupPrMass( &_Stokes.prM, _LvlSet);

    // MG-Vorkonditionierer fuer Geschwindigkeiten; Indizes und Prolongationsmatrizen
    if (_usematMG) {
        _matMG->clear();
        MultiGridCL& mg= _Stokes.GetMG();
        IdxDescCL* c_idx= 0;
        for(Uint lvl= 0; lvl<=mg.GetLastLevel(); ++lvl) {
            _matMG->push_back( MGLevelDataCL());
            MGLevelDataCL& tmp= _matMG->back();
            std::cerr << "    Create indices on Level " << lvl << std::endl;
            tmp.Idx.Set( 3, 3);
            _Stokes.CreateNumberingVel( lvl, &tmp.Idx);
            if(lvl!=0) {
                std::cerr << "    Create Prolongation on Level " << lvl << std::endl;
                SetupP2ProlongationMatrix( mg, tmp.P, c_idx, &tmp.Idx);
//                std::cout << "    Matrix P " << tmp.P.Data << std::endl;
            }
            c_idx= &tmp.Idx;
        }
    }
    else {
        _matMG->clear();
        _matMG->push_back( MGLevelDataCL());
    }
    // _mat is always a pointer to _matMG->back().A.Data for efficiency.
    _mat= &_matMG->back().A.Data;
    time.Stop();
    std::cerr << "Discretizing took " << time.GetTime() << " sec.\n";
}


// ==============================================
//              CouplLevelsetNavStokes2PhaseCL
// ==============================================

template <class StokesT, class SolverT>
CouplLevelsetNavStokes2PhaseCL<StokesT,SolverT>::CouplLevelsetNavStokes2PhaseCL
    ( StokesT& Stokes, LevelsetP2CL& ls, SolverT& solver, double theta, double nonlinear, bool withProjection, double stab)
  : _base( Stokes, ls, theta), _solver( solver),
    _cplN( new VelVecDescCL), _old_cplN( new VelVecDescCL),
    _old_curv(new VelVecDescCL), _nonlinear( nonlinear), withProj_( withProjection), stab_( stab)
{
    _mat= new MatrixCL();
    _rhs.resize( Stokes.b.RowIdx->NumUnknowns);
    _ls_rhs.resize( ls.Phi.RowIdx->NumUnknowns);
    _Stokes.SetLevelSet( ls);
    Update();
}

template <class StokesT, class SolverT>
CouplLevelsetNavStokes2PhaseCL<StokesT,SolverT>::~CouplLevelsetNavStokes2PhaseCL()
{
    delete _old_curv;
    delete _cplN; delete _old_cplN;
    delete _mat;
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokes2PhaseCL<StokesT,SolverT>::MaybeStabilize (VectorCL& b)
{
    if (stab_ == 0.0) return;

    cplLB_.SetIdx( &_Stokes.vel_idx);
    LB_.SetIdx( &_Stokes.vel_idx, &_Stokes.vel_idx);
    // The MatrixBuilderCL's method of determining when to reuse the pattern
    // is not save for matrix LB_
    LB_.Data.clear();
    _Stokes.SetupLB( &LB_, &cplLB_, _LvlSet, _Stokes.t);

    MatrixCL mat0( *_mat);
    _mat->clear();
    const double s= stab_*_theta*_dt;
    std::cerr << "Stabilizing with: " << s << '\n';
    _mat->LinComb( 1., mat0, s, LB_.Data); 
    b+= s*(LB_.Data*_Stokes.v.Data);
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokes2PhaseCL<StokesT,SolverT>::InitStep()
{
// compute all terms that don't change during the following FP iterations

    _LvlSet.ComputeRhs( _ls_rhs);

    _Stokes.t+= _dt;

    _rhs=  _Stokes.A.Data * _Stokes.v.Data + _nonlinear*(_Stokes.N.Data * _Stokes.v.Data - _old_cplN->Data);
    _rhs*= (_theta-1.);
    _rhs+= (1./_dt)*(_Stokes.M.Data*_Stokes.v.Data - _old_cplM->Data)
         + (1.-_theta)*_old_b->Data;
         
    if (withProj_)
        DoProjectionStep();

    _rhs+= (1.-_theta)*_old_curv->Data;
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokes2PhaseCL<StokesT,SolverT>::DoProjectionStep()
// perform preceding projection step
{
    std::cerr << "~~~~~~~~~~~~~~~~ Projection step\n";

    // solve level set eq.
    TimerCL time;
    time.Reset();
    time.Start();

    _curv->Clear();
    _LvlSet.AccumulateBndIntegral( *_curv);

    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _b, _b, _cplM, _LvlSet, _Stokes.t);
    if (_Stokes.UsesXFEM()) {
        _Stokes.UpdateXNumbering( &_Stokes.pr_idx, _LvlSet, /*NumberingChanged*/ false);
        _Stokes.UpdatePressure( &_Stokes.p);
        _Stokes.c.SetIdx( &_Stokes.pr_idx);
        _Stokes.B.SetIdx( &_Stokes.pr_idx, &_Stokes.vel_idx);
        _Stokes.prA.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        _Stokes.prM.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        // The MatrixBuilderCL's method of determining when to reuse the pattern
        // is not save for P1X-elements.
        _Stokes.B.Data.clear();
        _Stokes.prA.Data.clear();
        _Stokes.prM.Data.clear();
        _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
    }
    _Stokes.SetupPrStiff( &_Stokes.prA, _LvlSet);
    _Stokes.SetupPrMass( &_Stokes.prM, _LvlSet);

    // The MatrixBuilderCL's method of determining when to reuse the pattern
    // is not save for matrix LB_
    LB_.Data.clear();
    _Stokes.SetupLB( &LB_, &cplLB_, _LvlSet, _Stokes.t);
    MatrixCL mat0;
    mat0.LinComb( 1./_dt, _Stokes.M.Data, _theta, _Stokes.A.Data);
    _mat->LinComb( 1., mat0, _dt, LB_.Data);
    mat0.clear();
    VectorCL b2( _rhs + (1./_dt)*_cplM->Data + _theta*_b->Data + _old_curv->Data + _dt*cplLB_.Data);

    time.Stop();
    std::cerr << "Discretizing NavierStokes/Curv took "<<time.GetTime()<<" sec.\n";

    time.Reset();
    _solver.Solve( *_mat, _Stokes.B.Data,
        _Stokes.v, _Stokes.p.Data,
        b2, *_cplN, _Stokes.c.Data, /*alpha*/ _theta*_nonlinear);
    time.Stop();
    std::cerr << "Solving NavierStokes took "<<time.GetTime()<<" sec.\n";

    // solve level set eq.
    time.Reset();
    time.Start();
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _LvlSet.SetTimeStep( _dt);

    time.Stop();
    std::cerr << "Discretizing Levelset took " << time.GetTime() << " sec.\n";
    time.Reset();

    _LvlSet.DoStep( _ls_rhs);

    time.Stop();
    std::cerr << "Solving Levelset took " << time.GetTime() << " sec.\n";
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokes2PhaseCL<StokesT,SolverT>::DoFPIter()
// perform fixed point iteration
{
    TimerCL time;
    time.Reset();
    time.Start();

    // setup system for levelset eq.
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _LvlSet.SetTimeStep( _dt);

    time.Stop();
    std::cerr << "Discretizing Levelset took " << time.GetTime() << " sec.\n";
    time.Reset();

    _LvlSet.DoStep( _ls_rhs);

    time.Stop();
    std::cerr << "Solving Levelset took " << time.GetTime() << " sec.\n";

    time.Reset();
    time.Start();

    _curv->Clear();
    _LvlSet.AccumulateBndIntegral( *_curv);

    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _b, _b, _cplM, _LvlSet, _Stokes.t);
    if (_Stokes.UsesXFEM()) {
        _Stokes.UpdateXNumbering( &_Stokes.pr_idx, _LvlSet, /*NumberingChanged*/ false);
        _Stokes.UpdatePressure( &_Stokes.p);
        _Stokes.c.SetIdx( &_Stokes.pr_idx);
        _Stokes.B.SetIdx( &_Stokes.pr_idx, &_Stokes.vel_idx);
        _Stokes.prA.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        _Stokes.prM.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        // The MatrixBuilderCL's method of determining when to reuse the pattern
        // is not save for P1X-elements.
        _Stokes.B.Data.clear();
        _Stokes.prA.Data.clear();
        _Stokes.prM.Data.clear();
        _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
    }
    _Stokes.SetupPrStiff( &_Stokes.prA, _LvlSet);
    _Stokes.SetupPrMass( &_Stokes.prM, _LvlSet);
    _mat->LinComb( 1./_dt, _Stokes.M.Data, _theta, _Stokes.A.Data);
    VectorCL b2( _rhs + (1./_dt)*_cplM->Data + _theta*(_curv->Data + _b->Data));
    MaybeStabilize( b2);
    time.Stop();
    std::cerr << "Discretizing NavierStokes/Curv took "<<time.GetTime()<<" sec.\n";

    time.Reset();
    _solver.Solve( *_mat, _Stokes.B.Data,
        _Stokes.v, _Stokes.p.Data,
        b2, *_cplN, _Stokes.c.Data, /*alpha*/ _theta*_nonlinear);
    time.Stop();
    std::cerr << "Solving NavierStokes took "<<time.GetTime()<<" sec.\n";
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokes2PhaseCL<StokesT,SolverT>::CommitStep()
{
    std::swap( _b, _old_b);
    std::swap( _cplM, _old_cplM);
    std::swap( _cplN, _old_cplN);
    std::swap( _curv, _old_curv);
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokes2PhaseCL<StokesT,SolverT>::DoStep( int maxFPiter)
{
    if (maxFPiter==-1)
        maxFPiter= 99;

    InitStep();
    for (int i=0; i<maxFPiter; ++i)
    {
        std::cerr << "~~~~~~~~~~~~~~~~ FP-Iter " << i+1 << '\n';
        DoFPIter();
        if (_solver.GetIter()==0 && _LvlSet.GetSolver().GetResid()<_LvlSet.GetSolver().GetTol()) // no change of vel -> no change of Phi
        {
            std::cerr << "Convergence after " << i+1 << " fixed point iterations!" << std::endl;
            break;
        }
    }
    CommitStep();
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokes2PhaseCL<StokesT,SolverT>::Update()
{
    IdxDescCL* const vidx= &_Stokes.vel_idx;
    IdxDescCL* const pidx= &_Stokes.pr_idx;
    TimerCL time;
    time.Reset();
    time.Start();

    std::cerr << "Updating discretization...\n";
    if (withProj_ || stab_!=0.0)
    {
        LB_.Data.clear();
        LB_.SetIdx( vidx, vidx);
        cplLB_.SetIdx( vidx);
    }
    _mat->clear();
    _Stokes.ClearMat();
    _LvlSet.ClearMat();
    // IndexDesc setzen
    _b->SetIdx( vidx);       _old_b->SetIdx( vidx);
    _cplM->SetIdx( vidx);    _old_cplM->SetIdx( vidx);
    _cplN->SetIdx( vidx);    _old_cplN->SetIdx( vidx);
    _curv->SetIdx( vidx);    _old_curv->SetIdx( vidx);
    _rhs.resize( vidx->NumUnknowns);
    _ls_rhs.resize( _LvlSet.idx.NumUnknowns);
    _Stokes.c.SetIdx( pidx);
    _Stokes.A.SetIdx( vidx, vidx);
    _Stokes.B.SetIdx( pidx, vidx);
    _Stokes.M.SetIdx( vidx, vidx);
    _Stokes.N.SetIdx( vidx, vidx);
    _Stokes.prA.SetIdx( pidx, pidx);
    _Stokes.prM.SetIdx( pidx, pidx);

    // Diskretisierung
    _LvlSet.AccumulateBndIntegral( *_old_curv);
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _old_b, _old_b, _old_cplM, _LvlSet, _Stokes.t);
    _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
    _Stokes.SetupNonlinear( &_Stokes.N, &_Stokes.v, _old_cplN, _LvlSet, _Stokes.t);

    // Vorkonditionierer
    _Stokes.SetupPrStiff( &_Stokes.prA, _LvlSet);
    _Stokes.SetupPrMass( &_Stokes.prM, _LvlSet);

    time.Stop();
    std::cerr << "Discretizing took " << time.GetTime() << " sec.\n";
}


// ==============================================
//    CouplLevelsetNavStokesDefectCorr2PhaseCL
// ==============================================

template <class StokesT, class SolverT>
CouplLevelsetNavStokesDefectCorr2PhaseCL<StokesT,SolverT>::CouplLevelsetNavStokesDefectCorr2PhaseCL
    ( StokesT& Stokes, LevelsetP2CL& ls, SolverT& solver, double theta, double nonlinear, double stab)
  : _base( Stokes, ls, theta), _solver( solver),
    _cplN( new VelVecDescCL), _old_cplN( new VelVecDescCL),
    _old_curv(new VelVecDescCL), _nonlinear( nonlinear), stab_( stab)
{
    _mat= new MatrixCL();
    _rhs.resize( Stokes.b.RowIdx->NumUnknowns);
    _ls_rhs.resize( ls.Phi.RowIdx->NumUnknowns);
    _Stokes.SetLevelSet( ls);
    Update();
}

template <class StokesT, class SolverT>
CouplLevelsetNavStokesDefectCorr2PhaseCL<StokesT,SolverT>::~CouplLevelsetNavStokesDefectCorr2PhaseCL()
{
    delete _old_curv;
    delete _cplN; delete _old_cplN;
    delete _mat;
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokesDefectCorr2PhaseCL<StokesT,SolverT>::MaybeStabilize (VectorCL& b)
{
    if (stab_ == 0.0) return;

    VelVecDescCL cplLB_( &_Stokes.vel_idx);
    MatDescCL    LB_( &_Stokes.vel_idx, &_Stokes.vel_idx);
    _Stokes.SetupLB( &LB_, &cplLB_, _LvlSet, _Stokes.t);

    MatrixCL mat0( *_mat);
    _mat->clear();
    const double s= stab_*_theta*_dt;
    std::cerr << "Stabilizing with: " << s << '\n';
    _mat->LinComb( 1., mat0, s, LB_.Data); 
    b+= s*(LB_.Data*_Stokes.v.Data);
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokesDefectCorr2PhaseCL<StokesT,SolverT>::InitStep()
{
// compute all terms that don't change during the following FP iterations

    _LvlSet.ComputeRhs( _ls_rhs);

    _Stokes.t+= _dt;

    _rhs=  _Stokes.A.Data * _Stokes.v.Data + _nonlinear*(_Stokes.N.Data * _Stokes.v.Data - _old_cplN->Data);
    _rhs*= (_theta-1.);
    _rhs+= (1./_dt)*(_Stokes.M.Data*_Stokes.v.Data - _old_cplM->Data)
         + (1.-_theta)*(_old_b->Data + _old_curv->Data);

    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _b, _b, _cplM, _LvlSet, _Stokes.t);
    if (_Stokes.UsesXFEM()) {
        _Stokes.UpdateXNumbering( &_Stokes.pr_idx, _LvlSet, /*NumberingChanged*/ false);
        _Stokes.UpdatePressure( &_Stokes.p);
        _Stokes.c.SetIdx( &_Stokes.pr_idx);
        _Stokes.B.SetIdx( &_Stokes.pr_idx, &_Stokes.vel_idx);
        // The MatrixBuilderCL's method of determining when to reuse the pattern
        // is not save for P1X-elements.
        _Stokes.B.Data.clear();
    }
    _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokesDefectCorr2PhaseCL<StokesT,SolverT>::DoFPIter()
// perform fixed point iteration
{
    TimerCL time;
    time.Reset();

    time.Start();
    // setup system for levelset eq.
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _LvlSet.SetTimeStep( _dt);
    time.Stop();
    std::cerr << "Discretizing Levelset took " << time.GetTime() << " sec.\n";
    time.Reset();

    VectorCL phi_d( _LvlSet.GetL()*_LvlSet.Phi.Data - _ls_rhs);

    time.Start();
    _curv->Clear();
    _LvlSet.AccumulateBndIntegral( *_curv);
    _Stokes.SetupNonlinear( &_Stokes.N, &_Stokes.v, _cplN, _LvlSet, _Stokes.t);

    _mat->LinComb( 1./_dt, _Stokes.M.Data, _theta, _Stokes.A.Data);
    VectorCL b2( _rhs + (1./_dt)*_cplM->Data + _theta*(_curv->Data + _b->Data));
    VectorCL v_d( (*_mat)*_Stokes.v.Data + _theta*((_Stokes.N.Data)*_Stokes.v.Data - _cplN->Data)
                  + transp_mul( _Stokes.B.Data, _Stokes.p.Data) - b2),
             p_d( _Stokes.B.Data*_Stokes.v.Data - _Stokes.c.Data);
    time.Stop();
    std::cerr << "Discretizing NavierStokes/Curv took "<<time.GetTime()<<" sec.";
    std::cerr << "\tresidual norms: v: " << norm( v_d) << "\tp: " << norm( p_d)
              << "\tphi: " << norm( phi_d) << '\n';
    time.Reset();
// TODO: Why cpl_XXX in defect correction? bnd values should be zero!!
    time.Start();
//    VectorCL phi( _LvlSet.Phi.Data);
//    _LvlSet.Phi.Data= 0.;
//    _LvlSet.DoStep( phi_d);
    _LvlSet.DoStep( _ls_rhs);
    time.Stop();
//    _LvlSet.Phi.Data*= -1.0;
//    _LvlSet.Phi.Data+= phi;
    std::cerr << "Solving Levelset took " << time.GetTime() << " sec.\n";
    time.Reset();

    time.Start();
    _curv->Clear();
    _LvlSet.AccumulateBndIntegral( *_curv);

    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _b, _b, _cplM, _LvlSet, _Stokes.t);
    if (_Stokes.UsesXFEM()) {
        _Stokes.UpdateXNumbering( &_Stokes.pr_idx, _LvlSet, /*NumberingChanged*/ false);
        _Stokes.UpdatePressure( &_Stokes.p);
        _Stokes.c.SetIdx( &_Stokes.pr_idx);
        _Stokes.B.SetIdx( &_Stokes.pr_idx, &_Stokes.vel_idx);
        _Stokes.prA.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        _Stokes.prM.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        // The MatrixBuilderCL's method of determining when to reuse the pattern
        // is not save for P1X-elements.
        _Stokes.B.Data.clear();
        _Stokes.prA.Data.clear();
        _Stokes.prM.Data.clear();
    }
    _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
    _Stokes.SetupNonlinear( &_Stokes.N, &_Stokes.v, _cplN, _LvlSet, _Stokes.t);
    _Stokes.SetupPrStiff( &_Stokes.prA, _LvlSet);
    _Stokes.SetupPrMass( &_Stokes.prM, _LvlSet);
    time.Stop();
    std::cerr << "Discretizing NavierStokes/Curv took "<<time.GetTime()<<" sec.\n";
    std::cerr << "\tresidual norms: v: " << norm( v_d) << "\tp: " << norm( p_d)
              << "\tphi: " << norm( phi_d) << '\n';
    time.Reset();

    time.Start();

    _mat->LinComb( 1./_dt, _Stokes.M.Data, _theta, _Stokes.A.Data);
    VelVecDescCL w( _Stokes.v.RowIdx);
    VectorCL q( _Stokes.p.Data.size());
    MaybeStabilize ( v_d);
    _solver.SetBaseVel( &_Stokes.v.Data); // ?
    _solver.Solve( *_mat, _Stokes.B.Data, w, q,
        v_d, *_cplN, p_d, /*alpha*/ _theta*_nonlinear);
    _Stokes.v.Data-= w.Data;
    _Stokes.p.Data-= q;
    time.Stop();
    std::cerr << "Solving NavierStokes took "<<time.GetTime()<<" sec.\n";
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokesDefectCorr2PhaseCL<StokesT,SolverT>::CommitStep()
{
    std::swap( _b, _old_b);
    std::swap( _cplM, _old_cplM);
    std::swap( _cplN, _old_cplN);
    std::swap( _curv, _old_curv);
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokesDefectCorr2PhaseCL<StokesT,SolverT>::DoStep( int maxFPiter)
{
    if (maxFPiter==-1)
        maxFPiter= 99;

    InitStep();
    for (int i=0; i<maxFPiter; ++i)
    {
        DoFPIter();
        if (_solver.GetIter()==0 && _LvlSet.GetSolver().GetResid()<_LvlSet.GetSolver().GetTol()) // no change of vel -> no change of Phi
        {
            std::cerr << "Convergence after " << i+1 << " fixed point iterations!" << std::endl;
            break;
        }
        VectorCL b2( _rhs + (1./_dt)*_cplM->Data + _theta*(_curv->Data + _b->Data));
        VectorCL v_d( 1./_dt*(_Stokes.M.Data*_Stokes.v.Data) + _theta*(_Stokes.A.Data*_Stokes.v.Data )
                      + _theta*((_Stokes.N.Data)*_Stokes.v.Data - _cplN->Data)
                      + transp_mul( _Stokes.B.Data, _Stokes.p.Data) - b2);
        std::cerr << " Impulsdefekt: " << norm( v_d) << '\n';
    }
    CommitStep();
}

template <class StokesT, class SolverT>
void CouplLevelsetNavStokesDefectCorr2PhaseCL<StokesT,SolverT>::Update()
{
    IdxDescCL* const vidx= &_Stokes.vel_idx;
    IdxDescCL* const pidx= &_Stokes.pr_idx;
    TimerCL time;
    time.Reset();
    time.Start();

    std::cerr << "Updating discretization...\n";
    _mat->clear();
    _Stokes.ClearMat();
    _LvlSet.ClearMat();
    // IndexDesc setzen
    _b->SetIdx( vidx);       _old_b->SetIdx( vidx);
    _cplM->SetIdx( vidx);    _old_cplM->SetIdx( vidx);
    _cplN->SetIdx( vidx);    _old_cplN->SetIdx( vidx);
    _curv->SetIdx( vidx);    _old_curv->SetIdx( vidx);
    _rhs.resize( vidx->NumUnknowns);
    _ls_rhs.resize( _LvlSet.idx.NumUnknowns);
    _Stokes.c.SetIdx( pidx);
    _Stokes.A.SetIdx( vidx, vidx);
    _Stokes.B.SetIdx( pidx, vidx);
    _Stokes.M.SetIdx( vidx, vidx);
    _Stokes.N.SetIdx( vidx, vidx);
    _Stokes.prA.SetIdx( pidx, pidx);
    _Stokes.prM.SetIdx( pidx, pidx);

    // Diskretisierung
    _LvlSet.AccumulateBndIntegral( *_old_curv);
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _old_b, _old_b, _old_cplM, _LvlSet, _Stokes.t);
    _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
    _Stokes.SetupNonlinear( &_Stokes.N, &_Stokes.v, _old_cplN, _LvlSet, _Stokes.t);

    // Vorkonditionierer
    _Stokes.SetupPrStiff( &_Stokes.prA, _LvlSet);
    _Stokes.SetupPrMass( &_Stokes.prM, _LvlSet);

    time.Stop();
    std::cerr << "Discretizing took " << time.GetTime() << " sec.\n";
}


// ==============================================
//              CouplLsNsBaenschCL
// ==============================================

template <class StokesT, class SolverT>
CouplLsNsBaenschCL<StokesT,SolverT>::CouplLsNsBaenschCL
    ( StokesT& Stokes, LevelsetP2CL& ls, SolverT& solver, int gm_iter, double gm_tol, double nonlinear)

  : _base( Stokes, ls, /*theta*/ 1.0 - std::sqrt( 2.)/2.), _solver(solver),
    _gm( _pc, 100, gm_iter, gm_tol, false /*test absolute resid*/),
    _cplA( new VelVecDescCL), _old_cplA( new VelVecDescCL),
    _cplN( new VelVecDescCL), _old_cplN( new VelVecDescCL),
    _alpha( (1.0 - 2.0*_theta)/(1.0 - _theta)), _nonlinear( nonlinear)
{
    _mat= new MatrixCL();
    _rhs.resize(Stokes.v.RowIdx->NumUnknowns);
    _ls_rhs.resize(ls.Phi.RowIdx->NumUnknowns);
    std::cerr << "theta = " << _theta << "\talpha = " << _alpha << std::endl;
    Update();
}

template <class StokesT, class SolverT>
CouplLsNsBaenschCL<StokesT,SolverT>::~CouplLsNsBaenschCL()
{
    delete _cplA; delete _old_cplA;
    delete _cplN; delete _old_cplN;
    delete _mat;
}

template <class StokesT, class SolverT>
void CouplLsNsBaenschCL<StokesT,SolverT>::InitStep( bool StokesStep)
{
// compute all terms that don't change during the following FP iterations

    const double frac_dt= StokesStep ? _theta*_dt : (1-2*_theta)*_dt;

    _LvlSet.SetTimeStep( frac_dt, StokesStep ? _alpha : 1-_alpha);
    _LvlSet.ComputeRhs( _ls_rhs);

    _Stokes.t+= frac_dt;

    if (StokesStep)
    {
        _rhs= -(1-_alpha)*(_Stokes.A.Data * _Stokes.v.Data - _old_cplA->Data)
              -_nonlinear*(_Stokes.N.Data * _Stokes.v.Data - _old_cplN->Data);
    }
    else
    {
        _rhs= -_alpha*(_Stokes.A.Data * _Stokes.v.Data - _old_cplA->Data)
              - transp_mul( _Stokes.B.Data, _Stokes.p.Data);
    }
    _rhs+= (1./frac_dt)*(_Stokes.M.Data*_Stokes.v.Data - _old_cplM->Data);

}

template <class StokesT, class SolverT>
void CouplLsNsBaenschCL<StokesT,SolverT>::DoStokesFPIter()
// perform fixed point iteration: Levelset / Stokes
// for fractional steps A and C
{
    TimerCL time;
    time.Reset();
    time.Start();
    const double frac_dt= _theta*_dt;
    // setup system for levelset eq.
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _LvlSet.SetTimeStep( frac_dt, _alpha);
    time.Stop();
    std::cerr << "Discretizing Levelset took "<<time.GetTime()<<" sec.\n";
    time.Reset();
    _LvlSet.DoStep( _ls_rhs);
    time.Stop();
    std::cerr << "Solving Levelset took "<<time.GetTime()<<" sec.\n";
    time.Reset();
    time.Start();
    _curv->Clear();
    _LvlSet.AccumulateBndIntegral( *_curv);
    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _b, _cplA, _cplM, _LvlSet, _Stokes.t);
    _mat->LinComb( 1./frac_dt, _Stokes.M.Data, _alpha, _Stokes.A.Data);
    if (_Stokes.UsesXFEM()) {
        _Stokes.UpdateXNumbering( &_Stokes.pr_idx, _LvlSet, /*NumberingChanged*/ false);
        _Stokes.UpdatePressure( &_Stokes.p);
        _Stokes.c.SetIdx( &_Stokes.pr_idx);
        _Stokes.B.SetIdx( &_Stokes.pr_idx, &_Stokes.vel_idx);
        _Stokes.prA.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        _Stokes.prM.SetIdx( &_Stokes.pr_idx, &_Stokes.pr_idx);
        // The MatrixBuilderCL's method of determining when to reuse the pattern
        // is not save for P1X-elements.
        _Stokes.B.Data.clear();
        _Stokes.prA.Data.clear();
        _Stokes.prM.Data.clear();
        _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
    }
    _Stokes.SetupPrStiff( &_Stokes.prA, _LvlSet);
    _Stokes.SetupPrMass( &_Stokes.prM, _LvlSet);
    time.Stop();
    std::cerr << "Discretizing Stokes/Curv took "<<time.GetTime()<<" sec.\n";
    time.Reset();
    _solver.Solve( *_mat, _Stokes.B.Data, _Stokes.v.Data, _Stokes.p.Data,
                   VectorCL( _rhs + (1./frac_dt)*_cplM->Data + _alpha*_cplA->Data + _curv->Data + _b->Data), _Stokes.c.Data);
    time.Stop();
    std::cerr << "Solving Stokes took "<<time.GetTime()<<" sec.\n";
}

template <class StokesT, class SolverT>
void CouplLsNsBaenschCL<StokesT,SolverT>::DoNonlinearFPIter()
// perform fixed point iteration: Levelset / nonlinear system
// for fractional step B
{
    const double frac_dt= _dt*(1.-2.*_theta);
    TimerCL time;
    time.Reset();
    time.Start();
    // setup system for levelset eq.
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _LvlSet.SetTimeStep( frac_dt, 1-_alpha);
    time.Stop();
    std::cerr << "Discretizing Levelset took "<<time.GetTime()<<" sec.\n";
    time.Reset();
    _LvlSet.DoStep( _ls_rhs);
    time.Stop();
    std::cerr << "Solving Levelset took "<<time.GetTime()<<" sec.\n";
    time.Reset();
    time.Start();
    _curv->Clear();
    _LvlSet.AccumulateBndIntegral( *_curv);
    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _b, _cplA, _cplM, _LvlSet, _Stokes.t);
    time.Stop();
    std::cerr << "Discretizing Stokes/Curv took "<<time.GetTime()<<" sec.\n";
    time.Reset();
    std::cerr << "Starting fixed point iterations for solving nonlinear system...\n";
    _iter_nonlinear= 0;
    do
    {
        _Stokes.SetupNonlinear( &_Stokes.N, &_Stokes.v, _cplN, _LvlSet, _Stokes.t);
        _AN.LinComb( 1-_alpha, _Stokes.A.Data, _nonlinear, _Stokes.N.Data);
        _mat->LinComb( 1./frac_dt, _Stokes.M.Data, 1., _AN);
        _gm.Solve( *_mat, _Stokes.v.Data,
            VectorCL( _rhs + (1./frac_dt)*_cplM->Data + (1-_alpha)*_cplA->Data
            + _nonlinear*_cplN->Data + _curv->Data + _b->Data));
        std::cerr << "fp cycle " << ++_iter_nonlinear << ":\titerations: "
                  << _gm.GetIter() << "\tresidual: " << _gm.GetResid() << std::endl;
    } while (_gm.GetIter() > 0 && _iter_nonlinear<20);
    time.Stop();
    std::cerr << "Solving nonlinear system took "<<time.GetTime()<<" sec.\n";
}

template <class StokesT, class SolverT>
void CouplLsNsBaenschCL<StokesT,SolverT>::CommitStep()
{
    std::swap( _cplM, _old_cplM);
    std::swap( _cplA, _old_cplA);
    std::swap( _cplN, _old_cplN);
}

template <class StokesT, class SolverT>
void CouplLsNsBaenschCL<StokesT,SolverT>::DoStep( int maxFPiter)
{
    if (maxFPiter==-1)
        maxFPiter= 99;

    // ------ frac. step A ------
    InitStep();
    for (int i=0; i<maxFPiter; ++i)
    {
        DoStokesFPIter();
        if (_solver.GetIter()==0 && _LvlSet.GetSolver().GetResid()<_LvlSet.GetSolver().GetTol()) // no change of vel -> no change of Phi
        {
            std::cerr << "===> frac.step A: Convergence after " << i+1 << " fixed point iterations!" << std::endl;
            break;
        }
    }
    CommitStep();

    // ------ frac. step B ------
    InitStep( false);
    for (int i=0; i<maxFPiter; ++i)
    {
        DoNonlinearFPIter();
        if (_iter_nonlinear==1 && _LvlSet.GetSolver().GetResid()<_LvlSet.GetSolver().GetTol()) // no change of vel -> no change of Phi
        {
            std::cerr << "===> frac.step B: Convergence after " << i+1 << " fixed point iterations!" << std::endl;
            break;
        }
    }
    CommitStep();

    // ------ frac. step C ------
    InitStep();
    for (int i=0; i<maxFPiter; ++i)
    {
        DoStokesFPIter();
        if (_solver.GetIter()==0 && _LvlSet.GetSolver().GetResid()<_LvlSet.GetSolver().GetTol()) // no change of vel -> no change of Phi
        {
            std::cerr << "===> frac.step C: Convergence after " << i+1 << " fixed point iterations!" << std::endl;
            break;
        }
    }
    CommitStep();
}

template <class StokesT, class SolverT>
void CouplLsNsBaenschCL<StokesT,SolverT>::Update()
{
    IdxDescCL* const vidx= &_Stokes.vel_idx;
    IdxDescCL* const pidx= &_Stokes.pr_idx;
    TimerCL time;
    time.Reset();
    time.Start();

    std::cerr << "Updating discretization...\n";
    _mat->clear();
    _AN.clear();
    _Stokes.ClearMat();
    _LvlSet.ClearMat();
    // IndexDesc setzen
    _b->SetIdx( vidx);
    _cplM->SetIdx( vidx);    _old_cplM->SetIdx( vidx);
    _cplA->SetIdx( vidx);    _old_cplA->SetIdx( vidx);
    _cplN->SetIdx( vidx);    _old_cplN->SetIdx( vidx);
    _curv->SetIdx( vidx);
    _rhs.resize( vidx->NumUnknowns);
    _ls_rhs.resize( _LvlSet.idx.NumUnknowns);
    _Stokes.c.SetIdx( pidx);
    _Stokes.A.SetIdx( vidx, vidx);
    _Stokes.B.SetIdx( pidx, vidx);
    _Stokes.M.SetIdx( vidx, vidx);
    _Stokes.N.SetIdx( vidx, vidx);
    _Stokes.prA.SetIdx( pidx, pidx);
    _Stokes.prM.SetIdx( pidx, pidx);

    // Diskretisierung
    _LvlSet.SetupSystem( _Stokes.GetVelSolution() );
    _Stokes.SetupSystem1( &_Stokes.A, &_Stokes.M, _b, _old_cplA, _old_cplM, _LvlSet, _Stokes.t);
    _Stokes.SetupSystem2( &_Stokes.B, &_Stokes.c, _LvlSet, _Stokes.t);
    _Stokes.SetupNonlinear( &_Stokes.N, &_Stokes.v, _old_cplN, _LvlSet, _Stokes.t);
    _Stokes.SetupPrStiff( &_Stokes.prA, _LvlSet);
    _Stokes.SetupPrMass( &_Stokes.prM, _LvlSet);

    time.Stop();
    std::cerr << "Discretizing took " << time.GetTime() << " sec.\n";
}

} // end of namespace DROPS
