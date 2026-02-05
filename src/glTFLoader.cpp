#include "glTFLoader.h"

#include <stdexcept>
#include <string>
#include <cstdio>

#ifdef _WIN32
  #include <io.h>
  #include <fcntl.h>
#endif

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

static const unsigned char* GetBufferDataPtr(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    size_t* outStrideBytes)
{
    const tinygltf::BufferView& view = model.bufferViews.at(accessor.bufferView);
    const tinygltf::Buffer& buf = model.buffers.at(view.buffer);

    const size_t viewOffset = view.byteOffset;
    const size_t accOffset  = accessor.byteOffset;
    const unsigned char* base = buf.data.data() + viewOffset + accOffset;

    // Stride: if 0, tightly packed based on accessor type
    size_t stride = view.byteStride;
    if (stride == 0) {
        // Only handle vec3 float positions in this simple loader.
        // vec3 float = 12 bytes
        stride = 12;
    }
    if (outStrideBytes) *outStrideBytes = stride;
    return base;
}

static void CopyNormalsVec3Float(
    const tinygltf::Model& model,
    const tinygltf::Accessor& normAcc,
    std::vector<float>& outNormals)
{
    if (normAcc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
        throw std::runtime_error("NORMAL is not float.");

    if (normAcc.type != TINYGLTF_TYPE_VEC3)
        throw std::runtime_error("NORMAL is not VEC3.");

    size_t stride = 0;
    const unsigned char* base = GetBufferDataPtr(model, normAcc, &stride);

    outNormals.resize(normAcc.count * 3);

    for (size_t i = 0; i < normAcc.count; i++) {
        const float* n = reinterpret_cast<const float*>(base + i * stride);
        outNormals[i * 3 + 0] = n[0];
        outNormals[i * 3 + 1] = n[1];
        outNormals[i * 3 + 2] = n[2];
    }
}

static void CopyPositionsVec3Float(
    const tinygltf::Model& model,
    const tinygltf::Accessor& posAcc,
    std::vector<float>& outPositions)
{
    if (posAcc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
        throw std::runtime_error("POSITION is not float.");

    if (posAcc.type != TINYGLTF_TYPE_VEC3)
        throw std::runtime_error("POSITION is not VEC3.");

    size_t stride = 0;
    const unsigned char* base = GetBufferDataPtr(model, posAcc, &stride);

    outPositions.resize(posAcc.count * 3);

    for (size_t i = 0; i < posAcc.count; i++) {
        const float* p = reinterpret_cast<const float*>(base + i * stride);
        outPositions[i * 3 + 0] = p[0];
        outPositions[i * 3 + 1] = p[1];
        outPositions[i * 3 + 2] = p[2];
    }
}

static void CopyIndicesToU32(
    const tinygltf::Model& model,
    const tinygltf::Accessor& idxAcc,
    std::vector<uint32_t>& outIndices)
{
    if (idxAcc.type != TINYGLTF_TYPE_SCALAR)
        throw std::runtime_error("Indices accessor is not SCALAR.");

    const tinygltf::BufferView& view = model.bufferViews.at(idxAcc.bufferView);
    const tinygltf::Buffer& buf = model.buffers.at(view.buffer);

    const unsigned char* data = buf.data.data() + view.byteOffset + idxAcc.byteOffset;

    outIndices.resize(idxAcc.count);

    switch (idxAcc.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            auto src = reinterpret_cast<const uint8_t*>(data);
            for (size_t i = 0; i < idxAcc.count; i++) outIndices[i] = src[i];
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            auto src = reinterpret_cast<const uint16_t*>(data);
            for (size_t i = 0; i < idxAcc.count; i++) outIndices[i] = src[i];
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            auto src = reinterpret_cast<const uint32_t*>(data);
            for (size_t i = 0; i < idxAcc.count; i++) outIndices[i] = src[i];
        } break;
        default:
            throw std::runtime_error("Unsupported index componentType (need U8/U16/U32).");
    }
}

SimpleMeshData LoadFirstMeshPositions(const std::string& path)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ok = false;
    const bool isGlb = path.size() >= 4 && (path.substr(path.size()-4) == ".glb");
    if (isGlb) ok = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    else       ok = loader.LoadASCIIFromFile(&model, &err, &warn, path);

    if (!warn.empty()) std::fprintf(stderr, "glTF warn: %s\n", warn.c_str());
    if (!err.empty())  std::fprintf(stderr, "glTF err:  %s\n", err.c_str());
    if (!ok) throw std::runtime_error("Failed to load glTF file: " + path);

    if (model.meshes.empty())
        throw std::runtime_error("glTF has no meshes.");

    const tinygltf::Mesh& mesh = model.meshes[0];
    if (mesh.primitives.empty())
        throw std::runtime_error("Mesh[0] has no primitives.");

    const tinygltf::Primitive& prim = mesh.primitives[0];

    // We want triangles
    if (prim.mode != TINYGLTF_MODE_TRIANGLES)
        throw std::runtime_error("Primitive is not TRIANGLES (only TRIANGLES supported in simple loader).");

    // POSITION attribute is required for our loader
    auto itPos = prim.attributes.find("POSITION");
    if (itPos == prim.attributes.end())
        throw std::runtime_error("Primitive has no POSITION attribute.");

    const tinygltf::Accessor& posAcc = model.accessors.at(itPos->second);   

    SimpleMeshData out;
    CopyPositionsVec3Float(model, posAcc, out.positions);

    // Load normals if available
    auto itNorm = prim.attributes.find("NORMAL");
    if (itNorm != prim.attributes.end()) {
        const tinygltf::Accessor& normAcc = model.accessors.at(itNorm->second);
        CopyNormalsVec3Float(model, normAcc, out.normals);
    }

    // Indices are optional
    if (prim.indices >= 0) {
        const tinygltf::Accessor& idxAcc = model.accessors.at(prim.indices);
        CopyIndicesToU32(model, idxAcc, out.indices);
    }

    return out;
}