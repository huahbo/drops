/// \file subtriangulation.cpp
/// \brief Triangulation of a principal-lattice of a tetra adapted to a piecewise linear level-set function.
/// \author LNM RWTH Aachen: Joerg Grande; SC RWTH Aachen:

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
 * Copyright 2011 LNM/SC RWTH Aachen, Germany
*/

#include "geom/subtriangulation.h"

namespace DROPS
{

void SortedVertexPolicyCL::sort_vertexes (const GridFunctionCL<>& ls, TetraContT::iterator tetra_begin, TetraContT::iterator tetra_end, size_t)
{
    // Count signs
    Uint num_sign[3]= { 0, 0, 0 }; // num_sign[i] == number of verts with sign i-1
    for (Uint i= 0; i < lattice_num_vertexes_; ++i)
        ++num_sign[sign_plus_one( ls[i])];
    const Uint num_zero_vertexes= num_sign[1] + cut_vertexes_.size();

    vertexes_.resize( num_sign[0] + num_sign[2] + num_zero_vertexes);
    pos_vertex_begin_= num_sign[0];
    neg_vertex_end_=   num_sign[0] + num_zero_vertexes;

    std::vector<Uint> new_pos( num_sign[0] + num_sign[2] + num_zero_vertexes); // maps old vertex-index to the new one
    size_t cursor[3]; // Insertion cursors for the sorted-by-sign vertex numbers
    cursor[0]= 0;
    cursor[1]= num_sign[0];
    cursor[2]= num_sign[0] + num_zero_vertexes;
    for (Uint i= 0; i < lattice_num_vertexes_; ++i) {
        size_t& cur= cursor[sign_plus_one( ls[i])];
        new_pos[i]= cur;
        vertexes_[cur]= lattice_vertex_begin_[i];
        ++cur;
    }
    size_t& cur= cursor[1];
    for (Uint i= 0; i < cut_vertexes_.size(); ++i, ++cur) {
        new_pos[i + lattice_num_vertexes_]= cur;
        vertexes_[cur]= cut_vertexes_[i];
    }
    // Reorder the indices in the tetras
    for (TetraContT::iterator it= tetra_begin; it != tetra_end; ++it)
        for (Uint i= 0; i < 4; ++i)
            (*it)[i]= new_pos[(*it)[i]];
}

void PartitionedVertexPolicyCL::sort_vertexes (const GridFunctionCL<>& ls, TetraContT::iterator tetra_begin, TetraContT::iterator tetra_end, size_t pos_tetra_begin)
{
    // Count signs
    Uint num_sign[3]= { 0, 0, 0 }; // num_sign[i] == number of verts with sign i-1
    for (Uint i= 0; i < lattice_num_vertexes_; ++i)
        ++num_sign[sign_plus_one( ls[i])];
    const Uint num_zero_vertexes= num_sign[1] + cut_vertexes_.size();

    partition_vertexes_.resize( num_sign[0] + num_sign[2] + 2*num_zero_vertexes);
    neg_vertex_end_= pos_vertex_begin_= num_sign[0] + num_zero_vertexes;

    std::vector<Uint> new_pos( num_sign[0] + num_sign[2] + num_zero_vertexes); // maps old vertex-index to the new one
    size_t cursor[3]; // Insertion cursors for the sorted-by-sign vertex numbers
    cursor[0]= 0;
    cursor[1]= num_sign[0];
    cursor[2]= num_sign[0] + 2*num_zero_vertexes;
    for (Uint i= 0; i < lattice_num_vertexes_; ++i) {
        const Uint sign_po= sign_plus_one( ls[i]);
        size_t& cur= cursor[sign_po];
        new_pos[i]= cur;
        partition_vertexes_[cur]= lattice_vertex_begin_[i];
        if (sign_po == 1)
            partition_vertexes_[cur + num_zero_vertexes]= lattice_vertex_begin_[i];
        ++cur;
    }
    size_t& cur= cursor[1];
    for (Uint i= 0; i < cut_vertexes_.size(); ++i, ++cur) {
        new_pos[i + lattice_num_vertexes_]= cur;
        partition_vertexes_[cur + num_zero_vertexes]= partition_vertexes_[cur]= cut_vertexes_[i];
    }
    // Reorder the indices in the tetras
    for (TetraContT::iterator it= tetra_begin; it != tetra_end; ++it) {
        const bool t_is_pos= it >= tetra_begin + pos_tetra_begin;
        for (Uint i= 0; i < 4; ++i) {
            const bool duplicate= t_is_pos && ( (*it)[i] >= lattice_num_vertexes_ || ls[(*it)[i]] == 0.);
            (*it)[i]= new_pos[(*it)[i]] + (duplicate ? num_zero_vertexes : 0);
        }
    }
}


void SurfacePatchCL::partition_principal_lattice (Uint num_intervals, const GridFunctionCL<>& ls)
{
    const PrincipalLatticeCL& lat= PrincipalLatticeCL::instance( num_intervals);
    PrincipalLatticeCL::const_vertex_iterator lattice_verts= lat.vertex_begin();

    triangles_.resize( 0);
    is_boundary_triangle_.resize( 0);
    MergeCutPolicyCL edgecut( lat.vertex_begin(), vertexes_);
    std::vector<Uint> copied_vertexes( lat.num_vertexes(), static_cast<Uint>( -1));

    double lset[4];
    Uint loc_vert_num;
    TriangleT tri;
    for (PrincipalLatticeCL::const_tetra_iterator lattice_tet= lat.tetra_begin(), lattice_end= lat.tetra_end(); lattice_tet != lattice_end; ++lattice_tet) {
        for (Uint i= 0; i < 4; ++i)
            lset[i]= ls[(*lattice_tet)[i]]; 
        const RefTetraSurfacePatchCL& cut= RefTetraSurfacePatchCL::instance( lset);
        if (cut.empty()) continue;
        for (RefTetraSurfacePatchCL::const_triangle_iterator it= cut.triangle_begin(), end= cut.triangle_end(); it != end; ++it) {
            for (Uint j= 0; j < 3; ++j) {
                loc_vert_num= (*it)[j];
                if (loc_vert_num < 4) {
                    const Uint lattice_vert_num= (*lattice_tet)[loc_vert_num];
                    if (copied_vertexes[lattice_vert_num] == static_cast<Uint>( -1)) {
                        vertexes_.push_back( lattice_verts[lattice_vert_num]);
                        copied_vertexes[lattice_vert_num]= vertexes_.size() - 1;
                    }
                    tri[j]= copied_vertexes[lattice_vert_num];
                }
                else { // Cut vertex
                    const Ubyte v0= VertOfEdge( loc_vert_num - 4, 0),
                                v1= VertOfEdge( loc_vert_num - 4, 1);
                    tri[j]= edgecut( (*lattice_tet)[v0], (*lattice_tet)[v1], lset[v0], lset[v1]);
                }
            }
            triangles_.push_back( tri);
            is_boundary_triangle_.push_back( cut.is_boundary_triangle());
        }
    }
}

void write_paraview_vtu (std::ostream& file_, const SurfacePatchCL& t)
{
    file_ << "<?xml version=\"1.0\"?>"  << '\n'
          << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">"   << '\n'
          << "<UnstructuredGrid>"   << '\n';

    file_<< "<Piece NumberOfPoints=\""<< t.vertexes_.size() <<"\" NumberOfCells=\""<< t.triangles_.size() << "\">";
    file_<< "\n\t<Points>"
         << "\n\t\t<DataArray type=\"Float32\" NumberOfComponents=\"3\" format=\"" << "ascii\">\n\t\t";
    for(SurfacePatchCL::const_vertex_iterator it= t.vertexes_.begin(), end= t.vertexes_.end(); it != end; ++it) {
        file_ << it[0][1] << ' ' << it[0][2] << ' ' << it[0][3] << ' ';
    }
    file_<< "\n\t\t</DataArray> \n"
         << "\t</Points>\n";

    file_   << "\t<Cells>\n"
            << "\t\t<DataArray type=\"Int32\" Name=\"connectivity\" format=\""
            <<"ascii\">\n\t\t";
    std::copy( t.triangles_.begin(), t.triangles_.end(), std::ostream_iterator<LatticePartitionTypesNS::TriangleT>( file_));
    file_ << "\n\t\t</DataArray>\n";
    file_ << "\t\t<DataArray type=\"Int32\" Name=\"offsets\" format=\"ascii\">\n\t\t";
    for(Uint i= 1; i <= t.triangles_.size(); ++i) {
        file_ << i*3 << " ";
    }
    file_ << "\n\t\t</DataArray>";
    file_ << "\n\t\t<DataArray type=\"UInt8\" Name=\"types\" format=\"ascii\">\n\t\t";
    const int Type= 5; // Triangles
    for(Uint i= 1; i <= t.triangles_.size(); ++i) {
            file_ << Type<<" ";
    }
    file_<<"\n\t\t</DataArray>"
         <<"\n\t</Cells>";

    file_ <<"\n</Piece>"
          <<"\n</UnstructuredGrid>"
          <<"\n</VTKFile>";
}


} // end of namespace DROPS