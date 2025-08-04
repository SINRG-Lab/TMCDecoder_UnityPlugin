#include "TVMDecoder.h"
#include "SimpleMesh.h"
#include "SimpleMeshIO.h"
#include "MatrixIO.h"
#include "TVMLogger.h"
#include "TVMUtil.h"
#include <filesystem>
#include <cmath>
#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <Eigen/IterativeLinearSolvers>
#include <unordered_set>
#include <vector>
#include <stdexcept>


namespace TVMDecoder {

Decoder::Decoder(const std::string& name, const std::string& out) { //Constructor
    outputPath = out;
    std::filesystem::create_directory(outputPath);
    decoderName = name;
}

Decoder::~Decoder() { //Destructor
    Clear();// Clear memory

}

void Decoder::Clear() { //Memory cleanup helper on destruction
    LOG_INFO("[Decoder] 🔄 Clearing decoder state: %s", decoderName.c_str());
    decodedFrames.clear(); decodedFrames.shrink_to_fit();
    decodedVertexBuffer.clear(); decodedVertexBuffer.shrink_to_fit();
    referenceVertexBuffer.clear(); referenceVertexBuffer.shrink_to_fit();
    triangleIndicesFlat.clear(); triangleIndicesFlat.shrink_to_fit();
    anchor_indices.clear(); anchor_indices.shrink_to_fit();

    Eigen::MatrixXd().swap(dHat);
    Eigen::MatrixXd().swap(bMatrix);
    Eigen::MatrixXd().swap(tMatrix);
    Eigen::MatrixXd().swap(S_hat);
    Eigen::MatrixXd().swap(T_hat);
    Eigen::MatrixXd().swap(tMean);
    Eigen::SparseMatrix<double>().swap(l_star);

    decodedReferenceMesh.vertices.clear();
    decodedReferenceMesh.vertices.shrink_to_fit();
    decodedReferenceMesh.triangles.clear();
    decodedReferenceMesh.triangles.shrink_to_fit();
    decodedReferenceMesh.adjacency_list.clear();
    decodedReferenceMesh.adjacency_list.shrink_to_fit();

    totalFrames = 0;
    verticesPerFrame = 0;
    isLoaded = false;

    LOG_INFO ("[Decoder] ✅ Clear complete for decoder: %s",  decoderName.c_str());
}


bool Decoder::LoadSequence(const std::string& meshFile,
                           const std::string& dHatFile,
                           const std::string& bMatrixFile,
                           const std::string& tMatrixFile) {

    if (meshFile.empty() || dHatFile.empty() || bMatrixFile.empty() || tMatrixFile.empty()) {
        LOG_ERROR("[Decoder] ❌ One or more file paths are empty!");
        return false;
    }

    LOG_INFO ("[Decoder] Paths received:");
    LOG_INFO("  Reference Mesh: ", meshFile.c_str());
    LOG_INFO("  Displacement BIN: ", dHatFile.c_str());
    LOG_INFO ("  B_matrix: ", bMatrixFile.c_str());
    LOG_INFO ("  T_matrix: ", tMatrixFile.c_str());

    //Load and compute reference mesh.
    SimpleMeshIO::ReadOBJ(meshFile, decodedReferenceMesh);
    triangleIndicesFlat = SimpleMeshIO::LoadTriangleIndicesFlat(meshFile);
    decodedReferenceMesh.ComputeAdjacencyList();
    int refCount = decodedReferenceMesh.vertices.size();
    LOG_INFO ("[Decoder] ✅ Loaded reference mesh with %d vertices", refCount);

    //Load and compute dHat.
    dHat = MatrixIO::LoadDeltaTrajectories(dHatFile);
    LOG_INFO("[Decoder] ✅ Loaded dHat: %d x %d", (long)dHat.rows(), (long)dHat.cols());
    int totalRows = dHat.rows(), anchorCount = totalRows - refCount;
    //Infer how many anchors were used during enncoding based off of dHat shape and the reference mesh vertex count.
    //Split the matrix between anchor vertices and reference mesh vertices.
    LOG_INFO("[Decoder] Calculated anchor count: %d", anchorCount);
    anchor_indices.clear();
    for (int i = 0; i < anchorCount; ++i)
        anchor_indices.push_back(std::round(i * (refCount - 1.0f) / (anchorCount - 1.0f)));
    Eigen::MatrixXd D_regular = dHat.topRows(refCount);
    Eigen::MatrixXd D_anchor = dHat.bottomRows(anchorCount);

    //Build the lapacian matrix
    l_star = TVMUtil::BuildLaplacianMatrix(decodedReferenceMesh, anchor_indices);
    LOG_INFO("[Decoder] ✅ Constructed L_star");

    Eigen::MatrixXd rhs(l_star.rows(), dHat.cols());
    rhs.topRows(refCount) = D_regular;
    rhs.bottomRows(anchorCount) = D_anchor;
    //Solve for S Hat.
    S_hat = TVMUtil::SolveLeastSquares(l_star, rhs, 500, 1e-6f);

    //Load B Matrix
    try {
        bMatrix = MatrixIO::loadtxt(bMatrixFile);
    } catch (const std::exception& e) {
        LOG_INFO("[Decoder] ❌ Failed to load B_matrix.txt: %s", e.what());
        return false;
    }

    //Load T Matrix
    try {
        tMatrix = MatrixIO::loadtxt(tMatrixFile);
        LOG_INFO("[Decoder] ✅ Loaded T_matrix");
    } catch (const std::exception& e) {
        LOG_INFO("[Decoder] ❌ Failed to load T_matrix.txt: %s", e.what());
        return false;
    }


    LOG_INFO("[Decoder] Loaded Matrix Shapes:");
    LOG_INFO("  S_hat: %d x %d", static_cast<int>(S_hat.rows()), static_cast<int>(S_hat.cols()));
    LOG_INFO("  B_matrix: %d x %d", static_cast<int>(bMatrix.rows()),"x", static_cast<int>(bMatrix.cols()));
    LOG_INFO("  T_matrix: %d x %d", static_cast<int>(tMatrix.rows()),"x", static_cast<int>(tMatrix.cols()));

    //Compute  S * B + T and get the final T_hat.
    Eigen::MatrixXd sb = S_hat * bMatrix;
    T_hat = TVMUtil::ApplyTMatrixOffset(sb, tMatrix);

    //Set values for sequence
    totalFrames = bMatrix.cols() / 3;
    verticesPerFrame = refCount;
    decodedVertexBuffer.resize(totalFrames * verticesPerFrame * 3);
    decodedFrames.clear();
    decodedFrames.reserve(totalFrames);

    //Loop to store final frame displacements
    for (int i = 0; i < totalFrames; ++i) {
        int colStart = i * 3;
        Eigen::MatrixXd disp = T_hat.block(0, colStart, verticesPerFrame, 3);

        std::vector<Eigen::Vector3d, Eigen::aligned_allocator<Eigen::Vector3d>> frameDisplacements(verticesPerFrame);
        for (int v = 0; v < verticesPerFrame; ++v) {
            frameDisplacements[v] = disp.row(v).transpose();
            size_t offset = (i * verticesPerFrame + v) * 3;
            decodedVertexBuffer[offset + 0] = frameDisplacements[v].x();
            decodedVertexBuffer[offset + 1] = frameDisplacements[v].y();
            decodedVertexBuffer[offset + 2] = frameDisplacements[v].z();
        }
        decodedFrames.push_back(std::move(frameDisplacements));
    }

    LOG_INFO("[Decoder] ✅ Decoded and cached %d frames", totalFrames);



    //Store reference vertices.
    referenceVertexBuffer.clear();
    referenceVertexBuffer.reserve(verticesPerFrame * 3);
    for (const auto& v : decodedReferenceMesh.vertices) {
        referenceVertexBuffer.push_back(v.x());
        referenceVertexBuffer.push_back(v.y());
        referenceVertexBuffer.push_back(v.z());
    }


    isLoaded = true;
    return true;

}

std::vector<std::string> Decoder::DecodeObjs() {
    std::vector<std::string> framePaths;
    if (!isLoaded) return framePaths;

    std::filesystem::create_directories(outputPath);

    for (int frame = 0; frame < totalFrames; ++frame) {
        // Use existing helper to compute deformed vertices
        auto deformedVerts = ApplyDisplacementToFrame(frame);

        // Create mesh from reference and overwrite with displaced verts
        auto mesh = std::make_shared<SimpleMesh::Mesh>(decodedReferenceMesh);
        mesh->vertices = std::move(deformedVerts);

        // Write .obj file
        std::string path = outputPath + "/mesh_frame_" + std::to_string(frame) + ".obj";
        SimpleMeshIO::WriteOBJ(path, *mesh);

        framePaths.push_back(path);
    }

    return framePaths;
}


std::vector<Eigen::Vector3d, Eigen::aligned_allocator<Eigen::Vector3d>>
Decoder::ApplyDisplacementToFrame(int frameIndex) const {
    if(!isLoaded) {
        throw std::runtime_error("Sequence hasn't been loaded yet.");
    };
    if (frameIndex < 0 || frameIndex >= decodedFrames.size()) {
        throw std::out_of_range("Invalid frame index in ApplyDisplacementToFrame");
    }

    const auto& baseVerts = decodedReferenceMesh.vertices;
    const auto& displacements = decodedFrames[frameIndex];

    if (baseVerts.size() != displacements.size()) {
        throw std::runtime_error("Mismatch in base and displacement vector sizes.");
    }

    std::vector<Eigen::Vector3d, Eigen::aligned_allocator<Eigen::Vector3d>> deformedVerts(baseVerts.size());

    for (size_t i = 0; i < baseVerts.size(); ++i) {
        deformedVerts[i] = baseVerts[i] + displacements[i];
    }

    return deformedVerts;
}

} // namespace TVMDecoder
