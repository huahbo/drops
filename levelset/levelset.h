//**************************************************************************
// File:    levelset.h                                                     *
// Content: levelset equation for two phase flow problems                  *
// Author:  Sven Gross, Joerg Peters, Volker Reichelt, IGPM RWTH Aachen    *
//**************************************************************************

#ifndef DROPS_LEVELSET_H
#define DROPS_LEVELSET_H

#include "num/spmat.h"
#include "num/discretize.h"
#include "num/solver.h"
#include "num/bndData.h"
#include "num/fe.h"

namespace DROPS
{

class LevelsetP2CL
// P2-discretization and solution of the levelset equation for two phase
// flow problems.
{
  public:
    typedef BndDataCL<>    BndDataT;
    typedef P2EvalCL<double, const BndDataT, VecDescCL>       DiscSolCL;
    typedef P2EvalCL<double, const BndDataT, const VecDescCL> const_DiscSolCL;

    IdxDescCL           idx;
    VecDescCL           Phi;
    double              sigma;     // surface tension

  private:
    MultiGridCL&        _MG;
    double              _diff,     // amount of diffusion in reparametrization
                        _curvDiff, // amount of diffusion in curvature calculation
                        _SD,       // streamline diffusion
                        _theta, _dt;  
    MatrixCL            _E, _H, _L;
    BndDataT            _Bnd;
    SSORPcCL            _pc;
    GMResSolverCL<SSORPcCL>  _gm;

    void SetupReparamSystem( MatrixCL&, MatrixCL&, const VectorCL&, VectorCL&) const;
    void SetupSmoothSystem ( MatrixCL&, MatrixCL&)                             const;
    void SmoothPhi( VectorCL& SmPhi, double diff)                              const;
    
  public:
    LevelsetP2CL( MultiGridCL& mg, double sig= 0, double theta= 0.5, double SD= 0, 
                  double diff= 0, Uint iter=1000, double tol=1e-7, double curvDiff= -1)
      : idx( 1, 1), sigma( sig), _MG( mg), _diff(diff), _curvDiff( curvDiff), _SD( SD), 
        _theta( theta), _dt( 0.), _Bnd( BndDataT(mg.GetBnd().GetNumBndSeg()) ), _gm( _pc, 100, iter, tol)
    {}
    
    LevelsetP2CL( MultiGridCL& mg, const BndDataT& bnd, double sig= 0, double theta= 0.5, double SD= 0, 
                  double diff= 0, Uint iter=1000, double tol=1e-7, double curvDiff= -1)
      : idx( 1, 1), sigma( sig), _MG( mg), _diff(diff), _curvDiff( curvDiff), _SD( SD), 
        _theta( theta), _dt( 0.), _Bnd( bnd), _gm( _pc, 100, iter, tol)
    {}
    
    GMResSolverCL<SSORPcCL>& GetSolver() { return _gm; }
    
    const BndDataT& GetBndData() const { return _Bnd; }
    
    void CreateNumbering( Uint level, IdxDescCL* idx, match_fun match= 0)
        { CreateNumb( level, *idx, _MG, _Bnd, match); }
    void DeleteNumbering( IdxDescCL*);

    void Init( scalar_fun_ptr);

    void SetTimeStep( double dt, double theta=-1);
    // call SetupSystem *before* calling SetTimeStep!
    template<class DiscVelSolT>
    void SetupSystem( const DiscVelSolT&);
    void DoStep();
    void Reparam( Uint steps, double dt);
    void ReparamFastMarching( bool ModifyZero= true, bool Periodic= false, bool OnlyZeroLvl= false);
    
    bool   Intersects( const TetraCL&) const;
    double GetVolume( double translation= 0) const;
    double AdjustVolume( double vol, double tol, double surf= 0) const;
    void   AccumulateBndIntegral( VecDescCL& f) const;
    
    const_DiscSolCL GetSolution() const
        { return const_DiscSolCL( &Phi, &_Bnd, &_MG); }
    const_DiscSolCL GetSolution( const VecDescCL& MyPhi) const
        { return const_DiscSolCL( &MyPhi, &_Bnd, &_MG); }
        
    // the following member functions are added to enable an easier implementation
    // of the coupling navstokes-levelset. They should not be called by a common user.
    void ComputeRhs( VectorCL&) const;
    void DoStep    ( const VectorCL&);
};


class InterfacePatchCL
/// computes the planar interface patches, which are the intersection of a child T' of
/// a tetrahedron T and the zero level of I(phi), where I(phi) is the linear interpolation
/// of the level set function phi on T'.
{
  private:
    const double    approxZero_;
    const RefRuleCL RegRef_;
    int             sign_[10], num_sign_[3];  // 0/1/2 = -/0/+
    int             intersec_;
    double          PhiLoc_[10], sqrtDetATA_;
    Point3DCL       PQRS_[4], Coord_[10], B_[3];
    BaryCoordCL     Bary_[4], BaryDoF_[10];
    Point2DCL       ab_;
  
    inline void Solve2x2( const double det, const SMatrixCL<2,2>& A, SVectorCL<2>& x, const SVectorCL<2>& b)
    {
        x[0]= (A(1,1)*b[0]-A(0,1)*b[1])/det;
        x[1]= (A(0,0)*b[1]-A(1,0)*b[0])/det;
    }

  public:
    InterfacePatchCL();

    void Init( TetraCL& t, VecDescCL& ls);

    // Remark: The following functions are only valid, if Init(...) was called before!
    int  GetSign( Uint DoF) const { return sign_[DoF]; } //< returns -1/0/1
    bool ComputeForChild( Uint ch); //< returns true, if a patch exists for this child

    // Remark: The following functions are only valid, if ComputeForChild(...) was called before!
    bool               IsQuadrilateral()     const { return intersec_==4; }
    bool               EqualToFace()         const { return num_sign_[1]>=3; }   //< returns true, if patch is shared by two tetras
    Uint               GetNumPoints()        const { return intersec_; }
    const Point3DCL&   GetPoint( Uint i)     const { return PQRS_[i]; }
    const BaryCoordCL& GetBary ( Uint i)     const { return Bary_[i]; }
    int                GetNumSign( int sign) const { return num_sign_[sign+1]; } //< returns number of patch points with given sign, where sign is in {-1, 0, 1}
    double             GetFuncDet()          const { return sqrtDetATA_; }
    double             GetAreaFrac()         const { return intersec_==4 ? ab_[0]+ab_[1]-1 : 0; }
    const Point3DCL&   GetGradId( Uint i)    const { return B_[i]; }

    void               WriteGeom( std::ostream&) const; //< Geomview output for debugging
};

} // end of namespace DROPS

#include "levelset/levelset.tpp"

#endif

