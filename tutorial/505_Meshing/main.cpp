#include <iostream>
#include <string>
#include <Eigen/Core>
#include <directional/TriMesh.h>
#include <directional/PCFaceTangentBundle.h>
#include <directional/CartesianField.h>
#include <directional/readOFF.h>
#include <directional/read_raw_field.h>
#include <directional/curl_matching.h>
#include <directional/combing.h>
#include <directional/setup_integration.h>
#include <directional/integrate.h>
#include <directional/setup_mesher.h>
#include <directional/mesher.h>
#include <directional/directional_viewer.h>
#include "polygonal_write_OFF.h"

#define NUM_N 3

int N[NUM_N];
int currN = 0;
directional::TriMesh meshWhole, meshCut[NUM_N];
directional::PCFaceTangentBundle ftb;
directional::CartesianField rawField[NUM_N], combedField[NUM_N];
Eigen::VectorXi DPolyMesh[NUM_N];
Eigen::MatrixXi FPolyMesh[NUM_N];
Eigen::MatrixXd VPolyMesh[NUM_N];
Eigen::MatrixXd NFunction[NUM_N], NCornerFunction[NUM_N];
directional::DirectionalViewer viewer;
polyscope::SurfaceMesh* polygonalMeshes[NUM_N];

typedef enum {FIELD, INTEGRATION} ViewingModes;
ViewingModes viewingMode=FIELD;



int main(int argc, char** argv)
{
    //optional CLI flags:
    //  --rounding-offset <off>  rounding target for singularities when roundSeams=false (0 = integer, 0.5 = half-integer)
    //  --epsilon-round <eps>    batch-round all values closer than eps to their target in one iteration (0 = greedy, default)
    double roundingOffset = 0.0;
    double epsilonRound = 0.0;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--rounding-offset" && i + 1 < argc)
            roundingOffset = std::stod(argv[++i]);
        else if (arg == "--epsilon-round" && i + 1 < argc)
            epsilonRound = std::stod(argv[++i]);
    }
    std::cout<<"roundingOffset = "<<roundingOffset<<", epsilonRound = "<<epsilonRound<<std::endl;
    
    directional::readOFF(TUTORIAL_DATA_PATH "/vase.off",meshWhole);
    ftb.init(meshWhole);
    directional::read_raw_field(TUTORIAL_DATA_PATH "/vase-4.rawfield", ftb, N[0], rawField[0]);
    directional::read_raw_field(TUTORIAL_DATA_PATH "/vase-7.rawfield", ftb, N[1], rawField[1]);
    directional::read_raw_field(TUTORIAL_DATA_PATH "/vase-11.rawfield", ftb, N[2], rawField[2]);
    
    bool verbose=true;
    
    //combing and cutting
    viewer.init();
    for (int i=0;i<NUM_N;i++){
        directional::principal_matching(rawField[i]);
        
        directional::IntegrationData intData(N[i]);
        std::cout<<"Setting up Integration #"<<i<<std::endl;
        directional::setup_integration(rawField[i], intData, meshCut[i], combedField[i]);
        
        intData.verbose=false;
        intData.integralSeamless=true;
        intData.roundSeams=false;
        intData.roundingOffset=roundingOffset;
        intData.epsilonRound=epsilonRound;
        
        std::cout<<"Solving integration for N="<<N[i]<<std::endl;
        directional::integrate(combedField[i],  intData, meshCut[i], NFunction[i],NCornerFunction[i]);
        std::cout<<"Done!"<<std::endl;
        
        //setting up mesh data from integration data
        directional::MesherData mData;
        directional::setup_mesher(meshCut[i], intData, mData);
        
        //meshing and saving
        mData.verbose = false;
        std::cout<<"Meshing N="<<N[i]<<std::endl;
        directional::mesher(meshWhole, mData, VPolyMesh[i], DPolyMesh[i], FPolyMesh[i]);
        std::cout<<"Done!"<<std::endl;
        
        viewer.set_surface_mesh(meshWhole);
        viewer.toggle_mesh(false);
        viewer.set_cartesian_field(combedField[i], std::to_string(N[i]) + "-field", i);
        viewer.set_isolines(meshCut[i],NFunction[i], std::to_string(N[i]) + "-function", i);
        
        //Viewing polygonal mesh by direct call to Polyscope (Directional Viewer doesn't natively support polygonal meshes)
        std::vector<std::vector<int>> psF; psF.resize(DPolyMesh[i].size());
        for (int j=0;j<DPolyMesh[i].size();j++){
            psF[j].resize(DPolyMesh[i](j));
            for (int k=0;k<DPolyMesh[i](j);k++)
                psF[j][k]=FPolyMesh[i](j,k);
        }
        polygonalMeshes[i] = polyscope::registerSurfaceMesh("/vase-"+std::to_string(N[i])+"-generated.off", VPolyMesh[i], psF);
        
        //Also saving file
        hedra::polygonal_write_OFF(TUTORIAL_OUTPUT_PATH "/vase-"+std::to_string(N[i])+"-generated.off", VPolyMesh[i], DPolyMesh[i], FPolyMesh[i]);
        if (i!=0){
            viewer.toggle_cartesian_field(false, i);
            viewer.toggle_isolines(false, i);
            viewer.toggle_singularities(false, i);
            polygonalMeshes[i]->setEnabled(false);
        }
        
        
    }
    viewer.launch();
}


