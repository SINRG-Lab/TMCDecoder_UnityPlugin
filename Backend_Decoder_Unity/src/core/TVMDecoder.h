#pragma once

#include <string>
#include <vector>
#include <Eigen/Sparse>
#include "SimpleMesh.h"

namespace TVMDecoder {

class Decoder {
public:
    // Required for proper memory alignment when using Eigen in containers
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    /**
     * @brief Decoder: constructor
     * @param name: A string corresponding to the name of the decoder to store in the manager.
     * @param out: The out file path for writing files.
     */
    Decoder(const std::string& name, const std::string& out);
    ~Decoder();

    /**
     * @brief LoadSequence: Loads and decode an encoded mesh sequence.
     * @param meshFile: The file path corelating to the reference mesh
     * @param dHatFile: The file path corelating to the delta trajectories .bin file
     * @param bMatrixFile: The file path correlating to the b matrix .txt file
     * @param tMatrixFile: The file path correlating to the t matrix .txt file
     * @returns: A boolean corresponding to whether or not the sequence was sucessfully loaded.
     */
    bool LoadSequence(const std::string& meshFile, const std::string& dHatFile, const std::string& bMatrixFile, const std::string& tMatrixFile);


    /**
     * @brief DecodeObjs: Output a sequence of .OBJs from the encoded sequence
     * @return A vector of strings that represent the files paths for the decoded .OBJ files.
     *         NOTE: The vector will return null if the sequence has not been loaded.
     */
    std::vector<std::string> DecodeObjs();

    /**
     * @brief ApplyDisplacementsToFrame: Fetches the stored decoded displaments for a specific frame.
     * @param frameIndex: An integer representing the index for the frame we are fetching (0-based)
     * @throws RunTimeError: If the sequence hasn't been loaded or the displacement vertex count doesnt
     *         match the reference frame vertex count.
     * @throws OutOfRange: If the provided frame is outside of the range of frames in the sequence.
     * @return A vector storing the final displaced mesh data.
     *         NOTE: The vector will return null if the sequence has not been loaded.
     */
    std::vector<Eigen::Vector3d, Eigen::aligned_allocator<Eigen::Vector3d>> ApplyDisplacementToFrame(int frameIndex) const;

    // Getter functions
    const std::string& GetName() const { return decoderName; }
    int GetTotalFrames() const { return totalFrames; }
    bool IsLoaded() const {return isLoaded;}
    int GetVertexCount() const { return verticesPerFrame; }
    std::vector<int> GetTriangleIndicesFlat() const { return triangleIndicesFlat; }
    const double* GetReferenceVertices() {return referenceVertexBuffer.data();}

    // Memory cleanup
    void Clear();

private:
    std::string decoderName;
    std::string outputPath;

    // Decoding data
    SimpleMesh::Mesh decodedReferenceMesh;
    Eigen::MatrixXd dHat, bMatrix, tMatrix, S_hat, T_hat, tMean;
    Eigen::SparseMatrix<double> l_star;

    // Decoded buffers
    std::vector<std::vector<Eigen::Vector3d, Eigen::aligned_allocator<Eigen::Vector3d>>> decodedFrames;
    std::vector<double> decodedVertexBuffer;
    std::vector<double> referenceVertexBuffer;
    std::vector<int> triangleIndicesFlat;
    std::vector<int> anchor_indices;

    // State
    int totalFrames = 0;
    int verticesPerFrame = 0;
    bool isLoaded = false;
};

} // namespace TVMDecoder
