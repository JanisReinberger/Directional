// This file is part of Directional, a library for directional field processing.
// Copyright (C) 2026 Amir Vaxman <avaxman@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.

#ifndef DIRECTIONAL_SMOOTH_MESH_H
#define DIRECTIONAL_SMOOTH_MESH_H

#include <algorithm>
#include <map>
#include <utility>
#include <vector>
#include <Eigen/Core>
#include <directional/TriMesh.h>
#include <directional/dissolve_degenerate_edges.h>

namespace directional
{

//Treatment of the edge forces between pinned (boundary / non-manifold) and
//interior vertices in smooth_mesh. The base update is always the plain
//uniform Laplacian; the modes only rescale the forces that pinned neighbours
//exert on interior vertices (direction unchanged).
enum class BoundaryForceMode {
    Uniform,   //shipped rule: every edge contributes its plain edge vector
    Median,    //forces from pinned neighbours are rescaled to L_median (both ways)
    Clamped    //only shorter-than-L_median forces from pinned neighbours are scaled up
};

struct SmoothingResult {
    Eigen::MatrixXd V;   //smoothed positions
    int pinned = 0;      //vertices held fixed (boundary / non-manifold / isolated)
    int moved = 0;       //vertices whose position changed
};

namespace smooth_mesh_detail {

//faces: vertex-index cycles with >= 3 corners each. This is the verified
//"internal smoothing" rule of the Remesher (see apps/Remesher/Internal_Smoothing.md):
//uniform Jacobi Laplacian that moves only interior vertices; pinned vertices
//keep their position but still contribute to their neighbours' averages.
inline SmoothingResult smooth_mesh_impl(const Eigen::MatrixXd& V,
                                        const std::vector<std::vector<int>>& faces,
                                        double strength,
                                        int iterations,
                                        const BoundaryForceMode boundaryMode)
{
    SmoothingResult result;
    const int n = static_cast<int>(V.rows());
    result.V = V;
    if (n == 0 || iterations < 1) return result;
    strength = std::min(1.0, std::max(0.0, strength));

    //One pass over the faces: neighbourhoods, edge->face and vertex->face
    //incidence (edges and faces are deduplicated per face, so a degenerate
    //face does not count as a second incident face).
    std::map<std::pair<int, int>, std::vector<int>> edgeFaces;
    std::vector<std::vector<int>> nbrs(n);
    std::vector<std::vector<int>> vertFaces(n);
    std::vector<std::vector<std::pair<int, int>>> vertEdgeKeys(n);
    for (int f = 0; f < static_cast<int>(faces.size()); ++f) {
        const std::vector<int>& poly = faces[f];
        const int m = static_cast<int>(poly.size());
        if (m < 3) continue;
        for (int c = 0; c < m; ++c) {
            const int v = poly[c];
            if (v >= 0 && v < n) vertFaces[v].push_back(f);
        }
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
            vertEdgeKeys[key.first].push_back(key);
            vertEdgeKeys[key.second].push_back(key);
        }
    }

    //Deterministic neighbour order (sorted, deduplicated) -- the summation
    //order is load-bearing for reproducible results.
    for (int v = 0; v < n; ++v) {
        std::vector<int>& list = nbrs[v];
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
        std::vector<int>& incident = vertFaces[v];
        std::sort(incident.begin(), incident.end());
        incident.erase(std::unique(incident.begin(), incident.end()), incident.end());
        std::vector<std::pair<int, int>>& keys = vertEdgeKeys[v];
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    }

    //Pin every endpoint of an edge that does not have exactly two incident
    //faces (boundary => 1, non-manifold => > 2).
    std::vector<char> pinned(n, 0);
    for (const auto& entry : edgeFaces) {
        if (entry.second.size() != 2) {
            pinned[entry.first.first] = 1;
            pinned[entry.first.second] = 1;
        }
    }

    //Pin vertices whose incident faces do not form a single fan: two incident
    //faces belong to the same fan when they share an edge with exactly two
    //incident faces. Vertices without incident faces are pinned as well.
    std::vector<int> parent;
    for (int v = 0; v < n; ++v) {
        const std::vector<int>& incident = vertFaces[v];
        if (incident.empty()) {
            pinned[v] = 1;
            continue;
        }
        if (incident.size() < 2) continue;
        parent.resize(incident.size());
        for (int i = 0; i < static_cast<int>(parent.size()); ++i) parent[i] = i;
        auto findRoot = [&parent](int x) {
            while (parent[x] != x) {
                parent[x] = parent[parent[x]];
                x = parent[x];
            }
            return x;
        };
        for (const std::pair<int, int>& key : vertEdgeKeys[v]) {
            const auto it = edgeFaces.find(key);
            if (it == edgeFaces.end() || it->second.size() != 2) continue;
            const auto pos0 = std::find(incident.begin(), incident.end(), it->second[0]);
            const auto pos1 = std::find(incident.begin(), incident.end(), it->second[1]);
            if (pos0 == incident.end() || pos1 == incident.end()) continue;
            const int root0 = findRoot(static_cast<int>(pos0 - incident.begin()));
            const int root1 = findRoot(static_cast<int>(pos1 - incident.begin()));
            if (root0 != root1) parent[root0] = root1;
        }
        int fanCount = 0;
        for (int i = 0; i < static_cast<int>(incident.size()); ++i)
            if (findRoot(i) == i) ++fanCount;
        if (fanCount > 1) pinned[v] = 1;
    }

    //Boundary force normalization: rescale the contributions of pinned
    //neighbours to the median edge length (no-op on meshes without pins).
    bool hasPinned = false;
    for (int v = 0; v < n && !hasPinned; ++v) hasPinned = pinned[v] != 0;
    const bool normalize = boundaryMode != BoundaryForceMode::Uniform && hasPinned;
    double lengthMedian = 0.0;
    if (normalize) {
        //median over the same unique face edges as median_edge_length()
        std::vector<double> lengths;
        lengths.reserve(edgeFaces.size());
        for (const auto& entry : edgeFaces)
            lengths.push_back((V.row(entry.first.first) - V.row(entry.first.second)).norm());
        std::sort(lengths.begin(), lengths.end());
        const size_t count = lengths.size();
        lengthMedian = (count % 2 == 1)
            ? lengths[count / 2]
            : 0.5 * (lengths[count / 2 - 1] + lengths[count / 2]);
    }

    //Jacobi passes over a double buffer: pinned vertices keep their position
    //but remain ordinary neighbours, so they still exert force.
    Eigen::MatrixXd p = V;
    Eigen::MatrixXd q = V;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (int v = 0; v < n; ++v) {
            if (pinned[v] || nbrs[v].empty()) {
                q.row(v) = p.row(v);
                continue;
            }
            if (normalize) {
                Eigen::RowVector3d force = Eigen::RowVector3d::Zero();
                for (const int u : nbrs[v]) {
                    Eigen::RowVector3d edge = p.row(u) - p.row(v);
                    if (pinned[u] && lengthMedian > 0.0) {
                        const double length = edge.norm();
                        if (length > 0.0) {
                            double scale = lengthMedian / length;
                            if (boundaryMode == BoundaryForceMode::Clamped && scale < 1.0)
                                scale = 1.0;
                            edge *= scale;
                        }
                    }
                    force += edge;
                }
                q.row(v) = p.row(v) +
                           (strength / static_cast<double>(nbrs[v].size())) * force;
                continue;
            }
            Eigen::RowVector3d mean = Eigen::RowVector3d::Zero();
            for (const int u : nbrs[v]) mean += p.row(u);
            mean /= static_cast<double>(nbrs[v].size());
            q.row(v) = p.row(v) + strength * (mean - p.row(v));
        }
        p.swap(q);
    }
    result.V = p;

    for (int v = 0; v < n; ++v) {
        if (pinned[v]) ++result.pinned;
        if ((result.V.row(v) - V.row(v)).norm() > 0.0) ++result.moved;
    }
    return result;
}

} //namespace smooth_mesh_detail

/***
 Iterative boundary-preserving mesh smoothing ("internal smoothing", the
 verified Remesher rule): a uniform Jacobi Laplacian that moves only interior
 vertices. Pinned vertices -- boundary (edge with != 2 incident faces),
 non-manifold (split fan) and isolated vertices -- keep their position but
 still contribute to their neighbours' averages, so the boundary shape is
 preserved exactly.

 strength is clamped to [0,1], iterations < 1 returns V bit-unchanged.
 BoundaryForceMode rescales only the forces that pinned neighbours exert on
 interior vertices (see the enum); on meshes without pinned vertices all
 modes are bit-identical.

 Overloads: triangle mesh (V, F), polygonal mesh (V, D, F -- face f uses only
 the first D(f) columns), and an in-place TriMesh wrapper that optionally
 dissolves degenerate edges first and refreshes the geometric quantities.
 ***/
inline SmoothingResult smooth_mesh(const Eigen::MatrixXd& V,
                                   const Eigen::MatrixXi& F,
                                   double strength,
                                   int iterations,
                                   const BoundaryForceMode boundaryMode = BoundaryForceMode::Uniform)
{
    std::vector<std::vector<int>> faces(F.rows());
    for (int f = 0; f < F.rows(); ++f)
        faces[f] = {F(f,0), F(f,1), F(f,2)};
    return smooth_mesh_detail::smooth_mesh_impl(V, faces, strength, iterations, boundaryMode);
}

inline SmoothingResult smooth_mesh(const Eigen::MatrixXd& V,
                                   const Eigen::VectorXi& D,
                                   const Eigen::MatrixXi& F,
                                   double strength,
                                   int iterations,
                                   const BoundaryForceMode boundaryMode = BoundaryForceMode::Uniform)
{
    std::vector<std::vector<int>> faces(F.rows());
    for (int f = 0; f < F.rows(); ++f) {
        const int m = (f < D.size()) ? D(f) : F.cols();
        if (m < 3) { faces[f] = {}; continue; }
        faces[f].reserve(m);
        for (int c = 0; c < m && c < F.cols(); ++c)
            faces[f].push_back(F(f, c));
    }
    return smooth_mesh_detail::smooth_mesh_impl(V, faces, strength, iterations, boundaryMode);
}

inline SmoothingResult smooth_mesh(TriMesh& mesh,
                                   double strength,
                                   int iterations,
                                   const BoundaryForceMode boundaryMode = BoundaryForceMode::Uniform,
                                   const bool dissolveDegenerate = true)
{
    if (dissolveDegenerate)
        dissolve_degenerate_edges(mesh);
    SmoothingResult result = smooth_mesh(mesh.V, mesh.F, strength, iterations, boundaryMode);
    mesh.V = result.V;
    //topology is unchanged; only geometric quantities need a refresh
    mesh.compute_geometric_quantities();
    return result;
}

}

#endif
