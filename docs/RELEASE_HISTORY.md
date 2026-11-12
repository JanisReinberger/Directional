# Directional Version Log

## Unreleased (dev)

### New library features

- **Boundary-preserving mesh smoothing** (`include/directional/smooth_mesh.h`): iterative "internal smoothing" — a uniform Jacobi Laplacian that moves only interior vertices, `v ← v + strength · (mean of neighbours − v)`, with `strength ∈ [0,1]` and a configurable iteration count. Pinned vertices — boundary (edges with ≠ 2 incident faces), non-manifold (split fans) and isolated vertices — keep their position but still exert force on their neighbours, so the boundary shape is preserved exactly. The `BoundaryForceMode` option rescales only the forces that pinned neighbours exert on interior vertices: `Uniform` (default, plain edge vectors), `Median` (rescaled to the median edge length both ways) and `Clamped` (only shorter-than-median ones scaled up); on meshes without pinned vertices all modes are bit-identical. Overloads: triangle meshes (`V, F`), polygonal meshes (`V, D, F`) and an in-place `TriMesh` wrapper; the raw overloads return a `SmoothingResult { V, pinned, moved }`. Neighbour summation is deterministic (sorted, deduplicated), so repeated runs with the same inputs are bit-identical. Exemplified by tutorial 107.
- **Degenerate-edge dissolution** (`include/directional/dissolve_degenerate_edges.h`): collapses edges with length ≤ eps, shortest first. If one endpoint is a boundary vertex, the other vertex is merged into it (the boundary node keeps its exact position); interior edges merge at their midpoint. A collapse is only performed if it satisfies the link condition, so the result always stays manifold; non-collapsible edges are skipped. Faces are repaired by dropping cyclically-consecutive repeated corners; faces keeping fewer than 3 corners are removed. Pure overloads for triangle (`V, F, eps`) and polygonal (`V, D, F, eps`) meshes return a `DegenerateEdgeDissolveResult { V, F, D, vertexMap, collapsedEdges, mergedVertices, removedFaces }`, where `vertexMap` (old → new vertex index) lets callers remap vertex-indexed data. The in-place `TriMesh` variant uses `eps = ratio · L_median` (default ratio 0.1), iterates to a fixpoint, compacts unreferenced vertices and rebuilds the mesh.
- **Median edge length** (`include/directional/median_edge_length.h`): median over all unique face edges, with `statistics.median` semantics for even edge counts; triangle and polygonal overloads.
- **Polygonal OBJ writer** (`include/directional/write_polygonal_obj.h`): writes a polygonal mesh (`V`, face degrees `D`, corner matrix `F`) as an ascii OBJ file with `max_digits10` precision, so coordinates round-trip bit-exactly. Complements the triangle-only `writeOBJ`.
- **New tutorial 107_MeshSmoothing**: degrades a mesh by snapping vertices onto neighbours, dissolves the degenerate edges, then smooths interactively (strength / iterations / boundary-force-mode controls); boundary edges are highlighted and the pinned/moved counts and the (zero) boundary drift are logged.

### Bug fixes

- **`TriMesh` / DCEL re-initialization**: `DCEL::init()` never reset the `twin` (and `valid`) fields of the halfedges, so re-running `set_mesh()` on an already-initialized `TriMesh` kept stale twin pointers and hung in `compute_edge_quantities()` (boundary-loop tracing never closed; e.g. a 25 → 24 vertex rebuild took minutes instead of milliseconds). Additionally, `compute_edge_quantities()` appended to `boundaryLoops` instead of rebuilding it, inflating the loop count (and with it `numGenerators`) on every re-initialization. Both are fixed; re-initializing an existing `TriMesh` is now safe and bit-stable.

## Version 3.0 Changes

- Libigl and CGAL are not longer dependencies.
- The viewer is now based on PolyScope.
- Added finite elements fields and discrete exterior calculus forms. This are special examples of the cochain complexes module that can incorporate rich structures.
- The Mesher is now independent, but can use GMP to speed up, if installed.
- Added PolyVector customization algorithms, with the possibility to iterate implicit energy-reduction steps and projections. Exemplified by Ginzburg-Landau fields and integrable fields.
- Phased out (temporarily) subdivision fields and conjugate fields, to modernize the code to the new version.


## Version 2.0 Changes

- Paradigm shift of the entire library to work with polymoprhic discrete tangent bundles. All algorithms (if possible) converted to work with this paradigm directly.

## Version 1.8.0 Changes

- Extension of the PolyVector and power field implementation to include soft and hard partial alignment, and rotational symmetry objectives.

## Version 1.7.0 Changes

- Major overhaul of the viewer, where most operation are encapsulated in a new  ```DirectionalViewer``` class, allowing properties such as isolines, vertex, face, and edge data to be associated with a mesh. User does not (need to) directly control shapes and colors, but rather only provide the geometric quantities.
- Many changes in the actual visualizations - thin arrows for glyphs, edge bars for seams and $N$-function isolines, and nicer spheres for singularities.
- Several bug fixes.

## Version 1.6.0 Changes

- The seamless parameterization package was entirely replaced with a seamless integration and meshing package.
- Several bug fixes.

## Version 1.5.0 Changes

The major changes are:

- Change in name from "libdirectional" to "Directional".
- Full compatibility to the libigl 2.0 release, including similar website build and tutorial cmake paradigms.
- Introducing seamless parameterization.
- Rendering the different component of field visualization (mesh, field, streamlines, glyphs, singularities) as separate meshes, to facilitate coding. As a result, the "appending" options of ``directional::line_cylinders()`` etc. were removed.
- Visualization was canonicalized using ```directional\visualization_schemes.h``` for a more homogeneous look.
- The tutorial portions imported from libigl (Conjugate fields, curl reduction, streamline tracing) are now fully compatible with Directional representation and data structures.
- Combing is now dependent on a given matching, and therefore there is no separate ```curl_combing()``` or  ```principal_combing()```.
- Functionality was renamed to better reflect its general application and target, rather then the name assigned by the relevant papers.

Former Name | New Name
--------|----------------------------------------------------------------------
Integrable PolyVectors   | PolyCurl Reduction
Trivial Connection | Index prescription
Globally Optimal | Power FIelds


## Version 1.0 Changes
Alpha version of Directional (then called "libdirectional"). Introducing the following functionality:

1. Glyph Drawing with singularities.
2. Trivial connection.
3. Globally optimal fields.
4. Polyvectors + integrable polyvectors.
5. Principal matching and combing.


