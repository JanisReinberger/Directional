#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <chrono>
#include <exception>
#include <Eigen/Core>
#include <directional/TriMesh.h>
#include <directional/PCFaceTangentBundle.h>
#include <directional/CartesianField.h>
#include <directional/readOFF.h>
#include <directional/read_raw_field.h>
#include <directional/principal_matching.h>
#include <directional/setup_integration.h>
#include <directional/integrate.h>
#include <directional/setup_mesher.h>
#include <directional/mesher.h>
#include <directional/directional_viewer.h>
#include "polygonal_write_OFF.h"

// 506: batch-rounding meshing benchmark.
// This is the same integration+meshing pipeline as 505_Meshing, but instead of comparing
// different N values, it runs a single N=4 field with three values of intData.epsilonRound:
//   0.0  - greedy rounding, one variable per iteration (the classic behavior)
//   0.25 - batch-rounding: all variables within 0.25 of their rounding target per iteration
//   0.5  - batch-rounding: all variables within 0.5 of their rounding target per iteration
// For every case we report the number of rounding iterations, the integration and meshing
// times, and whether integration/meshing succeeded. A failing case is reported and skipped
// rather than aborting the whole run. The resulting polygonal meshes are then compared in
// the Polyscope viewer, as in 505_Meshing.
// Half-integer rounding targets (intData.roundingOffset) are deliberately not used here.

#define NUM_CASES 3
int N = 4;  //read_raw_field() takes N by reference
const double epsilonRounds[NUM_CASES] = {0.0, 0.25, 0.5};

struct CaseResult{
    double epsilonRound = 0.0;
    int iterations = 0;
    double integrationTime = 0.0;
    double meshingTime = 0.0;
    std::string status = "not run";
    bool meshReady = false;
    int numVertices = 0;
    int numFaces = 0;
};

std::string epsilon_label(const double epsilonRound){
    std::ostringstream label;
    label<<std::fixed<<std::setprecision(2)<<epsilonRound;
    return label.str();
}

double seconds_since(const std::chrono::steady_clock::time_point& start){
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

int main()
{
    directional::TriMesh meshWhole;
    directional::PCFaceTangentBundle ftb;
    directional::CartesianField rawField;
    
    directional::readOFF(TUTORIAL_DATA_PATH "/vase.off", meshWhole);
    ftb.init(meshWhole);
    directional::read_raw_field(TUTORIAL_DATA_PATH "/vase-4.rawfield", ftb, N, rawField);
    directional::principal_matching(rawField);
    
    directional::DirectionalViewer viewer;
    viewer.init();
    viewer.set_surface_mesh(meshWhole);
    viewer.toggle_mesh(false);
    
    CaseResult results[NUM_CASES];
    polyscope::SurfaceMesh* polygonalMeshes[NUM_CASES] = {nullptr, nullptr, nullptr};
    bool anyMeshShown = false;
    
    for (int c=0;c<NUM_CASES;c++){
        const double epsilonRound = epsilonRounds[c];
        const std::string label = epsilon_label(epsilonRound);
        results[c].epsilonRound = epsilonRound;
        
        std::cout<<"=== Case "<<(c+1)<<"/"<<NUM_CASES<<": N="<<N<<", epsilonRound="<<label<<" ==="<<std::endl;
        
        directional::TriMesh meshCut;
        directional::CartesianField combedField;
        directional::IntegrationData intData(N);
        Eigen::MatrixXd NFunction, NCornerFunction;
        directional::MesherData mData;
        Eigen::MatrixXd VPolyMesh;
        Eigen::VectorXi DPolyMesh;
        Eigen::MatrixXi FPolyMesh;
        
        // ---- integration: cutting, combing and (batch-)rounding the singularity values ----
        bool integrationSucceeded = false;
        auto integrationStart = std::chrono::steady_clock::now();
        try{
            directional::setup_integration(rawField, intData, meshCut, combedField);
            
            intData.verbose = false;
            intData.integralSeamless = true;
            intData.roundSeams = false;     //round singularity function values, not the seams
            intData.epsilonRound = epsilonRound;
            
            std::cout<<"Solving integration..."<<std::endl;
            integrationSucceeded = directional::integrate(combedField, intData, meshCut, NFunction, NCornerFunction);
        }catch(const std::exception& e){
            std::cerr<<"Integration raised an exception: "<<e.what()<<std::endl;
        }catch(...){
            std::cerr<<"Integration raised an unknown exception"<<std::endl;
        }
        results[c].integrationTime = seconds_since(integrationStart);
        results[c].iterations = intData.numIterations;
        
        if (!integrationSucceeded){
            results[c].status = "INTEGRATION FAILED";
            std::cout<<"Integration FAILED after "<<results[c].iterations<<" iterations ("
                     <<std::fixed<<std::setprecision(3)<<results[c].integrationTime<<" s); skipping meshing"<<std::endl;
            continue;
        }
        std::cout<<"Integration succeeded: "<<results[c].iterations<<" iterations, "
                 <<std::fixed<<std::setprecision(3)<<results[c].integrationTime<<" s"<<std::endl;
        
        // ---- meshing: extracting the arrangement of integer isolines as a polygonal mesh ----
        bool meshingSucceeded = false;
        auto meshingStart = std::chrono::steady_clock::now();
        try{
            directional::setup_mesher(meshCut, intData, mData);
            
            mData.verbose = false;
            std::cout<<"Meshing..."<<std::endl;
            meshingSucceeded = directional::mesher(meshWhole, mData, VPolyMesh, DPolyMesh, FPolyMesh);
        }catch(const std::exception& e){
            std::cerr<<"Meshing raised an exception: "<<e.what()<<std::endl;
        }catch(...){
            std::cerr<<"Meshing raised an unknown exception"<<std::endl;
        }
        results[c].meshingTime = seconds_since(meshingStart);
        
        //an empty result is a failure as well, even if the mesher reported success
        if ((VPolyMesh.rows()==0)||(DPolyMesh.size()==0)||(FPolyMesh.rows()==0))
            meshingSucceeded = false;
        
        if (!meshingSucceeded){
            results[c].status = "MESHING FAILED";
            std::cout<<"Meshing FAILED after "<<std::fixed<<std::setprecision(3)<<results[c].meshingTime<<" s"<<std::endl;
            continue;
        }
        
        results[c].status = "SUCCESS";
        results[c].meshReady = true;
        results[c].numVertices = VPolyMesh.rows();
        results[c].numFaces = FPolyMesh.rows();
        std::cout<<"Meshing succeeded: "<<results[c].numVertices<<" vertices, "<<results[c].numFaces<<" faces, "
                 <<std::fixed<<std::setprecision(3)<<results[c].meshingTime<<" s"<<std::endl;
        
        // ---- viewing and saving the result ----
        viewer.set_cartesian_field(combedField, "eps"+label+"-field", c);
        viewer.set_isolines(meshCut, NFunction, "eps"+label+"-function", c);
        
        //Viewing polygonal mesh by direct call to Polyscope (Directional Viewer doesn't natively support polygonal meshes)
        std::vector<std::vector<int>> psF; psF.resize(DPolyMesh.size());
        for (int j=0;j<DPolyMesh.size();j++){
            psF[j].resize(DPolyMesh(j));
            for (int k=0;k<DPolyMesh(j);k++)
                psF[j][k]=FPolyMesh(j,k);
        }
        polygonalMeshes[c] = polyscope::registerSurfaceMesh("vase-4-eps"+label+"-generated.off", VPolyMesh, psF);
        
        //Also saving file
        hedra::polygonal_write_OFF(TUTORIAL_OUTPUT_PATH "/vase-4-eps"+label+"-generated.off", VPolyMesh, DPolyMesh, FPolyMesh);
        
        //show only the first successful case; the rest can be toggled on in the viewer
        if (anyMeshShown){
            viewer.toggle_cartesian_field(false, c);
            viewer.toggle_isolines(false, c);
            viewer.toggle_singularities(false, c);
            polygonalMeshes[c]->setEnabled(false);
        }else{
            anyMeshShown = true;
        }
    }
    
    // ---- summary ----
    std::cout<<std::endl;
    std::cout<<"================ Batch-rounding meshing summary (N="<<N<<", vase.off) ================"<<std::endl;
    std::cout<<std::left
             <<std::setw(15)<<"epsilonRound"
             <<std::setw(12)<<"iterations"
             <<std::setw(18)<<"integration [s]"
             <<std::setw(13)<<"meshing [s]"
             <<std::setw(12)<<"total [s]"
             <<std::setw(10)<<"vertices"
             <<std::setw(8)<<"faces"
             <<"status"<<std::endl;
    for (int c=0;c<NUM_CASES;c++){
        std::cout<<std::left
                 <<std::setw(15)<<epsilon_label(results[c].epsilonRound)
                 <<std::setw(12)<<results[c].iterations
                 <<std::setw(18)<<std::fixed<<std::setprecision(3)<<results[c].integrationTime
                 <<std::setw(13)<<std::fixed<<std::setprecision(3)<<results[c].meshingTime
                 <<std::setw(12)<<std::fixed<<std::setprecision(3)<<(results[c].integrationTime+results[c].meshingTime)
                 <<std::setw(10)<<(results[c].meshReady ? std::to_string(results[c].numVertices) : std::string("-"))
                 <<std::setw(8)<<(results[c].meshReady ? std::to_string(results[c].numFaces) : std::string("-"))
                 <<results[c].status<<std::endl;
    }
    std::cout<<"========================================================================================="<<std::endl;
    
    if (!anyMeshShown){
        std::cout<<"No case produced a mesh; not launching the viewer."<<std::endl;
        return 1;
    }
    
    viewer.launch();
    return 0;
}
