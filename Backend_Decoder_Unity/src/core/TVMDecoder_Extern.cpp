#include "TVMDecoder.h"
#include "DecoderManager.h"
#include <cstring>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <mutex>
std::mutex decoderMutex;

extern "C" {

// Global decoder registry, indexed by string name
static std::unordered_map<std::string, std::shared_ptr<TVMDecoder::Decoder>> g_decoderRegistry;

/**
 * @brief CreateDecoder: Creates a new decoder instance and registers it by name.
 *        If a decoder with the same name already exists, it is cleared and replaced.
 * @param name: Decoder name key (C string)
 * @param outputPath: Output directory path
 * @param logging: Whether logging is enabled for this decoder
 * @return true on success
 */
bool CreateDecoder(const char* name, const char* outputPath, bool logging) {
    std::lock_guard<std::mutex> lock(decoderMutex);
    std::string key(name);

    // If decoder exists, clear and remove it
    auto existing = g_decoderRegistry.find(key);
    if (existing != g_decoderRegistry.end()) {
        existing->second->Clear(); // optional
        g_decoderRegistry.erase(existing);
    }

    auto decoder = DecoderManager::CreateDecoder(name, outputPath, logging);
    g_decoderRegistry[key] = decoder;  // Sole ownership
    return true;
}
/**
 * @brief LoadSequence: Loads a full mesh sequence into the decoder.
 * @param name: Decoder key
 * @param mesh: Path to the reference mesh .obj
 * @param dHat: Path to decoded displacements
 * @param B: Path to B matrix
 * @param T: Path to T matrix
 * @return true if successful
 */
bool LoadSequence(const char* name, const char* mesh, const char* dHat, const char* B, const char* T) {
    std::lock_guard<std::mutex> lock(decoderMutex);
    std::string key(name);
    auto it = g_decoderRegistry.find(key);
    if (it == g_decoderRegistry.end()) return false;
    return it->second->LoadSequence(mesh, dHat, B, T);
}

/**
 * @brief GetTriangleIndexCount: Returns number of triangle indices (i.e., face vertex indices)
 */
int GetTriangleIndexCount(const char* name) {
    auto it = g_decoderRegistry.find(name);
    if (it == g_decoderRegistry.end()) return 0;
    if (it->second->IsLoaded()==false) return 0;
    return static_cast<int>(it->second->GetTriangleIndicesFlat().size());
}

/**
 * @brief GetTriangleIndices: Copies triangle indices into the provided output buffer.
 * @param outIndices: Destination buffer
 * @param maxCount: Maximum number of indices to copy
 */
void GetTriangleIndices(const char* name, int* outIndices, int maxCount) {
    auto it = g_decoderRegistry.find(name);
    if (it == g_decoderRegistry.end()) return;
    if (it->second->IsLoaded()==false) return;
    const std::vector<int>& tris = it->second->GetTriangleIndicesFlat();
    int count = std::min(static_cast<int>(tris.size()), maxCount);
    std::memcpy(outIndices, tris.data(), sizeof(int) * count);
}

/**
 * @brief GetTotalFrames: Returns the number of frames in the sequence.
 */
int GetTotalFrames(const char* name) {
    auto it = g_decoderRegistry.find(name);
    if (it == g_decoderRegistry.end()) return 0;
    if (it->second->IsLoaded()==false) return 0;
    return it->second->GetTotalFrames();
}

/**
 * @brief GetVertexCount: Returns the number of vertices in the reference mesh.
 */
int GetVertexCount(const char* name) {
    auto it = g_decoderRegistry.find(name);
    if (it == g_decoderRegistry.end()) return 0;
    if (it->second->IsLoaded()==false) return 0;
    return it->second->GetVertexCount();
}

/**
 * @brief GetReferenceVertices: Writes the reference mesh vertices into a float buffer.
 *        Each vertex is written as (x, y, z) in float format.
 */
void GetReferenceVertices(const char* name, float* outVertices) {
    auto it = g_decoderRegistry.find(name);
    if (it == g_decoderRegistry.end()) return;
    if (it->second->IsLoaded()==false) return;
    if (it == g_decoderRegistry.end() || !outVertices) return;

    int count = it->second->GetVertexCount();
    const double* src = it->second->GetReferenceVertices();
    for (int i = 0; i < count; ++i) {
        outVertices[i * 3 + 0] = static_cast<float>(src[i * 3 + 0]);
        outVertices[i * 3 + 1] = static_cast<float>(src[i * 3 + 1]);
        outVertices[i * 3 + 2] = static_cast<float>(src[i * 3 + 2]);
    }
}

/**
 * @brief GetFrameDeformedVertices: Computes the deformed vertex positions for a specific frame.
 *        Applies displacements to the reference mesh and writes the output into a float buffer.
 * @param frameIndex: Index of the frame to compute (0-based)
 */
void GetFrameDeformedVertices(const char* name, int frameIndex, float* outVertices) {
    auto it = g_decoderRegistry.find(name);
    if (it == g_decoderRegistry.end()) return;
    if (it->second->IsLoaded()==false) return;
    if (it == g_decoderRegistry.end() || !outVertices) return;

    int totalFrames = it->second->GetTotalFrames();
    if (frameIndex < 0 || frameIndex >= totalFrames) return;
    const auto& deformed = it->second->ApplyDisplacementToFrame(frameIndex);
    for (size_t i = 0; i < deformed.size(); ++i) {
        outVertices[i * 3 + 0] = static_cast<float>(deformed[i].x());
        outVertices[i * 3 + 1] = static_cast<float>(deformed[i].y());
        outVertices[i * 3 + 2] = static_cast<float>(deformed[i].z());
    }
}

/**
 * @brief CleanDecoders: Clears and removes all decoders not in the protected name list.
 * @param protectedNames: Array of C-strings (decoder names to preserve)
 * @param count: Number of entries in protectedNames
 */
void CleanDecoders(const char** protectedNames, int count) {
    std::lock_guard<std::mutex> lock(decoderMutex);
    std::unordered_set<std::string> protectedSet;
    for (int i = 0; i < count; ++i)
        protectedSet.insert(std::string(protectedNames[i]));

    std::vector<std::string> toDelete;
    for (const auto& [name, decoder] : g_decoderRegistry) {
        if (protectedSet.count(name) == 0)
            toDelete.push_back(name);
    }

    for (const std::string& name : toDelete) {
        g_decoderRegistry[name]->Clear();  // optional
        g_decoderRegistry.erase(name);
    }
}



} // extern "C"
