// This file is part of Directional, a library for directional field processing.
// Copyright (C) 2026 Amir Vaxman <avaxman@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at the copy of the MPL at http://mozilla.org/MPL/2.0/.

#ifndef DIRECTIONAL_WRITE_POLYGONAL_OBJ_H
#define DIRECTIONAL_WRITE_POLYGONAL_OBJ_H

#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <Eigen/Core>

namespace directional
{

/***
 Writes a polygonal mesh as an ascii OBJ file. Coordinates are written with
 std::numeric_limits<double>::max_digits10 significant digits, so reading the
 file back reproduces the doubles bit-exactly (the original Remesher code used
 16 digits).

 Inputs:
   fileName  path to the .obj file
   V  #V by 3 vertex coordinates
   D  #F by 1 face degrees (number of corners per face)
   F  #F by max(D) vertex indices per face; only the first D(f) columns of
      row f are written (1-based, OBJ convention). Faces with D(f) <= 0 are
      skipped.
 Returns false if the file cannot be opened.
 ***/
inline bool write_polygonal_obj(const std::string& fileName,
                                const Eigen::MatrixXd& V,
                                const Eigen::VectorXi& D,
                                const Eigen::MatrixXi& F)
{
    if (fileName.empty()) return false;
    std::ofstream out(fileName);
    if (!out.is_open()) return false;
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (int v = 0; v < V.rows(); ++v)
        out << "v " << V(v, 0) << " " << V(v, 1) << " " << V(v, 2) << "\n";
    for (int f = 0; f < D.size(); ++f) {
        if (D(f) <= 0) continue;
        out << "f";
        for (int c = 0; c < D(f); ++c)
            out << " " << (F(f, c) + 1);
        out << "\n";
    }
    return true;
}

}

#endif
