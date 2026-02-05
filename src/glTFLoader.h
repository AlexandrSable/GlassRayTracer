#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct SimpleMeshData {
    std::vector<float> positions;      // xyz xyz xyz ...
    std::vector<float> normals;        // xyz xyz xyz ... (optional)
    std::vector<uint32_t> indices;     // if empty -> draw arrays
    bool hasIndices() const { return !indices.empty(); }
    bool hasNormals() const { return !normals.empty(); }
};

// Loads first mesh/first primitive from .glb/.gltf
// Throws std::runtime_error on failure.
SimpleMeshData LoadFirstMeshPositions(const std::string& path);
