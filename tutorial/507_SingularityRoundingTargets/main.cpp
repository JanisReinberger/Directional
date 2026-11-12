#include <iostream>
#include <iomanip>
#include <algorithm>
#include <Eigen/Core>
#include <directional/readOFF.h>
#include <directional/TriMesh.h>
#include <directional/PCFaceTangentBundle.h>
#include <directional/CartesianField.h>
#include <directional/read_raw_field.h>
#include <directional/principal_matching.h>
#include <directional/setup_integration.h>
#include <directional/integrate.h>
#include <directional/directional_viewer.h>

// 507: rounding targets for singularities.
//
// When the singularity function values are rounded (intData.roundSeams = false), every
// value is snapped to the nearest (integer + intData.roundingOffset):
//   roundingOffset = 0.0  -> integer targets (the classic behavior). All N isolines meet
//                           at every singularity; this is what makes N=4 meshing produce
//                           pure quads (see 503, 505 and 506).
//   roundingOffset = 0.5  -> half-integer targets (new). The singularity sits halfway
//                           between two integer levels, so the isolines pass by it instead
//                           of meeting there, which changes the isoline arrangement (and
//                           the mesh extracted from it).
// Both integrations use the same field and the same cut mesh; only the rounding-target
// lattice differs. Use the combo box in the viewer to switch between the two results.

#define NUM_CASES 2
const double roundingOffsets[NUM_CASES] = {0.0, 0.5};

int N;
directional::TriMesh meshWhole, meshCut[NUM_CASES];
directional::PCFaceTangentBundle ftb;
directional::CartesianField rawField, combedField[NUM_CASES];
Eigen::MatrixXd NFunction[NUM_CASES], NCornerFunction[NUM_CASES];

directional::DirectionalViewer viewer;
int viewingCase = 0;

void callbackFunc(){
    ImGui::PushItemWidth(100);
    
    const char* items[] = {"Integer targets (offset 0)", "Half-integer targets (offset 0.5)"};
    static const char* current_item = NULL;
    
    static float combo_width = 0.0f;
    if (combo_width == 0.0f) {
        ImGuiStyle& style = ImGui::GetStyle();
        for (auto& item : items)
            combo_width = std::max(combo_width, ImGui::CalcTextSize(item).x);
        combo_width += style.FramePadding.x * 5.0 + ImGui::GetFontSize() + style.ItemInnerSpacing.x;
    }
    
    ImGui::PushItemWidth(combo_width);
    if (ImGui::BeginCombo("Rounding target", current_item))
    {
        for (int n = 0; n < IM_ARRAYSIZE(items); n++)
        {
            bool is_selected = (current_item == items[n]);
            if (ImGui::Selectable(items[n], is_selected)) {
                viewingCase = n;
                viewer.set_isolines(meshCut[viewingCase], NFunction[viewingCase]);
                current_item = items[n];
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    
    ImGui::PopItemWidth();
}


int main()
{
    directional::readOFF(TUTORIAL_DATA_PATH "/train-station.off", meshWhole);
    ftb.init(meshWhole);
    directional::read_raw_field(TUTORIAL_DATA_PATH "/train-station-5.rawfield", ftb, N, rawField);
    directional::principal_matching(rawField);
    
    for (int c=0;c<NUM_CASES;c++){
        const double roundingOffset = roundingOffsets[c];
        std::cout<<"=== Case "<<(c+1)<<"/"<<NUM_CASES<<": singularity rounding target = "
                 <<(roundingOffset==0.0 ? "integers" : "half-integers")
                 <<" (roundingOffset="<<std::fixed<<std::setprecision(1)<<roundingOffset<<") ==="<<std::endl;
        
        directional::IntegrationData intData(N);
        directional::setup_integration(rawField, intData, meshCut[c], combedField[c]);
        
        intData.verbose = false;
        intData.integralSeamless = true;
        intData.roundSeams = false;             //round the singularity function values
        intData.roundingOffset = roundingOffset;
        
        std::cout<<"Integrating..."<<std::endl;
        const bool success = directional::integrate(combedField[c], intData, meshCut[c], NFunction[c], NCornerFunction[c]);
        if (!success){
            std::cerr<<"Integration failed for roundingOffset="<<roundingOffset<<std::endl;
            return 1;
        }
        std::cout<<"Done ("<<intData.numIterations<<" rounding iterations)"<<std::endl;
        
        //the rounded function values at the singularities should all sit on the
        //(integer + roundingOffset) lattice
        const int numSingularities = intData.singularIndices.size()/intData.n;
        const int numReported = std::min(numSingularities, 5);
        std::cout<<"Singularity function values ("<<intData.n<<" per singularity):"<<std::endl;
        for (int s=0;s<numReported;s++){
            const int vertex = intData.singularIndices(intData.n*s)/intData.n;
            std::cout<<"  vertex "<<vertex<<": "
                     <<intData.nVertexFunction.segment(intData.n*vertex, intData.n).transpose()<<std::endl;
        }
        if (numSingularities>numReported)
            std::cout<<"  ... ("<<(numSingularities-numReported)<<" more singularities)"<<std::endl;
    }
    
    viewer.init();
    viewer.set_surface_mesh(meshWhole);
    viewer.set_cartesian_field(rawField);
    viewer.set_isolines(meshCut[viewingCase], NFunction[viewingCase]);
    viewer.set_callback(callbackFunc);
    viewer.launch();
    return 0;
}
