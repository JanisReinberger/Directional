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
#include <map>
#include <set>
#include <utility>
#include <vector>
#include <Eigen/Core>
#include <directional/TriMesh.h>
#include <directional/median_edge_length.h>

namespace directional
{

//Result of the pure (V, F, eps) / (V, D, F, eps) overloads.
struct DegenerateEdgeDissolveResult {
    Eigen::MatrixXd V;         //positions after merging
    Eigen::MatrixXi F;          //repaired faces (unchanged geometry if nothing was merged)
    Eigen::VectorXi D;         //polygon sizes (empty for the triangle overload)
    Eigen::VectorXi vertexMap; //old -> new vertex index (empty when nothing was merged)
    int collapsedEdges = 0;    //degenerate edges actually collapsed
    int mergedVertices = 0;    //vertices removed by merging
    int removedFaces = 0;      //faces dropped (fewer than 3 corners left)
};

namespace dissolve_degenerate_edges_detail {

struct DissolveCoreResult {
    Eigen::MatrixXd V;                   //positions per OLD vertex (cluster position spread to members)
    std::vector<std::vector<int>> faces; //repaired corner cycles
    std::vector<int> vertexMap;          //old -> new vertex index
    int collapsedEdges = 0;
    int mergedVertices = 0;
    int removedFaces = 0;
    bool changed = false;
};

/***
 Core dissolution on faces as vertex-index cycles. Every edge (consecutive
 corner pair, with wrap) with length <= eps is collapsed, shortest first:
 - if one endpoint is a boundary vertex (incident to an edge with exactly one
   face), the other vertex is merged INTO it and the boundary vertex keeps its
   exact position (the boundary shape is preserved, no averaging);
 - otherwise the two vertices are merged at their midpoint (sequential rule:
   the midpoint is computed from the current, possibly already-merged
   positions).
 A collapse is only performed if it satisfies the link condition (every common
 neighbour of the two endpoints co-occurs with both in an incident face);
 otherwise the edge is skipped, so the result stays manifold. Faces are
 repaired by dropping cyclically-consecutive repeated corners; faces that keep
 fewer than 3 corners are removed. All merge clusters survive in the output
 (no compaction of unreferenced vertices): vertexMap is total, like the
 original Remesher rule. eps <= 0 disables the step (unchanged, vertexMap empty).
 ***/
inline DissolveCoreResult dissolve_impl(const Eigen::MatrixXd& V,
                                        const std::vector<std::vector<int>>& faces,
                                        const double eps)
{
    DissolveCoreResult result;
    result.faces = faces;
    const int n = static_cast<int>(V.rows());
    if (n == 0 || eps <= 0.0) return result;

    //edge -> incident faces, vertex neighbourhoods (sorted, deduplicated)
    std::map<std::pair<int, int>, std::vector<int>> edgeFaces;
    std::vector<std::vector<int>> nbrs(n);
    for (int f = 0; f < static_cast<int>(faces.size()); ++f) {
        const std::vector<int>& poly = faces[f];
        const int m = static_cast<int>(poly.size());
        if (m < 3) continue;
        std::vector<std::pair<int, int>> faceEdges;
        for (int c = 0; c < m; ++c) {
            const int a = poly[c];
            const int b = poly[(c + 1) % m];
            if (a < 0 || b < 0 || a >= n || b >= n || a == b) continue;
            const std::pair<int, int> key = std::minmax(a, b);
            if (std::find(faceEdges.begin(), faceEdges.end(), key) != faceEdges.end()) continue;
            faceEdges.push_back(key);
        }
        for (const std::pair<int, int>& key : faceEdges) {
            nbrs[key.first].push_back(key.second);
            nbrs[key.second].push_back(key.first);
            edgeFaces[key].push_back(f);
        }
    }
    for (int v = 0; v < n; ++v) {
        std::vector<int>& list = nbrs[v];
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
    }

    //boundary vertices: endpoints of edges with exactly one incident face
    std::vector<char> isBoundary(n, 0);
    for (const auto& entry : edgeFaces)
        if (entry.second.size() == 1) {
            isBoundary[entry.first.first] = 1;
            isBoundary[entry.first.second] = 1;
        }

    //degenerate edges (length <= eps), shortest first
    std::vector<std::pair<double, std::pair<int, int>>> degenerate;
    for (const auto& entry : edgeFaces) {
        const double length = (V.row(entry.first.first) - V.row(entry.first.second)).norm();
        if (length <= eps)
            degenerate.push_back({length, entry.first});
    }
    if (degenerate.empty()) return result;
    std::sort(degenerate.begin(), degenerate.end());

    //sequential collapse with a remap (kept index) per vertex
    std::vector<int> remap(n);
    for (int v = 0; v < n; ++v) remap[v] = v;
    Eigen::MatrixXd P = V; //working positions (midpoint updates land here)

    //set of faces (sorted, remapped corner sets) for the link condition,
    //rebuilt after every collapse since the remap changes
    std::set<std::vector<int>> faceSet;
    auto remappedFaceCorners = [&](const std::vector<int>& poly) {
        std::vector<int> mapped;
        mapped.reserve(poly.size());
        for (const int corner : poly) {
            const int c = (corner >= 0 && corner < n) ? remap[corner] : corner;
            if (mapped.empty() || mapped.back() != c) mapped.push_back(c);
        }
        while (mapped.size() > 1 && mapped.front() == mapped.back()) mapped.pop_back();
        return mapped;
    };
    auto rebuildFaceSet = [&]() {
        faceSet.clear();
        for (const std::vector<int>& poly : faces) {
            std::vector<int> mapped = remappedFaceCorners(poly);
            if (mapped.size() >= 3) {
                std::sort(mapped.begin(), mapped.end());
                faceSet.insert(std::move(mapped));
            }
        }
    };
    rebuildFaceSet();

    auto remappedNbrs = [&](const int v) {
        std::vector<int> out;
        for (const int c : nbrs[v]) out.push_back(remap[c]);
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    };

    int collapsed = 0;
    for (const auto& entry : degenerate) {
        const int a = remap[entry.second.first];
        const int b = remap[entry.second.second];
        if (a == b)
            continue; //already merged through a chain of collapses

        //link condition: every common neighbour of a and b must co-occur
        //with both in an incident face, otherwise the collapse is skipped
        const std::vector<int> na = remappedNbrs(a);
        const std::vector<int> nb = remappedNbrs(b);
        bool linkOk = true;
        for (const int cc : na) {
            if (cc == a || cc == b)
                continue;
            if (!std::binary_search(nb.begin(), nb.end(), cc))
                continue; //not a common neighbour
            std::vector<int> probe = {a, b, cc};
            std::sort(probe.begin(), probe.end());
            if (faceSet.find(probe) == faceSet.end()) {
                linkOk = false;
                break;
            }
        }
        if (!linkOk)
            continue;

        int keep, remove;
        if (isBoundary[a]) {
            //merge b into the boundary vertex a: position of a stays fixed
            keep = a; remove = b;
        } else if (isBoundary[b]) {
            //merge a into the boundary vertex b: position of b stays fixed
            keep = b; remove = a;
        } else {
            //interior edge: merge at the (sequential) midpoint
            keep = a; remove = b;
            P.row(keep) = 0.5 * (P.row(a) + P.row(b));
        }
        remap[remove] = keep;
        ++collapsed;
        rebuildFaceSet();
    }
    result.collapsedEdges = collapsed;
    if (collapsed == 0) return result;
    result.changed = true;

    //cluster ids in ascending order of their smallest member (deterministic)
    std::vector<int> root(n);
    for (int v = 0; v < n; ++v) {
        int r = v;
        while (remap[r] != r) r = remap[r];
        root[v] = r;
    }
    std::vector<int> minMemberOfRoot(n, n);
    for (int v = 0; v < n; ++v)
        if (v < minMemberOfRoot[root[v]]) minMemberOfRoot[root[v]] = v;
    std::vector<std::pair<int, int>> roots; // (smallest member, root)
    for (int v = 0; v < n; ++v)
        if (minMemberOfRoot[v] != n) roots.push_back({minMemberOfRoot[v], v});
    std::sort(roots.begin(), roots.end());
    std::vector<int> clusterOfRoot(n, -1);
    for (int i = 0; i < static_cast<int>(roots.size()); ++i)
        clusterOfRoot[roots[i].second] = i;
    const int clusterCount = static_cast<int>(roots.size());
    result.mergedVertices = n - clusterCount;

    result.vertexMap.resize(n);
    for (int v = 0; v < n; ++v) result.vertexMap[v] = clusterOfRoot[root[v]];

    //spread every cluster's (possibly midpoint-moved) position to its members
    result.V = P;
    for (int v = 0; v < n; ++v) result.V.row(v) = P.row(root[v]);

    //repair faces: map the corners through vertexMap (cluster ids), drop
    //cyclically-consecutive repeated corners, drop faces that keep fewer than
    //3 distinct corners
    result.faces.clear();
    result.faces.reserve(faces.size());
    for (const std::vector<int>& poly : faces) {
        std::vector<int> mapped;
        mapped.reserve(poly.size());
        for (const int corner : poly) {
            const int c = (corner >= 0 && corner < n) ? result.vertexMap[corner] : corner;
            if (mapped.empty() || mapped.back() != c) mapped.push_back(c);
        }
        while (mapped.size() > 1 && mapped.front() == mapped.back()) mapped.pop_back();
        if (mapped.size() < 3) {
            ++result.removedFaces;
            continue;
        }
        result.faces.push_back(std::move(mapped));
    }
    return result;
}

} //namespace dissolve_degenerate_edges_detail

/***
 Topological clean-up for the smoothing: every edge with length <= eps is
 dissolved (see dissolve_impl). Triangle overload (V, F, eps): faces stay
 triangles. Polygonal overload (V, D, F, eps): face f uses only the first D(f)
 columns; repaired faces can shrink (e.g. quads become triangles), D is
 rebuilt accordingly and F padded with 0. When nothing was merged, the inputs
 are returned unchanged and vertexMap is empty.
 ***/
inline DegenerateEdgeDissolveResult dissolve_degenerate_edges(const Eigen::MatrixXd& V,
                                                               const Eigen::MatrixXi& F,
                                                               const double eps)
{
    DegenerateEdgeDissolveResult result;
    std::vector<std::vector<int>> faces(F.rows());
    for (int f = 0; f < F.rows(); ++f)
        faces[f] = {F(f,0), F(f,1), F(f,2)};
    const dissolve_degenerate_edges_detail::DissolveCoreResult core =
        dissolve_degenerate_edges_detail::dissolve_impl(V, faces, eps);
    result.collapsedEdges = core.collapsedEdges;
    result.mergedVertices = core.mergedVertices;
    result.removedFaces = core.removedFaces;
    if (!core.changed) {
        result.V = V;
        result.F = F;
        return result;
    }
    const int clusterCount = static_cast<int>(V.rows()) - core.mergedVertices;
    result.V.resize(clusterCount, 3);
    for (int v = 0; v < static_cast<int>(V.rows()); ++v)
        result.V.row(core.vertexMap[v]) = core.V.row(v);
    result.vertexMap = Eigen::Map<const Eigen::VectorXi>(core.vertexMap.data(), core.vertexMap.size());

    result.F.resize(static_cast<int>(core.faces.size()), 3);
    for (int f = 0; f < static_cast<int>(core.faces.size()); ++f)
        result.F.row(f) << core.faces[f][0], core.faces[f][1], core.faces[f][2];
    return result;
}

inline DegenerateEdgeDissolveResult dissolve_degenerate_edges(const Eigen::MatrixXd& V,
                                                               const Eigen::VectorXi& D,
                                                               const Eigen::MatrixXi& F,
                                                               const double eps)
{
    DegenerateEdgeDissolveResult result;
    std::vector<std::vector<int>> faces(F.rows());
    for (int f = 0; f < F.rows(); ++f) {
        const int m = (f < D.size()) ? D(f) : F.cols();
        if (m < 3) { faces[f] = {}; continue; }
        faces[f].reserve(m);
        for (int c = 0; c < m && c < F.cols(); ++c)
            faces[f].push_back(F(f, c));
    }
    const dissolve_degenerate_edges_detail::DissolveCoreResult core =
        dissolve_degenerate_edges_detail::dissolve_impl(V, faces, eps);
    result.collapsedEdges = core.collapsedEdges;
    result.mergedVertices = core.mergedVertices;
    result.removedFaces = core.removedFaces;
    if (!core.changed) {
        result.V = V;
        result.F = F;
        result.D = D;
        return result;
    }
    const int clusterCount = static_cast<int>(V.rows()) - core.mergedVertices;
    result.V.resize(clusterCount, 3);
    for (int v = 0; v < static_cast<int>(V.rows()); ++v)
        result.V.row(core.vertexMap[v]) = core.V.row(v);
    result.vertexMap = Eigen::Map<const Eigen::VectorXi>(core.vertexMap.data(), core.vertexMap.size());

    const int faceCount = static_cast<int>(core.faces.size());
    const int columns = std::max(3, static_cast<int>(F.cols()));
    result.D.resize(faceCount);
    result.F = Eigen::MatrixXi::Zero(faceCount, columns);
    for (int f = 0; f < faceCount; ++f) {
        const std::vector<int>& poly = core.faces[f];
        result.D(f) = static_cast<int>(poly.size());
        for (int c = 0; c < static_cast<int>(poly.size()) && c < columns; ++c)
            result.F(f, c) = poly[c];
    }
    return result;
}

/***
 In-place TriMesh variant: dissolves edges with length <= ratio * L_median
 (L_median recomputed on the current mesh every round), iterating until no
 collapsible degenerate edge remains or maxRounds is hit. The mesh is rebuilt
 on a FRESH TriMesh and swapped in (re-running dcel.init() on an
 already-initialized DCEL is only safe once the re-initialization fix lands);
 unreferenced vertices are compacted away. Returns the total number of
 collapsed edges. Vertex/face indices change: any field data computed on the
 mesh beforehand is invalidated -- run this before computing fields. The
 default ratio follows the library spec; the Remesher app passes 1e-3
 (mesher artifacts) explicitly.
 ***/
inline int dissolve_degenerate_edges(TriMesh& mesh,
                                     const double ratio = 0.1,
                                     const int maxRounds = 10)
{
    int totalCollapsed = 0;
    for (int round = 0; round < maxRounds; round++) {
        const double eps = ratio * median_edge_length(mesh.V, mesh.F);
        const DegenerateEdgeDissolveResult dissolved =
            dissolve_degenerate_edges(mesh.V, mesh.F, eps);
        if (!dissolved.mergedVertices)
            break;
        totalCollapsed += dissolved.collapsedEdges;

        //compact V: drop unreferenced clusters and reindex the faces.
        //dissolved.F corners and dissolved.V rows are indexed by CLUSTER id,
        //so the compaction map is keyed by cluster id as well.
        const int clusterCount = static_cast<int>(dissolved.V.rows());
        std::vector<int> compactMap(clusterCount, -1);
        int nNew = 0;
        for (int f = 0; f < dissolved.F.rows(); ++f)
            for (int k = 0; k < 3; ++k)
                if (compactMap[dissolved.F(f,k)] == -1)
                    compactMap[dissolved.F(f,k)] = nNew++;
        Eigen::MatrixXd newV(nNew, 3);
        for (int c = 0; c < clusterCount; ++c)
            if (compactMap[c] != -1)
                newV.row(compactMap[c]) = dissolved.V.row(c);
        Eigen::MatrixXi Fnew = dissolved.F;
        for (int f = 0; f < Fnew.rows(); ++f)
            for (int k = 0; k < 3; ++k)
                Fnew(f,k) = compactMap[Fnew(f,k)];

        //rebuild the full structure on a FRESH TriMesh and swap it in
        TriMesh rebuilt;
        rebuilt.set_mesh(newV, Fnew); //also runs the DCEL consistency check
        mesh = rebuilt;
    }
    return totalCollapsed;
}

}

#endif
