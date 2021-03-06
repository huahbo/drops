/// \file solver.h
/// \brief iterative solvers
/// \author LNM RWTH Aachen: Patrick Esser, Sven Gross, Helmut Jarausch, Joerg Peters, Volker Reichelt; SC RWTH Aachen:

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

#ifndef DROPS_SOLVER_H
#define DROPS_SOLVER_H

#include <vector>
#include "misc/container.h"
#include "num/spmat.h"
#include "num/spblockmat.h"

namespace DROPS
{

//*****************************************************************************
//
//  Gauss-Seidel type methods for preconditioning
//
//*****************************************************************************

//=============================================================================
//  Template magic for the selection of the preconditioner
//=============================================================================

// Available methods
enum PreMethGS
{
    P_JAC,     //  0 Jacobi
    P_GS,      //  1 Gauss-Seidel
    P_SGS,     //  2 symmetric Gauss-Seidel
    P_SGS0,    //  3 symmetric Gauss-Seidel with initial vector 0
    P_JOR,     //  4 P_JAC with over-relaxation
    P_SOR,     //  5 P_GS with over-relaxation
    P_SSOR,    //  6 P_SGS with over-relaxation
    P_SSOR0,   //  7 P_SGS0 with over-relaxation
    P_SGS0_D,  //  8 P_SGS0 using SparseMatDiagCL
    P_SSOR0_D, //  9 P_SSOR0 using SparseMatDiagCL
    P_DUMMY,   // 10 identity
    P_GS0,     // 11 Gauss-Seidel with initial vector 0
    P_JAC0     // 12 Jacobi with initial vector 0
};

// Base methods
enum PreBaseGS { PB_JAC, PB_GS, PB_SGS, PB_SGS0, PB_DUMMY, PB_GS0, PB_JAC0 };

// Properties of the methods
template <PreMethGS PM> struct PreTraitsCL
{
    static const PreBaseGS BaseMeth= PreBaseGS(PM<8 ? PM%4
                                     : (PM==10 ? PB_DUMMY : (PM==11 ? PB_GS0 : (PM==12 ? PB_JAC0 : PB_SGS0))) );
    static const bool      HasOmega= (PM>=4 && PM<8) || PM==9 || PM==11 || PM==12;
    static const bool      HasDiag=  PM==8 || PM==9;
};

// Used to make a distinct type from each method
template <PreBaseGS> class PreDummyCL {};


//=============================================================================
//  Implementation of the methods
//=============================================================================

// One step of the Jacobi method with start vector x
template <bool HasOmega, typename Vec>
void
SolveGSstep(const PreDummyCL<PB_JAC>&, const MatrixCL& A, Vec& x, const Vec& b, double omega)
{
    const size_t n= A.num_rows();
    Vec          y(x.size());

    for (size_t i=0, nz=0; i<n; ++i)
    {
        double aii=0, sum= b[i];
        for (const size_t end= A.row_beg(i+1); nz<end; ++nz)
            if (A.col_ind(nz) != i)
                sum-= A.val(nz)*x[A.col_ind(nz)];
            else
                aii= A.val(nz);
        if (HasOmega)
            y[i]= (1.-omega)*x[i]+omega*sum/aii;
        else
            y[i]= sum/aii;
    }

    std::swap(x,y);
}

// One step of the Jacobi method with start vector 0
template <bool HasOmega, typename Vec>
void
SolveGSstep(const PreDummyCL<PB_JAC0>&, const MatrixCL& A, Vec& x, const Vec& b, double omega)
{
    const size_t n= A.num_rows();
    size_t nz;

    for (size_t i= 0; i < n; ++i) {
        nz= A.row_beg( i);
        for (const size_t end= A.row_beg( i+1); A.col_ind( nz) != i && nz < end; ++nz) ; // empty loop
        if (HasOmega)
            x[i]= /*(1.-omega)*x[i]=0  + */ omega*b[i]/A.val( nz);
        else
            x[i]= b[i]/A.val( nz);
    }
}

// One step of the Gauss-Seidel/SOR method with start vector x
template <bool HasOmega, typename Vec>
void
SolveGSstep(const PreDummyCL<PB_GS>&, const MatrixCL& A, Vec& x, const Vec& b, double omega)
{
    const size_t n= A.num_rows();
    double aii, sum;

    for (size_t i=0, nz=0; i<n; ++i) {
        sum= b[i];
        const size_t end= A.row_beg( i+1);
        for (; A.col_ind( nz) != i; ++nz) // This is safe: Without diagonal entry, Gauss-Seidel would explode anyway.
            sum-= A.val( nz)*x[A.col_ind( nz)];
        aii= A.val( nz++);
        for (; nz<end; ++nz)
            sum-= A.val( nz)*x[A.col_ind( nz)];
        if (HasOmega)
            x[i]= (1.-omega)*x[i]+omega*sum/aii;
        else
            x[i]= sum/aii;
    }
}

// One step of the Gauss-Seidel/SOR method with start vector x
template <bool HasOmega, typename Vec>
void
SolveGSstep(const PreDummyCL<PB_GS0>&, const MatrixCL& A, Vec& x, const Vec& b, double omega)
{
    const size_t n= A.num_rows();
    double aii, sum;

    for (size_t i=0, nz=0; i<n; ++i) {
        sum= b[i];
        const size_t end= A.row_beg( i+1);
        for (; A.col_ind( nz) != i; ++nz) // This is safe: Without diagonal entry, Gauss-Seidel would explode anyway.
            sum-= A.val( nz)*x[A.col_ind( nz)];
        aii= A.val( nz);
        nz= end;
        if (HasOmega)
            x[i]= (1.-omega)*x[i]+omega*sum/aii;
        else
            x[i]= sum/aii;
    }
}


// One step of the Symmetric-Gauss-Seidel/SSOR method with start vector x
template <bool HasOmega, typename Vec>
void
SolveGSstep(const PreDummyCL<PB_SGS>&, const MatrixCL& A, Vec& x, const Vec& b, double omega)
{
    const size_t n= A.num_rows();
    double aii, sum;

    for (size_t i=0, nz=0; i<n; ++i) {
        sum= b[i];
        const size_t end= A.row_beg( i+1);
        for (; A.col_ind( nz) != i; ++nz) // This is safe: Without diagonal entry, Gauss-Seidel would explode anyway.
            sum-= A.val( nz)*x[A.col_ind( nz)];
        aii= A.val( nz++);
        for (; nz<end; ++nz)
            sum-= A.val( nz)*x[A.col_ind( nz)];
        if (HasOmega)
            x[i]= (1.-omega)*x[i]+omega*sum/aii;
        else
            x[i]= sum/aii;
    }
    for (size_t i= n, nz= A.row_beg( n); i>0; ) { // This is safe: Without diagonal entry, Gauss-Seidel would explode anyway.
        --i;
        double aii, sum= b[i];
        const size_t beg= A.row_beg( i);
        for (; A.col_ind( --nz) != i; ) {
            sum-= A.val( nz)*x[A.col_ind( nz)];
        }
        aii= A.val( nz);
        for (; nz>beg; ) {
            --nz;
            sum-= A.val( nz)*x[A.col_ind( nz)];
        }
        if (HasOmega)
            x[i]= (1.-omega)*x[i]+omega*sum/aii;
        else
            x[i]= sum/aii;
    }
}


// One step of the Symmetric-Gauss-Seidel/SSOR method with start vector 0
template <bool HasOmega, typename Vec>
void
SolveGSstep(const PreDummyCL<PB_SGS0>&, const MatrixCL& A, Vec& x, const Vec& b, double omega)
{
    const size_t n= A.num_rows();

    for (size_t i=0; i<n; ++i)
    {
        double sum= b[i];
        size_t j= A.row_beg(i);
        for ( ; A.col_ind(j) < i; ++j)
            sum-= A.val(j)*x[A.col_ind(j)];
        if (HasOmega)
            x[i]= omega*sum/A.val(j);
        else
            x[i]= sum/A.val(j);
    }

    for (size_t i=n; i>0; )
    {
        --i;
        double sum= 0;
        size_t j= A.row_beg(i+1)-1;
        for ( ; A.col_ind(j) > i; --j)
            sum-= A.val(j)*x[A.col_ind(j)];
        if (HasOmega)
            x[i]= (2.-omega)*x[i]+omega*sum/A.val(j);
        else
            x[i]+= sum/A.val(j);
    }
}


// One step of the Symmetric-Gauss-Seidel/SSOR method with start vector 0,
// uses SparseMatDiagCL for the location of the diagonal
template <bool HasOmega, typename Vec>
void
SolveGSstep(const PreDummyCL<PB_SGS0>&, const MatrixCL& A, Vec& x, const Vec& b, const SparseMatDiagCL& diag, double omega)
{
    const size_t n= A.num_rows();

    for (size_t i=0; i<n; ++i)
    {
        double sum= b[i];
        for (size_t j= A.row_beg(i); j < diag[i]; ++j)
            sum-= A.val(j)*x[A.col_ind(j)];
        if (HasOmega)
            x[i]= omega*sum/A.val(diag[i]);
        else
            x[i]= sum/A.val(diag[i]);
    }

    for (size_t i=n; i>0; )
    {
        --i;
        double sum= 0;
        for (size_t j= A.row_beg(i+1)-1; j > diag[i]; --j)
            sum-= A.val(j)*x[A.col_ind(j)];
        if (HasOmega)
            x[i]= (2.-omega)*x[i]+omega*sum/A.val(diag[i]);
        else
            x[i]+= sum/A.val(diag[i]);
    }
}

template <bool HasOmega, typename  Vec, PreBaseGS PBT>
void
SolveGSstep(const PreDummyCL<PBT>& pd, const MLMatrixCL& M, Vec& x, const Vec& b, double omega)
{
    SolveGSstep<HasOmega, Vec>( pd, M.GetFinest(), x, b, omega);
}

template <bool HasOmega, typename  Vec, PreBaseGS PBT>
void
SolveGSstep(const PreDummyCL<PBT>& pd, const MLMatrixCL& M, Vec& x, const Vec& b)
{
    SolveGSstep<HasOmega, Vec>( pd, M.GetFinest(), x, b);
}

template <bool HasOmega, typename  Vec, PreBaseGS PBT>
void
SolveGSstep(const PreDummyCL<PBT>& pd, const MLMatrixCL& A, Vec& x, const Vec& b, const SparseMatDiagCL& diag, double omega)
{
    SolveGSstep<HasOmega, Vec>( pd, A.GetFinest(), x, b, diag, omega);
}

template <bool HasOmega, typename  Vec, PreBaseGS PBT>
void
SolveGSstep(const PreDummyCL<PBT>& pd, const MLMatrixCL& A, Vec& x, const Vec& b, const SparseMatDiagCL& diag)
{
    SolveGSstep<HasOmega, Vec>( pd, A.GetFinest(), x, b, diag);
}
//=============================================================================
//  Preconditioner classes
//=============================================================================

// TODO: Init ueberdenken.

// Preconditioners without own matrix
template <PreMethGS PM, bool HasDiag= PreTraitsCL<PM>::HasDiag> class PreGSCL;

// Simple preconditioners
template <PreMethGS PM>
class PreGSCL<PM,false>
{
  private:
    double _omega;

  public:
    PreGSCL (double om= 1.0) : _omega(om) {}

    template <typename Mat, typename Vec>
    void Apply(const Mat& A, Vec& x, const Vec& b) const
    {
        SolveGSstep<PreTraitsCL<PM>::HasOmega,Vec>(PreDummyCL<PreTraitsCL<PM>::BaseMeth>(), A, x, b, _omega);
    }
};


// Preconditioner with SparseMatDiagCL
template <PreMethGS PM>
class PreGSCL<PM,true>
{
  private:
    const SparseMatDiagCL* _diag;
    double                 _omega;

  public:
    PreGSCL (double om= 1.0) : _diag(0), _omega(om) {}
    PreGSCL (const PreGSCL& p) : _diag(p._diag ? new SparseMatDiagCL(*(p._diag)) : 0), _omega(p._omega) {}
    ~PreGSCL() { delete _diag; }

    void Init(const MatrixCL& A)
    {
        delete _diag; _diag=new SparseMatDiagCL(A);
    }

    template <typename Vec>
    void Apply(const MatrixCL& A, Vec& x, const Vec& b) const
    {
        SolveGSstep<PreTraitsCL<PM>::HasOmega,Vec>(PreDummyCL<PreTraitsCL<PM>::BaseMeth>(), A, x, b, *_diag, _omega);
    }
    template <typename Vec>
    void Apply(const MLMatrixCL& A, Vec& x, const Vec& b) const
    {
        SolveGSstep<PreTraitsCL<PM>::HasOmega,Vec>(PreDummyCL<PreTraitsCL<PM>::BaseMeth>(), A.GetFinest(), x, b, *_diag, _omega);
    }
};


// Preconditioners with own matrix
template <PreMethGS PM, bool HasDiag= PreTraitsCL<PM>::HasDiag>
class PreGSOwnMatCL;

// Simple preconditioners
template <PreMethGS PM>
class PreGSOwnMatCL<PM,false>
{
  private:
    const MatrixCL& _M;
    double          _omega;

  public:
    PreGSOwnMatCL (const MatrixCL& M, double om= 1.0) : _M(M), _omega(om) {}

    template <typename Mat, typename Vec>
    void Apply(const Mat&, Vec& x, const Vec& b) const
    {
        SolveGSstep<PreTraitsCL<PM>::HasOmega,Vec>(PreDummyCL<PreTraitsCL<PM>::BaseMeth>(), _M, x, b, _omega);
    }
};


// Preconditioner with SparseMatDiagCL
template <PreMethGS PM>
class PreGSOwnMatCL<PM,true>
{
  private:
    const MatrixCL&        _M;
    const SparseMatDiagCL* _diag;
    double                 _omega;

  public:
    PreGSOwnMatCL (const MatrixCL& M, double om= 1.0) : _M(M), _diag(0), _omega(om) {}
    PreGSOwnMatCL (const PreGSOwnMatCL&); // not defined
    ~PreGSOwnMatCL() { delete _diag; }

    void Init(const MatrixCL& A)
    {
        delete _diag; _diag=new SparseMatDiagCL(A);
    }

    template <typename Mat, typename Vec>
    void Apply(const Mat&, Vec& x, const Vec& b) const
    {
        SolveGSstep<PreTraitsCL<PM>::HasOmega,Vec>(PreDummyCL<PreTraitsCL<PM>::BaseMeth>(), _M, x, b, *_diag, _omega);
    }
};

class MultiSSORPcCL
// do multiple SSOR-steps
{
  private:
    double _omega;
    int   _num;

  public:
    MultiSSORPcCL(double om= 1.0, int num= 1) : _omega(om), _num(num) {}

    template <typename Mat, typename Vec>
    void Apply(const Mat& A, Vec& x, const Vec& b) const
    {
        // one SSOR0-step
        SolveGSstep<PreTraitsCL<P_SSOR0>::HasOmega,Vec>(PreDummyCL<PreTraitsCL<P_SSOR0>::BaseMeth>(), A, x, b, _omega);
        // _num-1 SSOR-steps
        for (int i=1; i<_num; ++i)
            SolveGSstep<PreTraitsCL<P_SSOR>::HasOmega,Vec>(PreDummyCL<PreTraitsCL<P_SSOR>::BaseMeth>(), A, x, b, _omega);
    }
};

class DummyPcCL
{
  public:
    template <typename Mat, typename Vec>
    void Apply(const Mat&, Vec& x, const Vec& b) const
    {
        x = b;
    }
    template <typename Mat, typename Vec>
    void ApplyTranspose(const Mat&, Vec& x, const Vec& b) const
    {
        x = b;
    }
    template <typename Mat, typename Vec>
    Vec transp_mul(const Mat&, const Vec& b) const
    {
        return b;
    }
};

/// \brief Apply a diagonal-matrix given as a vector.
class DiagPcCL
{
  private:
    const VectorCL& D_;

  public:
    DiagPcCL (const VectorCL& D) : D_( D) {}

    template <typename Mat, typename Vec>
    void Apply (const Mat&, Vec& x, const Vec& b) const
    { 
        Assert( D_.size()==b.size(), DROPSErrCL("DiagPcCL: incompatible dimensions"), DebugNumericC);
        x= D_*b;
    }
};


/// \brief (Symmetric) Gauss-Seidel preconditioner (i.e. start-vector 0) for A*A^T (Normal Equations).
class NEGSPcCL
{
  private:
    bool symmetric_;             ///< If true, SGS is performed, else GS.
    mutable const void*  Aaddr_; ///< only used to validate, that the diagonal is for the correct matrix.
    mutable size_t Aversion_;
    mutable VectorCL D_;         ///< diagonal of AA^T
    mutable VectorCL y_;         ///< temp-variable: A^T*x.

    template <typename Mat>
    void Update (const Mat& A) const;

    ///\brief Forward Gauss-Seidel-step with start-vector 0 for A*A^T
    template <typename Mat, typename Vec>
    void ForwardGS(const Mat& A, Vec& x, const Vec& b) const;
    ///\brief Backward Gauss-Seidel-step with start-vector 0 for A*A^T
    template <typename Mat, typename Vec>
    void BackwardGS(const Mat& A, Vec& x, const Vec& b) const;

    ///\brief Inverse of ForwardGS
    template <typename Mat, typename Vec>
    void ForwardMulGS(const Mat& A, Vec& x, const Vec& b) const;
    ///\brief Inverse of BackwardGS
    template <typename Mat, typename Vec>
    void BackwardMulGS(const Mat& A, Vec& x, const Vec& b) const;

  public:
    NEGSPcCL (bool symmetric= true) : symmetric_( symmetric), Aaddr_( 0), Aversion_( 0) {}

    ///@{ Note, that A and not A*A^T is the first argument.
    ///\brief Execute a (symmetric) Gauss-Seidel preconditioning step.
    template <typename Mat, typename Vec>
    void Apply(const Mat& A, Vec& x, const Vec& b) const;
    ///\brief If symmetric == false, this  performs a backward Gauss-Seidel step, else it is identical to Apply.
    template <typename Mat, typename Vec>
    void ApplyTranspose(const Mat& A, Vec& x, const Vec& b) const;

    ///\brief Multiply with the preconditioning matrix -- needed for right preconditioning.
    template <typename Mat, typename Vec>
    Vec mul (const Mat& A, const Vec& b) const;
    ///\brief Multiply with the transpose of the preconditioning matrix -- needed for right preconditioning.
    template <typename Mat, typename Vec>
    Vec transp_mul(const Mat& A, const Vec& b) const;
    ///@}

    ///\brief Apply if A*A^T is given as CompositeMatrixCL
    void Apply(const CompositeMatrixCL& AAT, VectorCL& x, const VectorCL& b) const
    { Apply( *AAT.GetBlock1(), x, b); }
};

template <typename Mat>
void NEGSPcCL::Update (const Mat& A) const
{
    if (&A == Aaddr_ && Aversion_ == A.Version()) return;
    Aaddr_= &A;
    Aversion_= A.Version();

    D_.resize( A.num_rows());
    D_= BBTDiag( A);
    y_.resize( A.num_cols());
}

template <typename Mat, typename Vec>
void NEGSPcCL::ForwardGS(const Mat& A, Vec& x, const Vec& b) const
{
    // x= 0.; // implied, but superfluous, because we can assign the x-values below, not update.
    y_= 0.;
    double t;
    for (size_t i= 0; i < A.num_rows(); ++i) {
        t= (b[i] - mul_row( A, y_, i))/D_[i];
        x[i]= t;
        add_row_to_vec( A, t, y_, i); // y+= t* (i-th row of A)
    }
}

template <typename Mat, typename Vec>
void NEGSPcCL::BackwardGS(const Mat& A, Vec& x, const Vec& b) const
{
    // x= 0.; // implied, but superfluous, because we can assign the x-values below, not update.
    y_= 0.;
    for (size_t i= b.size() - 1; i < b.size(); --i) {
        x[i]= (b[i] - mul_row( A, y_, i))/D_[i];
        add_row_to_vec( A, x[i], y_, i); // y+= t* (i-th row of A)
    }
}

template <typename Mat, typename Vec>
void NEGSPcCL::Apply(const Mat& A, Vec& x, const Vec& b) const
{
    Update( A);

    ForwardGS( A, x, b);
    if (!symmetric_) return;
    BackwardGS( A, x, VectorCL( D_*x));
}

template <typename Mat, typename Vec>
void NEGSPcCL::ApplyTranspose(const Mat& A, Vec& x, const Vec& b) const
{
    Update( A);

    BackwardGS( A, x, b);
    if (!symmetric_) return;
    ForwardGS( A, x, VectorCL( D_*x));
}

template <typename Mat, typename Vec>
void NEGSPcCL::ForwardMulGS(const Mat& A, Vec& x, const Vec& b) const
{
    // x= 0.; // implied, but superfluous, because we can assign the x-values below, not update.
    y_= 0.;
    for (size_t i= 0 ; i < b.size(); ++i) {
        add_row_to_vec( A, b[i], y_, i); // y+= b[i]* (i-th row of A)
        x[i]= mul_row( A, y_, i);
    }
}

template <typename Mat, typename Vec>
void NEGSPcCL::BackwardMulGS(const Mat& A, Vec& x, const Vec& b) const
{
    // x= 0.; // implied, but superfluous, because we can assign the x-values below, not update.
    y_= 0.;
    for (size_t i= b.size() - 1 ; i < b.size(); --i) {
        add_row_to_vec( A, b[i], y_, i); // y+= b[i]* (i-th row of A)
        x[i]= mul_row( A, y_, i);
    }
}

template <typename Mat, typename Vec>
Vec NEGSPcCL::mul (const Mat& A, const Vec& b) const
{
    Update( A);

    Vec x( A.num_rows());
    VectorCL b2( b);
    if (symmetric_) {
        BackwardMulGS( A, x, b);
        b2= x/D_;
    }
    ForwardMulGS( A, x, b2);
    return x;
}

template <typename Mat, typename Vec>
Vec NEGSPcCL::transp_mul(const Mat& A, const Vec& b) const
{
    Update( A);

    Vec x( A.num_rows());
    VectorCL b2( b);
    if (symmetric_) {
        ForwardMulGS( A, x, b);
        b2= x/D_;
    }
    BackwardMulGS( A, x, b2);
    return x;
}


//*****************************************************************************
//
//  Iterative solvers: CG, PCG, PCGNE, GMRES, PMINRES, MINRES, BiCGStab, GCR,
//                     GMRESR, IDR(s)
//
//*****************************************************************************

//=============================================================================
//  Implementation of the methods
//=============================================================================

//-----------------------------------------------------------------------------
// CG: The return value indicates convergence within max_iter (input)
// iterations (true), or no convergence within max_iter iterations (false).
//
// Upon successful return, output arguments have the following values:
//
//        x - approximate solution to Ax = b
// max_iter - number of iterations performed before tolerance was reached
//      tol - (relative, see next parameter) 2-norm of the residual after the
//    final iteration
// measure_relative_tol - If true, stop if |b - Ax|/|b| <= tol,
//     if false, stop if |b - Ax| <= tol.
//-----------------------------------------------------------------------------

template <typename Mat, typename Vec>
bool
CG(const Mat& A, Vec& x, const Vec& b, int& max_iter, double& tol,
    bool measure_relative_tol= false)
{
    Vec r( A*x - b);
    Vec d( -r);
    double normb= norm( b), res, resid= norm_sq( r);

    if (normb == 0.0 || measure_relative_tol == false) normb= 1.0;

    if ((res= std::sqrt( resid)/normb) <= tol)
    {
        tol= res;
        max_iter= 0;
        return true;
    }

    for (int i= 1; i <= max_iter; ++i)
    {
        const Vec    Ad= A*d;
        const double delta= dot( Ad, d);
        const double alpha= resid/delta;
        double       beta= resid;

        axpy(alpha, d, x);  // x+= alpha*d;
        axpy(alpha, Ad, r); // r+= alpha*Ad;

        resid= norm_sq( r);
        if ((res= std::sqrt( resid)/normb) <= tol)
        {
            tol= res;
            max_iter= i;
            return true;
        }
        beta= resid / beta;
        d= beta*d-r;
    }
    tol= res;
    return false;
}


//-----------------------------------------------------------------------------
// PCG: The return value indicates convergence within max_iter (input)
// iterations (true), or no convergence within max_iter iterations (false).
// Upon successful return, output arguments have the following values:
//
//        x - approximate solution to Ax = b
// max_iter - number of iterations performed before tolerance was reached
//      tol - 2-norm of the (relative, see below) residual after the final iteration
// measure_relative_tol - If true, stop if |b - Ax|/|b| <= tol,
//     if false, stop if |b - Ax| <= tol.
//-----------------------------------------------------------------------------

template <typename Mat, typename Vec, typename PreCon>
bool
PCG(const Mat& A, Vec& x, const Vec& b, const PreCon& M,
    int& max_iter, double& tol, bool measure_relative_tol= false)
{
    const size_t n= x.size();
    Vec p( n), z( n), q( n), r( b - A*x);
    double rho, rho_1, normb= norm( b), resid;

    if (normb == 0.0 || measure_relative_tol == false) normb= 1.0;

    resid= norm( r)/normb;
    if (resid <= tol) {
        tol= resid;
        max_iter= 0;
        return true;
    }

    M.Apply(A, z, r);
    p= z;
    rho= dot( r, z);
    for (int i= 1; i <= max_iter; ++i) {
        q= A*p;
        const double alpha= rho/dot( p, q);
        axpy( alpha, p, x);                // x+= alpha*p;
        axpy( -alpha, q, r);               // r-= alpha*q;

        resid= norm( r)/normb;
        if (resid <= tol) {
            tol= resid;
            max_iter= i;
            return true;
        }

        M.Apply(A, z, r);
        rho_1= rho;
        rho= dot( r, z);
        z_xpay( p, z, rho/rho_1, p); // p= z + (rho/rho_1)*p;
    }
    tol= resid;
    return false;
}

/// \brief PCGNE: Preconditioned CG for the normal equations (error-minimization)
///
/// Solve A*A^T x = b with left preconditioner M. This is more stable than PCG with
/// a CompositeMatrixCL.
///
/// The return value indicates convergence within max_iter (input)
/// iterations (true), or no convergence within max_iter iterations (false).
/// Upon successful return, output arguments have the following values:
///
/// \param A - matrix (not necessarily quadratic)
/// \param b - right hand side
/// \param u - approximate solution to A*A^T u = b
/// \param M - preconditioner
/// \param max_iter - number of iterations performed before tolerance was reached
/// \param tol - 2-norm of the (relative, see below) residual after the final iteration
/// \param measure_relative_tol - If true, stop if |b - A*A^T u|/|b| <= tol,
///        if false, stop if |b - A*A^T u| <= tol.
template <typename Mat, typename Vec, typename PreCon>
bool
PCGNE(const Mat& A, Vec& u, const Vec& b, const PreCon& M,
    int& max_iter, double& tol, bool measure_relative_tol= false)
{
    Vec r( b - A*transp_mul( A, u));
    double normb= norm( b);
    if (normb == 0.0 || measure_relative_tol == false) normb= 1.0;
    double resid= norm( r)/normb;
    // std::cout << "PCGNE: iter: 0 resid: " << resid <<'\n';
    if (resid <= tol) {
        tol= resid;
        max_iter= 0;
        return true;
    }

    const size_t n= A.num_rows();
    const size_t num_cols= A.num_cols();

    Vec z( n);
    M.Apply( A, z, r);
    Vec qt( z), pt( num_cols);
    double rho= dot( z, r), rho_1;

    for (int i= 1; i <= max_iter; ++i) {
        pt= transp_mul( A, qt);
        const double alpha= rho/norm_sq( pt);
        u+= alpha*qt;
        r-= alpha*(A*pt);
        M.Apply( A, z, r);

        resid= norm( r)/normb;
        // if ( i%10 == 0) std::cout << "PCGNE: iter: " << i << " resid: " << resid <<'\n';
        if (resid <= tol) {
            tol= resid;
            max_iter= i;
            return true;
        }
        rho_1= rho;
        rho= dot( z, r);
        qt= z + (rho/rho_1)*qt;
    }
    tol= resid;
    return false;
}

//-----------------------------------------------------------------------------
// GMRES:
//-----------------------------------------------------------------------------

inline void GMRES_GeneratePlaneRotation(double &dx, double &dy, double &cs, double &sn)
{
    if (dy == 0.0)
    {
        cs = 1.0;
        sn = 0.0;
    }
    else
    {
        const double r=std::sqrt(dx*dx+dy*dy);
        cs = dx/r;
        sn = dy/r;
    }
}


inline void GMRES_ApplyPlaneRotation(double &dx, double &dy, double &cs, double &sn)
{
    const double tmp = cs*dx + sn*dy;
    dy = -sn*dx + cs*dy;
    dx = tmp;
}


template <typename Mat, typename Vec>
void GMRES_Update(Vec &x, int k, const Mat &H, const Vec &s, const std::vector<Vec> &v)
{
    Vec y(s);

    // Backsolve:
    for ( int i=k; i>=0; --i )
    {
        y[i] /= H(i,i);
        for ( int j=i-1; j>=0; --j )
            y[j] -= H(j,i) * y[i];
    }

    for ( int i=0; i<=k; ++i )
        x += y[i] * v[i];
}
enum PreMethGMRES { RightPreconditioning, LeftPreconditioning};
//-----------------------------------------------------------------------------
// GMRES: The return value indicates convergence within max_iter (input)
// iterations (true), or no convergence within max_iter iterations (false).
//
// Upon successful return, output arguments have the following values:
//
// x - approximate solution to Ax = b
// max_iter - number of iterations performed before tolerance was reached
// tol - 2-norm of the (relative, see below) preconditioned residual after the
//     final iteration
//
// measure_relative_tol - If true, stop if |M^(-1)( b - Ax)|/|M^(-1)b| <= tol,
//     if false, stop if |M^(-1)( b - Ax)| <= tol.
// calculate2norm - If true, the unpreconditioned (absolute) residual is
//     calculated in every step for debugging. This is rather expensive.
// method - If RightPreconditioning, solve the right-preconditioned GMRES: A*M^(-1)*u = b, u=Mx,
//     if LeftPreconditioning, solve the left-preconditioned GMRES:   M^(-1)*A*x= M^(-1)*b,
//TODO: bug in RightPreconditioning?
//-----------------------------------------------------------------------------
template <typename Mat, typename Vec, typename PreCon>
bool
GMRES(const Mat& A, Vec& x, const Vec& b, const PreCon& M,
      int /*restart parameter*/ m, int& max_iter, double& tol,
      bool measure_relative_tol= true, bool calculate2norm= false, PreMethGMRES method = LeftPreconditioning)
{
    m= (m <= max_iter) ? m : max_iter; // m > max_iter only wastes memory.

    DMatrixCL<double> H( m, m);
    Vec               s( m), cs( m), sn( m), w( b.size()), r( b.size());
    std::vector<Vec>  v( m);
    double            beta, normb, resid;
    Vec z(x.size()), t(x.size());
    for (int i= 0; i < m; ++i)
        v[i].resize( b.size());

     if (method == RightPreconditioning)
     {
          r= b - A*x;
          beta= norm( r);
          normb= norm(b);
     }
     else
     {
          M.Apply( A, r, Vec( b - A*x));
          beta= norm( r);
          M.Apply( A, w, b);
          normb= norm( w);
     }
    if (normb == 0.0 || measure_relative_tol == false) normb= 1.0;

    resid = beta/normb;
    if (resid <= tol) {
        tol= resid;
        max_iter= 0;
        return true;
    }

    int j= 1;
    while (j <= max_iter) {
        v[0]= r*(1.0/beta);
        s= 0.0;
        s[0]= beta;

        int i;
        for (i= 0; i < m - 1 && j <= max_iter; ++i, ++j) {
            if (method == RightPreconditioning)
            {
                M.Apply( A, w, v[i]);
                w=A*w;
            }
            else M.Apply( A, w, A*v[i]);
            for (int k= 0; k <= i; ++k ) {
                H( k, i)= dot( w, v[k]);
                w-= H( k, i)*v[k];
            }

            H( i + 1, i)= norm( w);
            v[i + 1]= w*(1.0/H( i + 1, i));

            for (int k= 0; k < i; ++k)
                GMRES_ApplyPlaneRotation( H(k,i), H(k + 1, i), cs[k], sn[k]);

            GMRES_GeneratePlaneRotation( H(i,i), H(i+1,i), cs[i], sn[i]);
            GMRES_ApplyPlaneRotation( H(i,i), H(i+1,i), cs[i], sn[i]);
            GMRES_ApplyPlaneRotation( s[i], s[i+1], cs[i], sn[i]);

            resid= std::abs( s[i+1])/normb;
            if (calculate2norm == true) { // debugging aid
                Vec y( x);
                if (method == RightPreconditioning)
                {
                    z=0.;
                    GMRES_Update( z, i, H, s, v);
                    M.Apply( A, t, z);
                    y+=t;
                }
                else GMRES_Update( y, i, H, s, v);
                double resid2= norm( Vec( b - A*y));
                std::cout << "GMRES: absolute residual 2-norm: " << resid2
                          << "\tabsolute preconditioned residual 2-norm: "
                          << std::fabs( s[i+1]) << '\n';
            }
            if (resid <= tol) {
                if (method == RightPreconditioning)
                {
                    z=0.;
                    GMRES_Update( z, i, H, s, v);
                    M.Apply( A, t, z);
                    x+=t;
                }
                else GMRES_Update( x, i, H, s, v);
                tol= resid;
                max_iter= j;
                return true;
            }
        }

        if (method == RightPreconditioning)
        {
            z=0.;
            GMRES_Update( z, i - 1, H, s, v);
            M.Apply( A, t, z);
            x+=t;
            r= b - A*x;
        }
        else
        {
            GMRES_Update( x, i - 1, H, s, v);
            M.Apply( A, r, Vec( b - A*x));
        }
        beta=norm(r);
        resid= beta/normb;
        if (resid <= tol) {
            tol= resid;
            max_iter= j;
            return true;
        }
    }
    tol= resid;
    return false;
}


// One recursive step of Lanzcos' algorithm for computing an ONB (q1, q2, q3,...)
// of the Krylovspace of A for a given starting vector r. This is a three term
// recursion, computing the next q_i from the two previous ones.
// See Arnold Reusken, "Numerical methods for elliptic partial differential equations",
// p. 148.
// Returns false for 'lucky breakdown' (see below), true in the generic case.
template <typename Mat, typename Vec>
bool
LanczosStep(const Mat& A,
            const Vec& q0, const Vec& q1, Vec& q2,
            double& a1,
            const double b0, double& b1)
{
    q2= A*q1 - b0*q0;
    a1= dot( q2, q1);
    q2-= a1*q1;
    b1= norm( q2);
    // Lucky breakdown; the Krylov-space K up to q1 is A-invariant. Thus,
    // the correction dx needed to solve A(x0+dx)=b is in this space and
    // the Minres-algo will terminate with the exact solution in the
    // following step.
    if (b1 < 1e-15) return false;
    q2/= b1;
    return true;
}

template <typename Vec>
class LanczosONBCL
{
  private:
    bool nobreakdown_;
    double norm_r0_;

  public:
    SBufferCL<Vec, 3> q;
    double a0;
    SBufferCL<double, 2> b;

    template <typename Mat>
    void // Sets up initial values and computes q0.
    new_basis(const Mat& A, const Vec& r0) {
        q[-1].resize( r0.size(), 0.);
        norm_r0_= norm( r0);
        q[0].resize( r0.size(), 0.); q[0]= r0/norm_r0_;
        q[1].resize( r0.size(), 0.);
        b[-1]= 0.;
        nobreakdown_= LanczosStep( A, q[-1], q[0], q[1], a0, b[-1], b[0]);
    }

    double norm_r0() const {
        return norm_r0_; }
    bool
    breakdown() const {
        return !nobreakdown_; }
    // Computes new q_i, a_i, b_1, q_{i+1} in q0, a0, b0, q1 and moves old
    // values to qm1, bm1.
    template <typename Mat>
    bool
    next( const Mat& A) {
        q.rotate(); b.rotate();
        return (nobreakdown_= LanczosStep( A, q[-1], q[0], q[1], a0, b[-1], b[0]));
    }
};


// One recursive step of the preconditioned Lanzcos algorithm for computing an ONB of
// a Krylovspace. This is a three term
// recursion, computing the next q_i from the two previous ones.
// See Arnold Reusken, "Numerical methods for elliptic partial differential equations",
// p. 153.
// Returns false for 'lucky breakdown' (see below), true in the generic case.
template <typename Mat, typename Vec, typename PreCon>
bool
PLanczosStep(const Mat& A,
             const PreCon& M,
             const Vec& q1, Vec& q2,
             const Vec& t0, const Vec& t1, Vec& t2,
             double& a1,
             const double b0, double& b1)
{
    t2= A*q1 - b0*t0;
    a1= dot( t2, q1);
    t2-= a1*t1;
    M.Apply( A, q2, t2);
    const double b1sq= dot( q2, t2);
    Assert( b1sq >= 0.0, "PLanczosStep: b1sq is negative!\n", DebugNumericC);
    b1= std::sqrt( b1sq);
    if (b1 < 1e-15) return false;
    t2*= 1./b1;
    q2*= 1./b1;
    return true;
}

template <typename Vec, typename PreCon>
class PLanczosONBCL
{
  private:
    bool nobreakdown_;
    double norm_r0_;

  public:
    const PreCon& M;
    SBufferCL<Vec, 2> q;
    SBufferCL<Vec, 3> t;
    double a0;
    SBufferCL<double, 2> b;

    // Sets up initial values and computes q0.
    PLanczosONBCL(const PreCon& M_)
      : M( M_) {}

    template <typename Mat>
    void // Sets up initial values and computes q0.
    new_basis(const Mat& A, const Vec& r0) {
        t[-1].resize( r0.size(), 0.);
        q[-1].resize( r0.size(), 0.); M.Apply( A, q[-1], r0);
        norm_r0_= std::sqrt( dot( q[-1], r0));
        t[0].resize( r0.size(), 0.); t[0]= r0/norm_r0_;
        q[0].resize( r0.size(), 0.); q[0]= q[-1]/norm_r0_;
        t[1].resize( r0.size(), 0.);
        b[-1]= 0.;
        nobreakdown_= PLanczosStep( A, M, q[0], q[1], t[-1], t[0], t[1], a0, b[-1], b[0]);
    }

    double norm_r0() const {
        return norm_r0_; }
    bool
    breakdown() const {
        return !nobreakdown_; }
    // Computes new q_i, t_i, a_i, b_1, q_{i+1} in q0, t_0, a0, b0, q1 and moves old
    // values to qm1, tm1, bm1.
    template <typename Mat>
    bool
    next(const Mat& A) {
        q.rotate(); t.rotate(); b.rotate();
        return (nobreakdown_= PLanczosStep( A, M, q[0], q[1], t[-1], t[0], t[1], a0, b[-1], b[0]));
    }
};

//-----------------------------------------------------------------------------
// PMINRES: The return value indicates convergence within max_iter (input)
// iterations (true), or no convergence within max_iter iterations (false).
// See Arnold Reusken, "Numerical methods for elliptic partial differential
// equations", pp. 149 -- 154
//
// Upon successful return, output arguments have the following values:
//
//        x - approximate solution to Ax = rhs
// max_iter - number of iterations performed before tolerance was reached
//      tol - (relative, see below) residual b - Ax measured in the (M^-1 ., .)-
//     inner-product-norm.
// measure_relative_tol - If true, stop if (M^(-1)( b - Ax), b - Ax)/(M^(-1)b, b) <= tol,
//     if false, stop if (M^(-1)( b - Ax), b - Ax) <= tol.
//-----------------------------------------------------------------------------
template <typename Mat, typename Vec, typename Lanczos>
bool
PMINRES(const Mat& A, Vec& x, const Vec&, Lanczos& q, int& max_iter, double& tol,
    bool measure_relative_tol= false)
{
    Vec dx( x.size());
    const double norm_r0= q.norm_r0();
    double normb= std::fabs( norm_r0);
    double res= norm_r0;
    bool lucky= q.breakdown();
    SBufferCL<double, 3> c;
    SBufferCL<double, 3> s;
    SBufferCL<SVectorCL<3>, 3> r;
    SBufferCL<Vec, 3> p;
    p[0].resize( x.size()); p[1].resize( x.size()); p[2].resize( x.size());
    SBufferCL<SVectorCL<2>, 2> b;

    if (normb == 0.0 || measure_relative_tol == false) normb= 1.0;

    if ((res= norm_r0/normb) <= tol) {
        tol= res;
        max_iter= 0;
        return true;
    }
    std::cout << "PMINRES: k: 0\tresidual: " << res << std::endl;
    for (int k= 1; k <= max_iter; ++k) {
        switch (k) {
          case 1:
            // Compute r1
            GMRES_GeneratePlaneRotation( q.a0, q.b[0], c[0], s[0]);
            r[0][0]= std::sqrt( q.a0*q.a0 + q.b[0]*q.b[0]);
            // Compute p1
            // p[0]= q.q[0]/r[0][0];
            p[0]= q.q[0]/r[0][0];
            // Compute b11
            b[0][0]= 1.; b[0][1]= 0.;
            GMRES_ApplyPlaneRotation(b[0][0], b[0][1], c[0], s[0]);
            break;
          case 2:
            // Compute r2
            r[0][0]= q.b[-1]; r[0][1]= q.a0; r[0][2]= q.b[0];
            GMRES_ApplyPlaneRotation( r[0][0], r[0][1], c[-1], s[-1]);
            GMRES_GeneratePlaneRotation( r[0][1], r[0][2], c[0], s[0]);
            GMRES_ApplyPlaneRotation( r[0][1], r[0][2], c[0], s[0]);
            // Compute p2
            // p[0]= (q.q[0] - r[0][0]*p[-1])/r[0][1];
            p[0]= (q.q[0] - r[0][0]*p[-1])/r[0][1];
            // Compute b22
            b[0][0]= b[-1][1]; b[0][1]= 0.;
            GMRES_ApplyPlaneRotation( b[0][0], b[0][1], c[0], s[0]);
            break;
          default:
            r[0][0]= 0.; r[0][1]= q.b[-1]; r[0][2]= q.a0;
            double tmp= q.b[0];
            GMRES_ApplyPlaneRotation( r[0][0], r[0][1], c[-2], s[-2]);
            GMRES_ApplyPlaneRotation( r[0][1], r[0][2], c[-1], s[-1]);
            GMRES_GeneratePlaneRotation( r[0][2], tmp, c[0], s[0]);
            GMRES_ApplyPlaneRotation( r[0][2], tmp, c[0], s[0]);
            // p[0]= (q.q[0] - r[0][0]*p[-2] -r[0][1]*p[-1])/r[0][2];
            p[0]= (q.q[0] - r[0][0]*p[-2] -r[0][1]*p[-1])*(1/r[0][2]);
            b[0][0]= b[-1][1]; b[0][1]= 0.;
            GMRES_ApplyPlaneRotation( b[0][0], b[0][1], c[0], s[0]);
        }
        dx= norm_r0*b[0][0]*p[0];
        x+= dx;

        res= std::fabs( norm_r0*b[0][1])/normb;
        if (k%10==0) std::cout << "PMINRES: k: " << k << "\tresidual: " << res << std::endl;
        if (res<= tol || lucky==true) {
            tol= res;
            max_iter= k;
            return true;
        }
        q.next( A);
        if (q.breakdown()) {
            lucky= true;
            std::cout << "PMINRES: lucky breakdown" << std::endl;
        }
        c.rotate(); s.rotate(); r.rotate(); p.rotate(); b.rotate();
    }
    tol= res;
    return false;
}


template <typename Mat, typename Vec>
bool
MINRES(const Mat& A, Vec& x, const Vec& rhs, int& max_iter, double& tol,
    bool measure_relative_tol= false)
{
    LanczosONBCL<Vec> q;
    q.new_basis( A, Vec( rhs - A*x));
    return PMINRES( A,  x, rhs, q, max_iter, tol, measure_relative_tol);
}


//*****************************************************************
// BiCGSTAB
//
// BiCGSTAB solves the unsymmetric linear system Ax = b
// using the Preconditioned BiConjugate Gradient Stabilized method
//
// BiCGSTAB follows the algorithm described on p. 27 of the
// SIAM Templates book.
//
// The return value indicates convergence within max_iter (input)
// iterations (true), or no convergence or breakdown within
// max_iter iterations (false). In cases of breakdown a message is printed
// std::cout and the iteration returns the approximate solution found.
//
// Upon successful return, output arguments have the following values:
//
//        x  --  approximate solution to Ax = b
// max_iter  --  the number of iterations performed before the
//               tolerance was reached
//      tol  --  the residual after the final iteration
//
// measure_relative_tol - If true, stop if |b - Ax|/|b| <= tol,
//     if false, stop if |b - Ax| <= tol. ( |.| is the euclidean norm.)
//
//*****************************************************************
template <class Mat, class Vec, class Preconditioner>
bool
BICGSTAB( const Mat& A, Vec& x, const Vec& b,
    const Preconditioner& M, int& max_iter, double& tol,
    bool measure_relative_tol= true)
{
    double rho_1= 0.0, rho_2= 0.0, alpha= 0.0, beta= 0.0, omega= 0.0;
    Vec p( x.size()), phat( x.size()), s( x.size()), shat( x.size()),
        t( x.size()), v( x.size());

    double normb= norm( b);
    Vec r( b - A*x);
    Vec rtilde= r;

    if (normb == 0.0 || measure_relative_tol == false) normb = 1.0;

    double resid= norm( r)/normb;
    if (resid <= tol) {
        tol= resid;
        max_iter= 0;
        return true;
    }

    for (int i= 1; i <= max_iter; ++i) {
        rho_1= dot( rtilde, r);
        if (rho_1 == 0.0) {
            tol = norm( r)/normb;
            max_iter= i;
            std::cout << "BiCGSTAB: Breakdown with rho_1 = 0.\n";
            return false;
        }
        if (i == 1) p= r;
        else {
            beta= (rho_1/rho_2)*(alpha/omega);
            p= r + beta*(p - omega*v);
        }
        M.Apply( A, phat, p);
        v= A*phat;
        alpha= rho_1/dot( rtilde, v);
        s= r - alpha*v;
        if ((resid= norm( s)/normb) < tol) {
            x+= alpha*phat;
            tol= resid;
            max_iter= i;
            return true;
        }
        M.Apply( A, shat, s);
        t= A*shat;
        omega= dot( t, s)/dot( t, t);
        x+= alpha*phat + omega*shat;
        r= s - omega*t;

        rho_2= rho_1;
        if ((resid= norm( r)/normb) < tol) {
            tol= resid;
            max_iter= i;
            return true;
        }
        if (omega == 0.0) {
            tol= norm( r)/normb;
            max_iter= i;
            std::cout << "BiCGSTAB: Breakdown with omega_ = 0.\n";
            return false;
        }
    }
    tol= resid;
    return false;
}

//*****************************************************************
// GCR
//
// GCR solves the unsymmetric linear system Ax = b
// using the Preconditioned Generalized Conjugate Residuals method;
//
// The return value indicates convergence within max_iter (input)
// iterations (true), or no convergence within
// max_iter iterations (false).
//
// Upon successful return, output arguments have the following values:
//
//        x  --  approximate solution to Ax = b
// max_iter  --  the number of iterations performed before the
//               tolerance was reached
//      tol  --  the residual after the final iteration
//
//
// m -- truncation parameter; only m >= 1 residual vectors are kept; the
//     one with the smallest a (in modulus) is overwritten by the
//     new vector sn (min-alpha strategy).
// measure_relative_tol -- If true, stop if |b - Ax|/|b| <= tol,
//     if false, stop if |b - Ax| <= tol. ( |.| is the euclidean norm.)
//
//*****************************************************************
template <class Mat, class Vec, class Preconditioner>
bool
GCR(const Mat& A, Vec& x, const Vec& b, const Preconditioner& M,
    int m, int& max_iter, double& tol, bool measure_relative_tol= true)
{
    m= (m <= max_iter) ? m : max_iter; // m > max_iter only wastes memory.

    Vec r( b - A*x);
    Vec sn( b.size()), vn( b.size());
    std::vector<Vec> s, v;
    std::vector<double> a( m);

    double normb= norm( b);
    if (normb == 0.0 || measure_relative_tol == false) normb= 1.0;
    double resid= norm( r)/normb;
    for (int k= 0; k < max_iter; ++k) {
        if (k%10==0) std::cout << "GCR: k: " << k << "\tresidual: " << resid << std::endl;
        if (resid < tol) {
            tol= resid;
            max_iter= k;
            return true;
        }
        M.Apply( A, sn, r);
        vn= A*sn;
        for (int i= 0; i < k && i < m; ++i) {
            const double alpha= dot( vn, v[i]);
            a[i]= alpha;
            vn-= alpha*v[i];
            sn-= alpha*s[i];
        }
        const double beta= norm( vn);
        vn/= beta;
        sn/= beta;
        const double gamma= dot( r, vn);
        x+= gamma*sn;
        r-= gamma*vn;
        resid= norm( r)/normb;
        if (k < m) {
            s.push_back( sn);
            v.push_back( vn);
        }
        else {
            int min_idx= 0;
            double a_min= std::fabs( a[0]); // m >= 1, thus this access is valid.
            for (int i= 1; i < k && i < m; ++i)
                if ( std::fabs( a[i]) < a_min) {
                    min_idx= i;
                    a_min= std::fabs( a[i]);
                }
            s[min_idx]= sn;
            v[min_idx]= vn;
        }
    }
    tol= resid;
    return false;
}

//*****************************************************************
// GMRESR
//
// GMRESR solves the unsymmetric linear system Ax = b
// using the GMRES Recursive method;
//
// The return value indicates convergence within max_iter (input)
// iterations (true), or no convergence within
// max_iter iterations (false).
//
// Upon successful return, output arguments have the following values:
//
//        x  --  approximate solution to Ax = b
// max_iter  --  the number of iterations performed before the
//               tolerance was reached
//      tol  --  the residual after the final iteration
//
// measure_relative_tol - If true, stop if |b - Ax|/|b| <= tol,
//     if false, stop if |b - Ax| <= tol. ( |.| is the euclidean norm.)
//
//*****************************************************************
template <class Mat, class Vec, class Preconditioner>
bool
GMRESR( const Mat& A, Vec& x, const Vec& b, const Preconditioner& M,
    int /*restart parameter m*/ m, int& max_iter, int& inner_max_iter, double& tol, double& inner_tol,
    bool measure_relative_tol= true, PreMethGMRES method = RightPreconditioning)
{  Vec r( b - A*x);
    std::vector<Vec> u(1), c(1); // Positions u[0], c[0] are unused below.
    double normb= norm( b);
    if (normb == 0.0 || measure_relative_tol == false) normb= 1.0;
    double resid= -1.0;

    for (int k= 0; k < max_iter; ++k) {
        if ((resid= norm( r)/normb) < tol) {
            tol= resid;
            max_iter= k;
            return true;
        }
        std::cout << "GMRESR: k: " << k << "\tresidual: " << resid << std::endl;
        u.push_back( Vec( b.size()));
        u[k+1]=0;
        inner_tol=0.0;
        double in_tol = inner_tol;
        int in_max_iter = inner_max_iter;
        GMRES(A, u[k+1], r, M, m, in_max_iter, in_tol, true, false, method);
        std::cout << "norm of u_k_0: "<<norm(u[k+1])<<"\n";
        std::cout << "inner iteration:  " << in_max_iter << " GMRES iteration(s),\tresidual: " << in_tol << std::endl;
        if (norm(A*u[k+1]-r)>0.999*norm(r) && norm(u[k+1]) < 1e-3)
        {
            u[k+1] = transp_mul(A, r);
            std::cout<<"LSQR switch!\n";
        }
        c.push_back( A*u[k+1]);
        for (int i= 1; i <= k; ++i) {
            const double alpha= dot( c[k+1], c[i]);
            c[k+1]-= alpha*c[i];
            u[k+1]-= alpha*u[i];
        }
        const double beta= norm( c[k+1]);
        c[k+1]/= beta;
        u[k+1]/= beta;
        const double gamma= dot( r, c[k+1]);
        x+= gamma*u[k+1];
        r-= gamma*c[k+1];
    }
    tol= resid;
    return false;
}


//*****************************************************************
// IDR(s)
//
// IDR(s) solves the unsymmetric linear system Ax = b
// using the method by Martin B. van Gijzen / Peter Sonneveld
// An Elegant IDR(s) Variant that Efficiently Exploits
// Bi-Orthogonality Properties
// ISSN 1389-6520
// Departent of Applied Mathematical Analysis,  Report 10-16
// Delft University of Technology, 2010
//
// The return value indicates convergence within max_iter (input)
// iterations (true), or no convergence within
// max_iter iterations (false).
//
// Upon successful return, output arguments have the following values:
//
//        x  --  approximate solution to Ax = b
// max_iter  --  the number of iterations performed before the
//               tolerance was reached
//      tol  --  the residual after the final iteration
//
// measure_relative_tol - If true, stop if |b - Ax|/|b| <= tol,
//     if false, stop if |b - Ax| <= tol. ( |.| is the euclidean norm.)
//
//*****************************************************************
template <class Mat, class Vec, class PC>
bool
IDRS( const Mat& A, Vec& x, const Vec& rhs, PC& pc, int& max_iter, double& tol, bool measure_relative_tol= false,
		const int s=4, typename Vec::value_type omega_bound=0.7)
{
    typedef typename Vec::value_type ElementTyp;
    int n= x.size(),  it;

    ElementTyp omega = 1.0;
    Vec v(n), t(n), f(s);

    double normb = norm(rhs);
    if (normb == 0.0 || measure_relative_tol == false)
    	normb= 1.0;

    Vec resid( rhs - A*x);
    double normres = norm (resid);
    if ( normres/normb <= tol ) {
        tol= normres;
        max_iter= 0;
        return true;
    }

    std::vector<Vec> P(s, Vec( x.size()));
    srand ( time(NULL) );
    for (int i=0; i<s; ++i){
        for ( size_t j=0; j< x.size(); ++j)
            P[i][j] = (double) (2.0 *rand()) / RAND_MAX - 1.0;  // random in [-1,1)
    }

    // orthonormalize P   (mod. Gram-Schmidt)
    for ( int k= 0; k < s; k++ ) {
        for ( int j= 0; j < k; j++ ) {
            ElementTyp sm= dot( P[j], P[k]);
            P[k]-= sm*P[j];
        }
        P[k]/= norm(P[k]);
    }

    std::vector<Vec> G(s, Vec( x.size())), U(s, Vec( x.size()));
    DMatrixCL<ElementTyp> M(s,s);
    for (int i = 0; i <s; ++i)
        M(i,i) = 1.0;
    it= 0;
    while ( normres/normb > tol && it < max_iter ) {
        for (int i=0; i < s; i++)  f[i]= dot(P[i], resid);
        for (int k=0; k < s; k++) {
//            if (it % 10 == 0) std::cout << "IDR(s) iter: " << it << "\tres: " << normres << std::endl;
            // solve the lower tridiagonal system  M[k:s,k:s] c = f[k:s]
            Vec c(s-k);
            for (int j=0; j < s-k; j++) {
                double cs = f[k+j];
                for (int l=0; l < j; l++) cs-= M(k+j,k+l)*c[l];
                c[j]= cs/M(k+j,k+j);
            }
            v= resid;
            for (int j=0; j < s-k; j++) v-= c[j]*G[k+j];
            pc.Apply( A, v, v);

            // Compute new U(:,k) and G(:,k), G(:,k) is in space G_j
            U[k]= c[0]*U[k] + omega*v;
            for (int j=1; j < s-k; j++) U[k]+= c[j]*U[k+j];
            G[k]= A * U[k];
            // Bi-Orthogonalize the new basis vectors
            for (int i= 0; i < k; i++) {
                ElementTyp alpha= dot (P[i], G[k])/M(i,i);
                G[k]-= alpha*G[i];
                U[k]-= alpha*U[i];
            }
            // compute new column of M (first k-1 entries are zero)
            for (int j=0; j < s-k; j++) M(k+j,k)= dot (P[k+j], G[k]);
            if ( M(k,k) == 0 )
            {
                throw DROPSErrCL( "IDR(s): M(k,k) ==0");
            }

            //    make  R orthogonal to  G
            ElementTyp beta = f[k] / M(k,k);
            resid-= beta*G[k];
            x+= beta*U[k];
            normres= norm(resid);
            it++;
            if ( normres/normb <= tol)   break;
            if ( k+1 < s ) {
                for (int j=1; j < s-k; j++) f[k+j]-= beta*M(k+j,k);
            }
        }  //  end of  k = 0 .. (s-1)

        if ( normres/normb <= tol)   break;
        // Entering  G+
        pc.Apply( A, v, resid);
        t= A*v;
        double tn = norm(t), tr = dot(t, resid);
        omega= tr/(tn*tn);
        ElementTyp rho= std::abs(tr)/(tn*normres);
        if ( rho < omega_bound )  omega*= omega_bound/rho;
        if ( omega == 0 ) {
            throw DROPSErrCL( "IDR(s): omega ==0");
        }
        resid-= omega*t;  x+= omega*v;
        normres= norm(resid);
        it++;
    }
    if (tol > normres/normb) {
        max_iter = it;
        tol = normres;
        return false;
    }
    max_iter = it;
    tol = normres;

    return true;
}



//=============================================================================
//  Drivers
//=============================================================================

// What every iterative solver should have
class SolverBaseCL
{
  protected:
    int            _maxiter;
    mutable int    _iter;
    double         _tol;
    mutable double _res;
    bool           rel_;

    mutable std::ostream* output_;

    SolverBaseCL (int maxiter, double tol, bool rel= false, std::ostream* output= 0)
        : _maxiter( maxiter), _iter( -1), _tol( tol), _res( -1.),
          rel_( rel), output_( output)  {}
    virtual ~SolverBaseCL() {}

  public:
    virtual void   SetTol     (double tol) { _tol= tol; }
    virtual void   SetMaxIter (int iter)   { _maxiter= iter; }
    virtual void   SetRelError(bool rel)   { rel_= rel; }

    virtual double GetTol     () const { return _tol; }
    virtual int    GetMaxIter () const { return _maxiter; }
    virtual double GetResid   () const { return _res; }
    virtual int    GetIter    () const { return _iter; }
    virtual bool   GetRelError() const { return rel_; }

};

/// \brief base class for "expensive" preconditioners.
///
/// Preconditioner base class for Schur complement preconditioners (see SchurPreBaseCL)
/// and SolverAsPreCL, as those are computational expensive compared to the additional virtual function call.
/// Anyway, Jacobi and Gauss-Seidel type preconditioners are not derived from this class to avoid the virtual function overhead.
class PreBaseCL
{
  protected:
    mutable std::ostream* output_;
    mutable Uint iter_;
    
    PreBaseCL( std::ostream* output= 0)
      : output_( output), iter_(0) {}
    virtual ~PreBaseCL() {}
    
    void AddIter( int iter) const { iter_+= iter; }

  public:
    virtual void Apply( const MatrixCL& A,   VectorCL& x, const VectorCL& b) const = 0;
    virtual void Apply( const MLMatrixCL& A, VectorCL& x, const VectorCL& b) const = 0;

    /// reset iteration counter
    void ResetIter()    const { iter_= 0; }
    /// return total number of iterations
    Uint GetTotalIter() const { return iter_; }
};



// Bare CG solver
class CGSolverCL : public SolverBaseCL
{
  public:
    CGSolverCL(int maxiter, double tol, bool rel= false)
        : SolverBaseCL(maxiter, tol, rel) {}

    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b)
    {
        _res=  _tol;
        _iter= _maxiter;
        CG(A, x, b, _iter, _res, rel_);
    }
    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b, int& numIter, double& resid) const
    {
        resid=   _tol;
        numIter= _maxiter;
        CG(A, x, b, numIter, resid, rel_);
    }
};


// With preconditioner
template <typename PC>
class PCGSolverCL : public SolverBaseCL
{
  private:
    PC& _pc;

  public:
    PCGSolverCL(PC& pc, int maxiter, double tol, bool rel= false)
        : SolverBaseCL(maxiter, tol, rel), _pc(pc) {}

    PC&       GetPc ()       { return _pc; }
    const PC& GetPc () const { return _pc; }

    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b)
    {
        _res=  _tol;
        _iter= _maxiter;
        PCG(A, x, b, _pc, _iter, _res, rel_);
    }
    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b, int& numIter, double& resid) const
    {
        resid=   _tol;
        numIter= _maxiter;
        PCG(A, x, b, _pc, numIter, resid, rel_);
    }
};

///\brief Solver for A*A^Tx=b with Craig's method and left preconditioning.
///
/// A preconditioned CG version for matrices of the form A*A^T. Note that *A* must be
/// supplied, not A*A^T, to the Solve-method.
template <typename PC>
class PCGNESolverCL : public SolverBaseCL
{
  private:
    PC& pc_;

  public:
    PCGNESolverCL(PC& pc, int maxiter, double tol, bool rel= false)
        : SolverBaseCL( maxiter, tol, rel), pc_( pc) {}

    PC&       GetPc ()       { return pc_; }
    const PC& GetPc () const { return pc_; }

    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b)
    {
        _res=  _tol;
        _iter= _maxiter;
        PCGNE( A, x, b, pc_, _iter, _res, rel_);
    }
    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b, int& numIter, double& resid) const
    {
        resid=   _tol;
        numIter= _maxiter;
        PCGNE(A, x, b, pc_, numIter, resid, rel_);
    }
};

// Bare MINRES solver
class MResSolverCL : public SolverBaseCL
{
  public:
    MResSolverCL(int maxiter, double tol, bool rel= false)
        : SolverBaseCL( maxiter, tol, rel) {}

    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b)
    {
        _res=  _tol;
        _iter= _maxiter;
        MINRES( A, x, b, _iter, _res, rel_);
    }
    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b, int& numIter, double& resid) const
    {
        resid=   _tol;
        numIter= _maxiter;
        MINRES( A, x, b, numIter, resid, rel_);
    }
};

// Preconditioned MINRES solver
template <typename Lanczos>
class PMResSolverCL : public SolverBaseCL
{
  private:
    Lanczos& q_;

  public:
    PMResSolverCL(Lanczos& q, int maxiter, double tol, bool rel= false)
      :SolverBaseCL( maxiter, tol, rel), q_( q) {}

    Lanczos&       GetONB ()       { return q_; }
    const Lanczos& GetONB () const { return q_; }

    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b)
    {
        _res=  _tol;
        _iter= _maxiter;
        q_.new_basis( A, Vec( b - A*x));
        PMINRES( A, x, b, q_, _iter, _res, rel_);
    }
    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b, int& numIter, double& resid) const
    {
        resid=   _tol;
        numIter= _maxiter;
        q_.new_basis( A, Vec( b - A*x));
        PMINRES( A, x, b, q_, numIter, resid, rel_);
    }
};

// GMRES
template <typename PC>
class GMResSolverCL : public SolverBaseCL
{
  private:
    PC&          pc_;
    int          restart_;
    bool         calculate2norm_;
    PreMethGMRES method_;

  public:
    GMResSolverCL( PC& pc, int restart, int maxiter, double tol,
        bool relative= true, bool calculate2norm= false, PreMethGMRES method= LeftPreconditioning)
        : SolverBaseCL( maxiter, tol, relative), pc_(pc), restart_(restart),
          calculate2norm_(calculate2norm), method_(method){}

    PC&       GetPc      ()       { return pc_; }
    const PC& GetPc      () const { return pc_; }
    int       GetRestart () const { return restart_; }

    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b)
    {
        _res=  _tol;
        _iter= _maxiter;
        GMRES(A, x, b, pc_, restart_, _iter, _res, rel_, calculate2norm_, method_);
    }
    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b, int& numIter, double& resid) const
    {
        resid=   _tol;
        numIter= _maxiter;
        GMRES(A, x, b, pc_, restart_, numIter, resid, rel_, calculate2norm_, method_);
    }
};

// BiCGStab
template <typename PC>
class BiCGStabSolverCL : public SolverBaseCL
{
  private:
    PC& pc_;

  public:
    BiCGStabSolverCL( PC& pc, int maxiter, double tol, bool relative= true)
        : SolverBaseCL( maxiter, tol, relative), pc_( pc){}

          PC& GetPc ()       { return pc_; }
    const PC& GetPc () const { return pc_; }

    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b)
    {
        _res=  _tol;
        _iter= _maxiter;
        BICGSTAB( A, x, b, pc_, _iter, _res, rel_);
    }
    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b, int& numIter, double& resid) const
    {
        resid=   _tol;
        numIter= _maxiter;
        BICGSTAB(A, x, b, pc_, numIter, resid, rel_);
    }
};

// GCR
template <typename PC>
class GCRSolverCL : public SolverBaseCL
{
  private:
    PC& pc_;
    int truncate_; // no effect atm.

  public:
    GCRSolverCL( PC& pc, int truncate, int maxiter, double tol,
        bool relative= true, std::ostream* output= 0)
        : SolverBaseCL( maxiter, tol, relative, output), pc_( pc),
          truncate_( truncate) {}

    PC&       GetPc      ()       { return pc_; }
    const PC& GetPc      () const { return pc_; }
    int       GetTruncate() const { return truncate_; }

    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b)
    {
        _res=  _tol;
        _iter= _maxiter;
        GCR( A, x, b, pc_, truncate_, _iter, _res, rel_);
        if (output_ != 0)
            *output_ << "GCRSolverCL: iterations: " << GetIter()
                     << "\tresidual: " << GetResid() << std::endl;
    }
    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b, int& numIter, double& resid) const
    {
        resid=   _tol;
        numIter= _maxiter;
        GCR(A, x, b, pc_, truncate_, numIter, resid, rel_);
        if (output_ != 0)
            *output_ << "GCRSolverCL: iterations: " << GetIter()
                    << "\tresidual: " << GetResid() << std::endl;
    }
};
// GMRESR
template <typename PC>
class GMResRSolverCL : public SolverBaseCL
{
  private:
    PC&          pc_;
    int          restart_;
    int          inner_maxiter_;
    double       inner_tol_;
    PreMethGMRES method_;
  public:
    GMResRSolverCL( PC& pc, int restart, int maxiter, int  inner_maxiter,
        double tol, double  inner_tol, bool relative= true, PreMethGMRES method = RightPreconditioning, std::ostream* output= 0)
        : SolverBaseCL( maxiter, tol, relative, output), pc_(pc), restart_(restart),
         inner_maxiter_( inner_maxiter), inner_tol_(inner_tol), method_(method){}

    PC&       GetPc      ()       { return pc_; }
    const PC& GetPc      () const { return pc_; }
    int       GetRestart () const { return restart_; }
    void   SetInnerTol     (double tol) { inner_tol_= tol; }
    void   SetInnerMaxIter (int iter)   { inner_maxiter_= iter; }

    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b)
    {
        _res=  _tol;
        _iter= _maxiter;
        GMRESR(A, x, b, pc_, restart_, _iter, inner_maxiter_, _res, inner_tol_, rel_, method_);
        if (output_ != 0)
            *output_ << "GmresRSolverCL: iterations: " << GetIter()
                     << "\tresidual: " << GetResid() << std::endl;
            std::cout << "GmresRSolverCL: iterations: " << GetIter()
                     << "\tresidual: " << GetResid() << std::endl;
    }
    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b, int& numIter, double& resid) const
    {
        resid=   _tol;
        numIter= _maxiter;
        GMRESR(A, x, b, pc_, restart_, numIter, inner_maxiter_, resid, inner_tol_, rel_, method_);
        if (output_ != 0)
            *output_ << "GmresRSolverCL: iterations: " << GetIter()
                     << "\tresidual: " << GetResid() << std::endl;
            std::cout << "GmresRSolverCL: iterations: " << GetIter()
                     << "\tresidual: " << GetResid() << std::endl;
    }
};

// IDR(s)
template <typename PC>
class IDRsSolverCL : public SolverBaseCL
{
  private:
    PC&          pc_;
    const int    s_;
    const double omega_bound_;

  public:
    IDRsSolverCL( PC& pc, int maxiter, double tol, bool relative= true, const int s = 4, const double omega_bound = 0.7, std::ostream* output= 0)
        : SolverBaseCL( maxiter, tol, relative, output), pc_(pc), s_(s), omega_bound_( omega_bound) {}

    PC&       GetPc      ()       { return pc_; }
    const PC& GetPc      () const { return pc_; }

    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b)
    {
        _res=  _tol;
        _iter= _maxiter;
        IDRS(A, x, b, pc_, _iter, _res, rel_, s_, omega_bound_);
        if (output_ != 0)
            *output_ << "IDRsSolverCL: iterations: " << GetIter()
                     << "\tresidual: " << GetResid() << std::endl;
    }
    template <typename Mat, typename Vec>
    void Solve(const Mat& A, Vec& x, const Vec& b, int& numIter, double& resid) const
    {
        resid=   _tol;
        numIter= _maxiter;
        IDRS(A, x, b, pc_, _iter, _res, rel_, s_, omega_bound_);
        if (output_ != 0)
            *output_ << "IDRsSolverCL: iterations: " << GetIter()
                     << "\tresidual: " << GetResid() << std::endl;
    }
};

//=============================================================================
//  Typedefs
//=============================================================================

typedef PreGSCL<P_SSOR>    SSORsmoothCL;
typedef PreGSCL<P_SOR>     SORsmoothCL;
typedef PreGSCL<P_SGS>     SGSsmoothCL;
typedef PreGSCL<P_JOR>     JORsmoothCL;
typedef PreGSCL<P_GS>      GSsmoothCL;
typedef PreGSCL<P_JAC0>    JACPcCL;
typedef PreGSCL<P_SGS0>    SGSPcCL;
typedef PreGSCL<P_SSOR0>   SSORPcCL;
typedef PreGSCL<P_SSOR0_D> SSORDiagPcCL;
typedef PreGSCL<P_GS0>     GSPcCL;

typedef PCGSolverCL<SGSPcCL>      PCG_SgsCL;
typedef PCGSolverCL<SSORPcCL>     PCG_SsorCL;
typedef PCGSolverCL<SSORDiagPcCL> PCG_SsorDiagCL;


//=============================================================================
// Krylov-methods as preconditioner.
// Needed for, e.g., InexactUzawa.
//=============================================================================
/// Wrapper to use a solver as preconditioner. Needed for, e.g., inexact Uzawa.
template <class SolverT>
class SolverAsPreCL: public PreBaseCL
{
  private:
    SolverT& solver_;

  public:
    SolverAsPreCL( SolverT& solver, std::ostream* output= 0)
        : PreBaseCL( output), solver_( solver) {}
    /// return solver object
    SolverT& GetSolver()             { return solver_; }
    /// return solver object
    const SolverT& GetSolver() const { return solver_; }
#ifdef _PAR
    /// \brief Check if return preconditioned vectors are accumulated after calling Apply
    bool RetAcc() const   { return true; }
    /// \brief Check if the diagonal of the matrix is needed
    bool NeedDiag() const { return solver_.GetPC().NeedDiag(); }
    /// \brief Set diagonal to the preconditioner of the solver
    template<typename Mat>
    void SetDiag(const Mat& A)
    {
        solver_.GetPC().SetDiag(A);
    }
#endif

    template <typename Mat, typename Vec>
    void
    Apply(const Mat& A, Vec& x, const Vec& b) const {
        x= 0.0;
        solver_.Solve( A, x, b);
//         if (solver_.GetIter()==solver_.GetMaxIter())
//           IF_MASTER
//             std::cout << "===> Warning: Cannot solve inner system!\n";
        if (output_ != 0)
          IF_MASTER
            *output_ << "SolverAsPreCL: iterations: " << solver_.GetIter()
                     << "\trelative residual: " << solver_.GetResid() << std::endl;
        AddIter( solver_.GetIter());
    }
    void Apply(const MatrixCL& A,   VectorCL& x, const VectorCL& b) const { Apply<>( A, x, b); }
    void Apply(const MLMatrixCL& A, VectorCL& x, const VectorCL& b) const { Apply<>( A, x, b); }
};


//*****************************************************************************
//
//  Non-iterative methods: Gauss solver with pivoting
//
//*****************************************************************************

template<typename Mat, typename Vec>
void
gauss_pivot(Mat& A, Vec& b)
{
    const size_t _Dim = b.size();
    double max;
    size_t ind_max;
    size_t* p = new size_t[_Dim];
    Vec b2(b);
    for (size_t i=0; i<_Dim; ++i) p[i]= i;

    for (size_t i=0; i<_Dim-1; ++i)
    {
        max= std::fabs(A(p[i], i));
        ind_max= i;
        for (size_t l=i+1; l<_Dim; ++l)
            if (std::fabs(A(p[l], i))>max)
            {
                max= std::fabs(A(p[l], i));
                ind_max= l;
            }
        if (max == 0.0) throw DROPSErrCL("gauss_pivot: Matrix is singular.");
        if (i!=ind_max) std::swap(p[i], p[ind_max]);
        const double piv= A(p[i], i);
        for (size_t j=i+1; j<_Dim; ++j)
        {
            const double fac= A(p[j], i)/piv;
            b2[p[j]]-= fac*b2[p[i]];
            for (size_t k=i+1; k<_Dim; ++k)
                A(p[j], k)-= fac* A(p[i], k);
        }
    }

    for (int i=_Dim-1; i>=0; --i)
    {
        for (int j=_Dim-1; j>i; --j)
            b2[p[i]]-= A(p[i], j)*b2[p[j]];
        b2[p[i]]/= A(p[i], i);
        b[i]= b2[p[i]];
    }
    delete[] p;
}

} // end of namespace DROPS

#endif
