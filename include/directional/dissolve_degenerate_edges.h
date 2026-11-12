// This file is part of Directional, a library for directional field processing.
// Copyright (C) 2026 Amir Vaxman <avaxman@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.

#ifndef DIRECTIONAL_DISSOLVE_DEGENERATE_EDGES_H
#define DIRECTIONAL_DISSOLVE_DEGENERATE_EDGES_H

#include <algorithm>
#include <array>
#include <set>
#include <vector>
#include <Eigen/Core>
#include <directional/TriMesh.h>

namespace directional
{

/***
 Dissolves (collapses) degenerate edges: edges whose length is below ratio * L_median,
 where L_median is the median edge length of the current mesh.

 Collapse rule: if one of the two edge vertices is on the boundary, the other vertex is
 merged INTO the boundary vertex (the boundary node keeps its exact position, no averaging).
 Otherwise the two vertices are merged at their midpoint. Faces that become degenerate
 (repeated indices) after a collapse are deleted.

 A collapse is only performed if it satisfies the link condition (every common neighbor of
 the two endpoints is opposite the edge in an incident face); otherwise the edge is left in
 place, so the result always stays a manifold. Degenerate edges that cannot be collapsed
 safely are therefore skipped rather than producing a non-manifold mesh.

 The whole mesh is rebuilt at the end (set_mesh), so vertex/face indices change: any field
 data computed on the mesh beforehand is invalidated. Run this before computing fields.

 Returns the total number of collapsed edges.
 ***/
inline int dissolve_degenerate_edges(TriMesh& mesh,
                                     const double ratio = 0.1,
                                     const int maxRounds = 10)
{
    int totalCollapsed = 0;
    for (int round = 0; round < maxRounds; round++) {
        const int E = mesh.EV.rows();
        if (E == 0) break;

        //current edge lengths and their median
        Eigen::VectorXd L(E);
        for (int e = 0; e < E; e++)
            L(e) = (mesh.V.row(mesh.EV(e,0)) - mesh.V.row(mesh.EV(e,1))).norm();
        std::vector<double> sortedL(L.data(), L.data() + E);
        std::sort(sortedL.begin(), sortedL.end());
        const double Lmedian = (E % 2 == 1) ? sortedL[E/2]
                                            : 0.5 * (sortedL[E/2 - 1] + sortedL[E/2]);

        //degenerate edges, shortest first
        std::vector<int> degenerate;
        for (int e = 0; e < E; e++)
            if (L(e) < ratio * Lmedian)
                degenerate.push_back(e);
        if (degenerate.empty())
            break;
        std::sort(degenerate.begin(), degenerate.end(),
                  [&](const int a, const int b){ return L(a) < L(b); });

        //sequential collapse with a remap (kept index) per vertex
        std::vector<int> remap(mesh.V.rows());
        for (int i = 0; i < (int)remap.size(); i++)
            remap[i] = i;

        //set of faces (sorted, remapped vertex triples) for the link condition,
        //rebuilt after every collapse since the remap changes
        auto sortedTriple = [](int x, int y, int z) {
            std::array<int,3> t = {x, y, z};
            std::sort(t.begin(), t.end());
            return t;
        };
        std::set<std::array<int,3>> faceSet;
        auto rebuildFaceSet = [&]() {
            faceSet.clear();
            for (int f = 0; f < mesh.F.rows(); f++)
                faceSet.insert(sortedTriple(remap[mesh.F(f,0)], remap[mesh.F(f,1)], remap[mesh.F(f,2)]));
        };
        rebuildFaceSet();

        //neighbors of a (remapped) vertex through its incident edges
        auto neighbors = [&](const int v, std::vector<int>& out) {
            out.clear();
            for (int k = 0; k < mesh.vertexValence(v); k++) {
                const int e = mesh.VE(v, k);
                const int o = (mesh.EV(e,0) == v) ? mesh.EV(e,1) : mesh.EV(e,0);
                out.push_back(remap[o]);
            }
        };

        int collapsedThisRound = 0;
        std::vector<int> na, nb;
        for (const int e : degenerate) {
            const int a = remap[mesh.EV(e,0)];
            const int b = remap[mesh.EV(e,1)];
            if (a == b)
                continue; //already merged through a chain of collapses

            //link condition: every common neighbor of a and b must be opposite the
            //edge (a,b) in an incident face, otherwise the collapse is skipped
            neighbors(a, na);
            neighbors(b, nb);
            bool linkOk = true;
            for (const int c : na) {
                if (c == a || c == b)
                    continue;
                if (std::find(nb.begin(), nb.end(), c) != nb.end() &&
                    faceSet.find(sortedTriple(a, b, c)) == faceSet.end()) {
                    linkOk = false;
                    break;
                }
            }
            if (!linkOk)
                continue;

            int keep, remove;
            if (mesh.isBoundaryVertex(a)) {
                //merge b into the boundary vertex a: position of a stays fixed
                keep = a; remove = b;
            } else if (mesh.isBoundaryVertex(b)) {
                //merge a into the boundary vertex b: position of b stays fixed
                keep = b; remove = a;
            } else {
                //interior edge: merge at the midpoint
                keep = a; remove = b;
                mesh.V.row(keep) = 0.5 * (mesh.V.row(a) + mesh.V.row(b));
            }
            remap[remove] = keep;
            collapsedThisRound++;
            rebuildFaceSet();
        }
        if (collapsedThisRound == 0)
            break;
        totalCollapsed += collapsedThisRound;

        //apply the remap to the faces, dropping degenerate faces
        std::vector<std::array<int,3>> newF;
        newF.reserve(mesh.F.rows());
        for (int f = 0; f < mesh.F.rows(); f++) {
            std::array<int,3> nf = {remap[mesh.F(f,0)], remap[mesh.F(f,1)], remap[mesh.F(f,2)]};
            if (nf[0] == nf[1] || nf[1] == nf[2] || nf[0] == nf[2])
                continue;
            newF.push_back(nf);
        }

        //compact V: drop unreferenced vertices and reindex the faces
        const int nOld = mesh.V.rows();
        Eigen::MatrixXd newV;
        std::vector<int> compactMap(nOld, -1);
        int nNew = 0;
        for (const auto& nf : newF)
            for (int k = 0; k < 3; k++)
                if (compactMap[nf[k]] == -1)
                    compactMap[nf[k]] = nNew++;
        newV.resize(nNew, 3);
        for (int i = 0; i < nOld; i++)
            if (compactMap[i] != -1)
                newV.row(compactMap[i]) = mesh.V.row(i);

        Eigen::MatrixXi Fnew((int)newF.size(), 3);
        for (int f = 0; f < (int)newF.size(); f++)
            Fnew.row(f) << compactMap[newF[f][0]], compactMap[newF[f][1]], compactMap[newF[f][2]];

        //rebuild the full structure on a FRESH TriMesh and swap it in: re-running
        //dcel.init() on an already-initialized DCEL leaves stale twin pointers
        //(init() never resets twin to -1), which corrupts the boundary loops
        TriMesh rebuilt;
        rebuilt.set_mesh(newV, Fnew); //also runs the DCEL consistency check
        mesh = rebuilt;
    }
    return totalCollapsed;
}

}

#endif
