// This file is part of Directional, a library for directional field processing.
// Copyright (C) 2026 Amir Vaxman <avaxman@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at copy of the MPL at http://mozilla.org/MPL/2.0/.

#ifndef DIRECTIONAL_MEDIAN_EDGE_LENGTH_H
#define DIRECTIONAL_MEDIAN_EDGE_LENGTH_H

#include <algorithm>
#include <set>
#include <vector>
#include <Eigen/Core>

namespace directional
{

namespace median_edge_length_detail {

//faces as vertex-index cycles (>= 3 corners)
inline double median_edge_length_impl(const Eigen::MatrixXd& V,
                                      const std::vector<std::vector<int>>& faces)
{
    const int n = static_cast<int>(V.rows());
    std::set<std::pair<int, int>> edges;
    for (const std::vector<int>& poly : faces) {
        const int m = static_cast<int>(poly.size());
        if (m < 3) continue;
        for (int c = 0; c < m; ++c) {
            const int a = poly[c];
            const int b = poly[(c + 1) % m];
            if (a < 0 || b < 0 || a >= n || b >= n || a == b) continue;
            edges.insert(std::minmax(a, b));
        }
    }
    if (edges.empty()) return 0.0;
    std::vector<double> lengths;
    lengths.reserve(edges.size());
    for (const std::pair<int, int>& e : edges)
        lengths.push_back((V.row(e.first) - V.row(e.second)).norm());
    std::sort(lengths.begin(), lengths.end());
    const size_t count = lengths.size();
    if (count % 2 == 1) return lengths[count / 2];
    return 0.5 * (lengths[count / 2 - 1] + lengths[count / 2]); //statistics.median semantics
}

inline std::vector<std::vector<int>> faces_from_triangles(const Eigen::MatrixXi& F)
{
    std::vector<std::vector<int>> faces(F.rows());
    for (int f = 0; f < F.rows(); ++f)
        faces[f] = {F(f,0), F(f,1), F(f,2)};
    return faces;
}

//polygonal mesh: face f uses only the first D(f) columns of F
inline std::vector<std::vector<int>> faces_from_polygons(const Eigen::VectorXi& D,
                                                         const Eigen::MatrixXi& F)
{
    std::vector<std::vector<int>> faces(F.rows());
    for (int f = 0; f < F.rows(); ++f) {
        const int m = (f < D.size()) ? D(f) : F.cols();
        if (m < 3) { faces[f] = {}; continue; }
        faces[f].reserve(m);
        for (int c = 0; c < m && c < F.cols(); ++c)
            faces[f].push_back(F(f, c));
    }
    return faces;
}

} //namespace median_edge_length_detail

/***
 Median length of all unique face edges (each unordered pair of consecutive
 corners in a face counts once). Edges with an out-of-range or repeated index
 are ignored; a mesh without usable edges returns 0. For an even number of
 edges the median is the average of the two middle values (statistics.median
 semantics). Overloads: triangle mesh (V, F) and polygonal mesh (V, D, F).
 ***/
inline double median_edge_length(const Eigen::MatrixXd& V, const Eigen::MatrixXi& F)
{
    return median_edge_length_detail::median_edge_length_impl(
        V, median_edge_length_detail::faces_from_triangles(F));
}

inline double median_edge_length(const Eigen::MatrixXd& V,
                                 const Eigen::VectorXi& D,
                                 const Eigen::MatrixXi& F)
{
    return median_edge_length_detail::median_edge_length_impl(
        V, median_edge_length_detail::faces_from_polygons(D, F));
}

}

#endif
