#include <iostream>
#include <Eigen/Core>
#include <directional/readOFF.h>
#include <directional/TriMesh.h>
#include <directional/directional_viewer.h>
#include <directional/dissolve_degenerate_edges.h>
#include <directional/smooth_mesh.h>

directional::TriMesh original;   // post-dissolution reference state
directional::TriMesh working;    // state being smoothed
directional::DirectionalViewer viewer;
polyscope::SurfaceMesh* workingPsMesh = nullptr;

float strength = 0.5f;       // smoothing step size in [0,1]
int numIterations = 10;      // iterations per "Smooth" press
int modeIndex = 0;           // 0 = Uniform, 1 = Median, 2 = Clamped
int dissolvedCount = 0;
int lastPinned = 0, lastMoved = 0;

// Snaps a few interior vertices almost onto a neighbor, creating degenerate
// (sub-threshold) edges so that the dissolution step is actually exercised.
void degrade(directional::TriMesh& mesh)
{
    int snapped = 0;
    for (int v = 0; v < mesh.V.rows() && snapped < 12; v++) {
        if (mesh.isBoundaryVertex(v))
            continue;
        const int e = mesh.VE(v, 0);
        const int other = (mesh.EV(e, 0) == v) ? mesh.EV(e, 1) : mesh.EV(e, 0);
        if (mesh.isBoundaryVertex(other))
            continue; // keep boundary-adjacent collapses for the boundary rule demo below
        mesh.V.row(v) = mesh.V.row(other) + 0.01 * mesh.avgEdgeLength * Eigen::RowVector3d(1, 0.3, 0.7).normalized();
        snapped++;
    }
    // also snap one interior vertex onto a boundary neighbor, to exercise the
    // "merge into the boundary node" rule
    for (int v = 0; v < mesh.V.rows() && snapped < 13; v++) {
        if (mesh.isBoundaryVertex(v))
            continue;
        const int e = mesh.VE(v, 0);
        const int other = (mesh.EV(e, 0) == v) ? mesh.EV(e, 1) : mesh.EV(e, 0);
        if (!mesh.isBoundaryVertex(other))
            continue;
        mesh.V.row(v) = mesh.V.row(other) + 0.01 * mesh.avgEdgeLength * Eigen::RowVector3d(0.2, 1, 0.5).normalized();
        snapped++;
    }
    mesh.compute_geometric_quantities(); // topology unchanged
    std::cout << "Degraded mesh: snapped " << snapped << " interior vertices onto neighbors." << std::endl;
}

void callbackFunc()
{
    ImGui::PushItemWidth(140);
    ImGui::Text("Dissolved degenerate edges: %d", dissolvedCount);
    ImGui::Text("Last smooth: pinned=%d moved=%d", lastPinned, lastMoved);
    ImGui::SliderFloat("strength", &strength, 0.0f, 1.0f);
    ImGui::SliderInt("iterations", &numIterations, 1, 100);
    ImGui::Combo("boundary force", &modeIndex, "Uniform\0Normalize pinned to L_median\0Clamp short pinned up\0");
    if (ImGui::Button("Smooth")) {
        // remember boundary positions to verify they stay fixed
        Eigen::MatrixXd boundaryBefore = working.V;
        const directional::BoundaryForceMode mode =
            (modeIndex == 0) ? directional::BoundaryForceMode::Uniform :
            (modeIndex == 1) ? directional::BoundaryForceMode::Median :
                               directional::BoundaryForceMode::Clamped;
        directional::SmoothingResult result =
            directional::smooth_mesh(working, (double)strength, numIterations, mode, /*dissolveDegenerate=*/false);
        lastPinned = result.pinned;
        lastMoved = result.moved;
        double maxBoundaryDrift = 0;
        for (int i = 0; i < working.V.rows(); i++)
            if (working.isBoundaryVertex(i))
                maxBoundaryDrift = std::max(maxBoundaryDrift,
                                            (working.V.row(i) - boundaryBefore.row(i)).norm());
        std::cout << "Smoothed " << numIterations << " iterations (strength=" << strength
                  << ", mode=" << (modeIndex == 0 ? "Uniform" : modeIndex == 1 ? "Median" : "Clamped")
                  << "): pinned=" << result.pinned << ", moved=" << result.moved
                  << ", max boundary drift = " << maxBoundaryDrift << std::endl;
        workingPsMesh->updateVertexPositions(working.V);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        working = original;
        workingPsMesh->updateVertexPositions(working.V);
    }
    ImGui::PopItemWidth();
}

int main()
{
    // Load mesh (open surface, so the pinned boundary is visible)
    directional::readOFF(TUTORIAL_DATA_PATH "/botanic-garden-bubble.off", working);
    std::cout << "Loaded mesh: V=" << working.V.rows() << " F=" << working.F.rows()
              << " boundary vertices=" << working.isBoundaryVertex.sum() << std::endl;

    // Create degenerate edges, then dissolve them (precondition for smoothing)
    degrade(working);
    dissolvedCount = directional::dissolve_degenerate_edges(working);
    std::cout << "Dissolved " << dissolvedCount << " degenerate edges: now V=" << working.V.rows()
              << " F=" << working.F.rows() << std::endl;

    original = working; // reference copy (post-dissolution)

    viewer.init();
    viewer.set_callback(&callbackFunc);
    viewer.set_surface_mesh(original, "Original (post-dissolution)", 0);
    workingPsMesh = viewer.set_surface_mesh(working, "Smoothed", 1);
    // highlight the pinned boundary edges on the smoothed mesh
    //note: isBoundaryEdge is sized 2*E in TriMesh; polyscope wants one value per edge
    viewer.set_surface_edge_data(working.isBoundaryEdge.head(working.EV.rows()).cast<double>(), "Boundary edges", 1);
    viewer.launch();
}
