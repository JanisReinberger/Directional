// This file is part of Directional, a library for directional field processing.
// Copyright (C) 2026 Amir Vaxman <avaxman@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.

#ifndef DIRECTIONAL_SMOOTH_MESH_H
#define DIRECTIONAL_SMOOTH_MESH_H

#include <algorithm>
#include <vector>
#include <Eigen/Core>
#include <directional/TriMesh.h>
#include <directional/dissolve_degenerate_edges.h>

namespace directional
{

//Normalization mode for the per-edge force strength (weight) in smooth_mesh
enum class EdgeForceMode {
    ClampUp,     //w_ij = min(L_ij, L_median): long edges capped at the median strength
    ClampUpDown  //w_ij = L_median for every edge: uniform Laplacian weights
};

/***
 Iterative boundary-preserving mesh smoothing. Every edge enacts a "force" on its
 endpoints, pulling them toward each other:

     F_i = sum_j w_ij * (v_j - v_i),   v_i <- v_i + strength * F_i / sum_j w_ij

 with the edge weight w_ij set according to the EdgeForceMode (see above), where
 L_median is the median edge length. All forces of one iteration are computed from the
 same positions (Jacobi-style update). Boundary vertices (isBoundaryVertex == 1) are
 pinned: they accumulate forces but never move, preserving the boundary shape.

 If dissolveDegenerate is true, degenerate edges (L < 0.1 * L_median) are dissolved
 first via dissolve_degenerate_edges() -- note this changes vertex/face indexing.

 strength is clamped to [0,1] (values > 1 can fold triangles over). Edge weights are
 computed once per call from the initial positions. The smoothing is a plain
 (rest-length-zero) Laplacian, so it shrinks the surface slightly -- expected.
 ***/
inline void smooth_mesh(TriMesh& mesh,
                        const double strength,
                        const int numIterations,
                        const EdgeForceMode mode = EdgeForceMode::ClampUp,
                        const bool dissolveDegenerate = true)
{
    if (dissolveDegenerate)
        dissolve_degenerate_edges(mesh);

    const int E = mesh.EV.rows();
    const int n = mesh.V.rows();
    if (E == 0 || n == 0)
        return;

    //edge lengths and their median (computed once, after any dissolution)
    Eigen::VectorXd L(E);
    for (int e = 0; e < E; e++)
        L(e) = (mesh.V.row(mesh.EV(e,0)) - mesh.V.row(mesh.EV(e,1))).norm();
    std::vector<double> sortedL(L.data(), L.data() + E);
    std::sort(sortedL.begin(), sortedL.end());
    const double Lmedian = (E % 2 == 1) ? sortedL[E/2]
                                        : 0.5 * (sortedL[E/2 - 1] + sortedL[E/2]);

    const double s = std::clamp(strength, 0.0, 1.0);

    Eigen::MatrixXd Facc = Eigen::MatrixXd::Zero(n, 3);
    Eigen::VectorXd W = Eigen::VectorXd::Zero(n);
    Eigen::MatrixXd Vnew(n, 3);

    for (int it = 0; it < numIterations; it++) {
        //accumulate all forces from the current positions (Jacobi)
        Facc.setZero();
        W.setZero();
        for (int e = 0; e < E; e++) {
            const int a = mesh.EV(e,0);
            const int b = mesh.EV(e,1);
            const double w = (mode == EdgeForceMode::ClampUpDown) ? Lmedian
                                                                  : std::min(L(e), Lmedian);
            if (w <= 0)
                continue;
            const Eigen::RowVector3d d = mesh.V.row(b) - mesh.V.row(a);
            Facc.row(a) += w * d;
            W(a) += w;
            Facc.row(b) -= w * d;
            W(b) += w;
        }
        //apply: boundary vertices stay fixed
        Vnew = mesh.V;
        for (int i = 0; i < n; i++) {
            if (mesh.isBoundaryVertex(i) || W(i) <= 0)
                continue;
            Vnew.row(i) += s * Facc.row(i) / W(i);
        }
        mesh.V = Vnew;
    }

    //topology is unchanged; only geometric quantities need a refresh
    mesh.compute_geometric_quantities();
}

}

#endif
