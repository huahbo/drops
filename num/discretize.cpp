/// \file discretize.cpp
/// \brief discretizations for several PDEs and FE types
/// \author LNM RWTH Aachen: Patrick Esser, Sven Gross, Trung Hieu Nguyen, Joerg Peters, Volker Reichelt; SC RWTH Aachen:

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

#include "num/discretize.h"
#include "num/interfacePatch.h"
#include "num/fe.h"

namespace DROPS
{
//**************************************************************************
// Class: Quad3DataCL                                                      *
//**************************************************************************

BaryCoordCL Quad3DataCL::Node[NumNodesC];

const double Quad3DataCL::Wght[2]= {
    -2./15., /* -(n+1)^2/[4(n+2)] /6       Node[0]*/
    3./40.,  /* (n+3)^2/[4(n+1)(n+2)] /6 , Node[1] bis Node[4]*/
};

std::valarray<double> Quad3DataCL::P2_Val[10]; // P2_Val[i] contains FE_P2CL::H_i( Node).

Quad3DataCL::Quad3DataCL()
{
    Node[0]= BaryCoordCL( 0.25);
    const double A= 1./6.,
                 B= 0.5;
    Node[1]= MakeBaryCoord( A,A,A,B);
    Node[2]= MakeBaryCoord( A,A,B,A);
    Node[3]= MakeBaryCoord( A,B,A,A);
    Node[4]= MakeBaryCoord( B,A,A,A);

    FE_P2CL::ApplyAll( NumNodesC, Node, P2_Val);
}

BaryCoordCL* Quad3DataCL::TransformNodes (const SArrayCL<BaryCoordCL,4>& M)
{
    BaryCoordCL* tN = new BaryCoordCL[NumNodesC];
    for (Uint i=0; i< NumNodesC; ++i)
        //tN[i]=M*Node[i]; M (als Matrix) ist spaltenweise gespeichert!
        for (Uint k=0; k<4; ++k)
            tN[i][k]= M[0][k]*Node[i][0] + M[1][k]*Node[i][1]
                    + M[2][k]*Node[i][2] + M[3][k]*Node[i][3];
    return tN;
}

namespace {
    Quad3DataCL theQuad3DataInitializer_; // The constructor sets up the static arrays
} // end of anonymous namespace

//**************************************************************************
// Class: Quad5DataCL                                                      *
//**************************************************************************

BaryCoordCL Quad5DataCL::Node[NumNodesC];

const double Quad5DataCL::Wght[4]= {
    0.01975308641975308641975308641975308641975, /* 8./405.,                                   Node[0]*/
    0.01198951396316977000173064248499538621936, /* (2665.0 + 14.0*std::sqrt( 15.0))/226800.0, Node[1] bis Node[4]*/
    0.01151136787104539754677023934921978132914, /* (2665.0 - 14.0*std::sqrt( 15.0))/226800.0, Node[5] bis Node[8]*/
    0.008818342151675485008818342151675485008818 /* 5./567.,                                   Node[9] bis Node[14]*/
};

std::valarray<double> Quad5DataCL::P2_Val[10]; // P2_Val[i] contains FE_P2CL::H_i( Node).

Quad5DataCL::Quad5DataCL()
{
    Node[0]= BaryCoordCL( 0.25);
    const double A1= (7.0 - std::sqrt( 15.0))/34.0,
                 B1= (13.0 + 3.0*std::sqrt( 15.0))/34.0;
    Node[1]= MakeBaryCoord( A1,A1,A1,B1);
    Node[2]= MakeBaryCoord( A1,A1,B1,A1);
    Node[3]= MakeBaryCoord( A1,B1,A1,A1);
    Node[4]= MakeBaryCoord( B1,A1,A1,A1);
    const double A2= (7.0 + std::sqrt( 15.0))/34.0,
                 B2= (13.0 - 3.0*std::sqrt( 15.0))/34.0;
    Node[5]= MakeBaryCoord( A2,A2,A2,B2);
    Node[6]= MakeBaryCoord( A2,A2,B2,A2);
    Node[7]= MakeBaryCoord( A2,B2,A2,A2);
    Node[8]= MakeBaryCoord( B2,A2,A2,A2);
    const double A3= (10.0 - 2.0*std::sqrt( 15.0))/40.0,
                 B3= (10.0 + 2.0*std::sqrt( 15.0))/40.0;
    Node[9] = MakeBaryCoord( A3,A3,B3,B3);
    Node[10]= MakeBaryCoord( A3,B3,A3,B3);
    Node[11]= MakeBaryCoord( A3,B3,B3,A3);
    Node[12]= MakeBaryCoord( B3,A3,A3,B3);
    Node[13]= MakeBaryCoord( B3,A3,B3,A3);
    Node[14]= MakeBaryCoord( B3,B3,A3,A3);

    FE_P2CL::ApplyAll( NumNodesC, Node, P2_Val);
}

BaryCoordCL* Quad5DataCL::TransformNodes (const SArrayCL<BaryCoordCL,4>& M)
{
    BaryCoordCL* tN = new BaryCoordCL[NumNodesC];
    for (Uint i=0; i< NumNodesC; ++i)
        //tN[i]=M*Node[i]; M (als Matrix) ist spaltenweise gespeichert!
        for (Uint k=0; k<4; ++k)
            tN[i][k]= M[0][k]*Node[i][0] + M[1][k]*Node[i][1]
                    + M[2][k]*Node[i][2] + M[3][k]*Node[i][3];
    return tN;
}

namespace {
    Quad5DataCL theQuad5DataInitializer_; // The constructor sets up the static arrays
} // end of anonymous namespace

//**************************************************************************
// Class: Quad5_2DDataCL                                                   *
//**************************************************************************
Point3DCL Quad5_2DDataCL::Node[NumNodesC];  // Barycentric coord for 2D

const double Quad5_2DDataCL::Wght[3]= {
      9./80.,                          /*Node[0]*/
      (155. - std::sqrt( 15.0))/2400., /*Node[1] to Node[3]*/
      (155. + std::sqrt( 15.0))/2400., /*Node[4] to Node[6]*/
};

Quad5_2DDataCL::Quad5_2DDataCL ()
{
    Node[0]= Point3DCL( 1./3.);
    const double A1= (6.0 - std::sqrt( 15.0))/21.0,
    B1= (9.0 + 2.0*std::sqrt( 15.0))/21.0;
    Node[1]= MakePoint3D( A1,A1,B1);
    Node[2]= MakePoint3D( A1,B1,A1);
    Node[3]= MakePoint3D( B1,A1,A1);
    const double A2= (6.0 + std::sqrt( 15.0))/21.0,
    B2= (9.0 - 2.0*std::sqrt( 15.0))/21.0;
    Node[4]= MakePoint3D( A2,A2,B2);
    Node[5]= MakePoint3D( A2,B2,A2);
    Node[6]= MakePoint3D( B2,A2,A2);
}

void
Quad5_2DDataCL::SetInterface (const BaryCoordCL*const p, BaryCoordCL* NodeInTetra)
{
    for (Uint i= 0; i < NumNodesC; ++i)
        NodeInTetra[i]= Node[i][0]*p[0] + Node[i][1]*p[1] + Node[i][2]*p[2];
}

namespace {
    Quad5_2DDataCL theQuad52DDataInitializer_; // The constructor sets up the static arrays
} // end of anonymous namespace


const double Quad3PosWeightsCL::_points[8][3]= {
    {0.,0.,0.}, {1.,0.,0.}, {0.,1.,0.}, {0.,0.,1.},
    {1./3.,1./3.,0.}, {1./3.,0.,1./3.}, {0.,1./3.,1./3.},
    {1./3.,1./3.,1./3.}
    };

const double FaceQuad2CL::_points[4][2]= {
    {0., 0.}, {1., 0.}, {0., 1.}, {1./3., 1./3.} };

const double P1BubbleDiscCL::_points1[26][3]= {
    {0.,0.,0.}, {1./3.,1./3.,0.}, {1./3.,0.,1./3.}, {0.,1./3.,1./3.},
    {1.,0.,0.}, {1./3.,1./3.,0.}, {1./3.,0.,1./3.}, {1./3.,1./3.,1./3.},
    {0.,1.,0.}, {1./3.,1./3.,0.}, {0.,1./3.,1./3.}, {1./3.,1./3.,1./3.},
    {0.,0.,1.}, {1./3.,0.,1./3.}, {0.,1./3.,1./3.}, {1./3.,1./3.,1./3.},
    {0.,0.,0.}, {1.,0.,0.}, {0.,1.,0.}, {0.,0.,1.}, {.5,0.,0.}, {0.,.5,0.}, {0.,0.,.5}, {.5,.5,0.}, {.5,0.,.5}, {0.,.5,.5}
    };

void P2DiscCL::GetGradientsOnRef( LocalP1CL<Point3DCL> GRef[10])
{
    for (int i= 0; i < 10; ++i)
    {
        GRef[i][0]= FE_P2CL::DHRef( i, 0,0,0);
        GRef[i][1]= FE_P2CL::DHRef( i, 1,0,0);
        GRef[i][2]= FE_P2CL::DHRef( i, 0,1,0);
        GRef[i][3]= FE_P2CL::DHRef( i, 0,0,1);
    }
}

void P2DiscCL::GetGradientsOnRef( Quad2CL<Point3DCL> GRef[10])
{
    for (int i=0; i<10; ++i)
    {
        GRef[i][0]= FE_P2CL::DHRef( i, 0,0,0);
        GRef[i][1]= FE_P2CL::DHRef( i, 1,0,0);
        GRef[i][2]= FE_P2CL::DHRef( i, 0,1,0);
        GRef[i][3]= FE_P2CL::DHRef( i, 0,0,1);
        GRef[i][4]= FE_P2CL::DHRef( i, 0.25,0.25,0.25);
    }
}

void P2DiscCL::GetGradientsOnRef( Quad5CL<Point3DCL> GRef[10])
{
    for (int i=0; i<10; ++i)
        for (int j=0; j<Quad5DataCL::NumNodesC; ++j)
        {
            const BaryCoordCL& Node= Quad5DataCL::Node[j];
            GRef[i][j]= FE_P2CL::DHRef( i, Node[1], Node[2], Node[3]);
        }
}

void P2DiscCL::GetGradientsOnRef( Quad5_2DCL<Point3DCL> GRef[10],
    const BaryCoordCL* const p)
{
    BaryCoordCL NodeInTetra[Quad5_2DDataCL::NumNodesC];
    Quad5_2DCL<Point3DCL>::SetInterface( p, NodeInTetra);
    for (int i= 0; i < 10; ++i)
        for (int j= 0; j < Quad5_2DDataCL::NumNodesC; ++j) {
            const BaryCoordCL& Node= NodeInTetra[j];
            GRef[i][j]= FE_P2CL::DHRef( i, Node[1], Node[2], Node[3]);
        }
}

void P2DiscCL::GetP2Basis( Quad5_2DCL<> p2[10], const BaryCoordCL* const p)
{
    BaryCoordCL NodeInTetra[Quad5_2DDataCL::NumNodesC];
    Quad5_2DCL<>::SetInterface( p, NodeInTetra);
    for (int j= 0; j < Quad5_2DDataCL::NumNodesC; ++j) {
        const BaryCoordCL& Node= NodeInTetra[j];
        p2[0][j]= FE_P2CL::H0( Node);
        p2[1][j]= FE_P2CL::H1( Node);
        p2[2][j]= FE_P2CL::H2( Node);
        p2[3][j]= FE_P2CL::H3( Node);
        p2[4][j]= FE_P2CL::H4( Node);
        p2[5][j]= FE_P2CL::H5( Node);
        p2[6][j]= FE_P2CL::H6( Node);
        p2[7][j]= FE_P2CL::H7( Node);
        p2[8][j]= FE_P2CL::H8( Node);
        p2[9][j]= FE_P2CL::H9( Node);
    }
}

void P1DiscCL::GetP1Basis( Quad5_2DCL<> p1[4], const BaryCoordCL* const p)
{
    BaryCoordCL NodeInTetra[Quad5_2DDataCL::NumNodesC];
    Quad5_2DCL<>::SetInterface( p, NodeInTetra);
    for (int j= 0; j < Quad5_2DDataCL::NumNodesC; ++j) {
        const BaryCoordCL& Node= NodeInTetra[j];
        p1[0][j]= FE_P1CL::H0( Node);
        p1[1][j]= FE_P1CL::H1( Node);
        p1[2][j]= FE_P1CL::H2( Node);
        p1[3][j]= FE_P1CL::H3( Node);
    }
}

void P2RidgeDiscCL::GetEnrichmentFunction( LocalP2CL<>& ridgeFunc_p, LocalP2CL<>& ridgeFunc_n, const LocalP2CL<>& lset)
/// initialize ridge enrichment function (to be interpreted as isoP2 function = P1 on child)
{
    for (int v=0; v<4; ++v) {
        const double absval= std::abs(lset[v]);
        ridgeFunc_p[v]= InterfacePatchCL::Sign(lset[v])== 1 ? 0 : 2*absval;
        ridgeFunc_n[v]= InterfacePatchCL::Sign(lset[v])==-1 ? 0 : 2*absval;
    }

    for (int e=0; e<6; ++e) { // linear interpolation of edge values
        const double linInterpolAbs= 0.5*(std::abs(lset[VertOfEdge(e,0)]) + std::abs(lset[VertOfEdge(e,1)]));
        ridgeFunc_p[e+4]= linInterpolAbs - lset[e+4];
        ridgeFunc_n[e+4]= linInterpolAbs + lset[e+4];
    }
}

void P2RidgeDiscCL::GetExtBasisOnChildren( LocalP2CL<> p1ridge_p[4][8], LocalP2CL<> p1ridge_n[4][8], const LocalP2CL<>& lset)
/// returns extended basis functions per child on pos./neg. part (P2 on child)
{
    LocalP2CL<> Fabs_p, Fabs_n;       // enrichment function
    LocalP2CL<> extFabs_p, extFabs_n; // extension of enrichment function from child to parent
    GetEnrichmentFunction( Fabs_p, Fabs_n, lset);
    for (int ch= 0; ch < 8; ++ch) {
        // extend P1 values on child (as Fabs has to be interpreted as isoP2 function) to whole parent
        ExtendP1onChild( Fabs_p, ch, extFabs_p);
        ExtendP1onChild( Fabs_n, ch, extFabs_n);
        for (int i=0; i<4; ++i) { // init extended basis functions: p1r = p1 * Fabs
            LocalP2CL<> &p1r_p= p1ridge_p[i][ch],
                        &p1r_n= p1ridge_n[i][ch];
            // p1_i[i] == 1
            p1r_p[i]= extFabs_p[i];
            p1r_n[i]= extFabs_n[i];
            // p1_i == 0 on opposite face
            const Ubyte oppFace= OppFace(i);
            for (Ubyte j=0; j<3; ++j) {
                const Ubyte vf= VertOfFace(oppFace,j),
                            ef= EdgeOfFace(oppFace,j) + 4;
                p1r_p[vf]= 0;
                p1r_p[ef]= 0;
                p1r_n[vf]= 0;
                p1r_n[ef]= 0;
                // p1_i == 0.5 on edges connecting vert i with opposite face
                const Ubyte e= EdgeByVert(i,vf) + 4;
                p1r_p[e]= 0.5*extFabs_p[e];
                p1r_n[e]= 0.5*extFabs_n[e];
            }
        }
    }
}

void P2RidgeDiscCL::GetExtBasisPointwise( LocalP2CL<> p1ridge_p[4], LocalP2CL<> p1ridge_n[4], const LocalP2CL<>& lset)
/// returns extended basis functions on pos./neg. part (to be interpreted pointwise in P2 degrees of freedom)
{
    LocalP2CL<> Fabs_p, Fabs_n;       // enrichment function
    GetEnrichmentFunction( Fabs_p, Fabs_n, lset);
    for (int i=0; i<4; ++i) { // init extended basis functions: p1r = p1 * Fabs
        LocalP2CL<> &p1r_p= p1ridge_p[i],
                    &p1r_n= p1ridge_n[i];
        // p1_i[i] == 1
        p1r_p[i]= Fabs_p[i];
        p1r_n[i]= Fabs_n[i];
        // p1_i == 0 on opposite face
        const Ubyte oppFace= OppFace(i);
        for (Ubyte j=0; j<3; ++j) {
            const Ubyte vf= VertOfFace(oppFace,j),
                        ef= EdgeOfFace(oppFace,j) + 4;
            p1r_p[vf]= 0;
            p1r_p[ef]= 0;
            p1r_n[vf]= 0;
            p1r_n[ef]= 0;
            // p1_i == 0.5 on edges connecting vert i with opposite face
            const Ubyte e= EdgeByVert(i,vf) + 4;
            p1r_p[e]= 0.5*Fabs_p[e];
            p1r_n[e]= 0.5*Fabs_n[e];
        }
    }
}

} // end of namespace DROPS
