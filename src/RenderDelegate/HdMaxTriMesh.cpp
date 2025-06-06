//
// Copyright 2025 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "HdMaxTriMesh.h"

#include <MaxUsd/MeshConversion/PrimvarMappingOptions.h>

#include <MeshNormalSpec.h>

namespace {
const Point3 DEFAULT_COLOR { 0.8f, 0.8f, 0.8f };
const int    VERT_COLOR_MAP = 0;
} // namespace

PXR_NAMESPACE_USING_DIRECTIVE

bool HdMaxTriMesh::Build(
    const std::vector<Input::Ptr>&                           inputs,
    std::vector<Matrix3>&                                    transforms,
    size_t                                                   sourceDataFingerPrint,
    const MaxUsd::PrimvarMappingOptions&                     primvarMappingOpts,
    pxr::TfHashSet<pxr::TfToken, pxr::TfToken::HashFunctor>& unmappedPrimvars)
{
    if (inputs.size() != transforms.size()) {
        TF_RUNTIME_ERROR("Incorrect USD geometry input / transform size.");
        return false;
    }

    const auto meshPtr = GetMesh();
    if (!meshPtr) {
        TF_RUNTIME_ERROR("Invalid mesh object.");
        return false;
    }
    Mesh& mesh = *meshPtr;

    // First, gather some information about the final mesh, so we can allocate memory
    // upfront.

    int numFaces = 0;
    int numPoints = 0;
    int numNormals = 0;
    int numColors = 0;

    // Figure out what UV primvars we will actually load into the render mesh.
    // Build a vector of pairs for each input..
    //   first = Index in input's uvs
    //   second = The target 3dsMax channel for that primvar.
    std::vector<std::vector<std::pair<int, int>>> uvPrimvarsToLoad;
    std::unordered_map<int, int>                  usedChannels; // Channel id -> vertex buffer size

    for (const auto& usdGeom : inputs) {
        // Make sure the input geometry is well defined. Each needs topology, mapped data and
        // material ids - so all those arrays should have matching sizes.
        if (usdGeom->subsetTopoIndices.size() != usdGeom->subsetPrimvarIndices.size()
            || usdGeom->subsetTopoIndices.size() != usdGeom->materialIds.size()) {
            TF_RUNTIME_ERROR("Invalid topology indices.");
            return false;
        }

        // All primvar buffers in the input are expected to be of the same size : normals, colors,
        // uvs. It is possible to have no colors or UVs, however.

        int primvarBufferSize = static_cast<int>(usdGeom->normals.size());

        bool primvarBuffersValid
            = usdGeom->colors.empty() || primvarBufferSize == usdGeom->colors.size();

        if (primvarBuffersValid) {
            for (int i = 0; i < usdGeom->uvs.size(); ++i) {
                if (primvarBufferSize != static_cast<int>(usdGeom->uvs[i].data.size())) {
                    primvarBuffersValid = false;
                    break;
                }
            }
        }

        if (!primvarBuffersValid) {
            TF_RUNTIME_ERROR("Invalid primvar buffers.");
            return false;
        }

        // Figure out the number of faces that we will need in the output mesh. It is an
        // aggregate of the faces of all the subset geometries.
        numFaces += std::accumulate(
            usdGeom->subsetTopoIndices.begin(),
            usdGeom->subsetTopoIndices.end(),
            0,
            [](int total, const pxr::VtVec3iArray& subset) {
                return total + static_cast<int>(subset.size());
            });

        numPoints += static_cast<int>(usdGeom->points.size());
        numNormals += primvarBufferSize;
        numColors += primvarBufferSize;

        // Mapping uv primvar index (in usdGeom->uvs), to max map channel id.
        std::vector<std::pair<int, int>> geomPrimvars;

        for (int i = 0; i < usdGeom->uvs.size(); ++i) {
            const auto& uvChannel = usdGeom->uvs[i];

            // No data... skip.
            if (uvChannel.data.empty()) {
                continue;
            }
            // Skip any auto-generated/fallback UVs that do not originate from a primvar.
            const auto primvarName = uvChannel.varname;
            if (primvarName.IsEmpty()) {
                continue;
            }

            // Is that primvar mapped to a channel? If not, skip, but keep track of this information
            // as it may be important for the caller to know.
            const auto channel
                = primvarMappingOpts.GetPrimvarChannelMapping(primvarName.GetString());
            if (channel == MaxUsd::PrimvarMappingOptions::invalidChannel) {
                unmappedPrimvars.insert(uvChannel.varname);
                continue;
            }

            // This primvar will be loaded into the mesh.
            geomPrimvars.emplace_back(i, channel);

            // Collect the total buffer size for each channel.
            const auto& it = usedChannels.find(channel);
            if (it == usedChannels.end()) {
                usedChannels.insert({ channel, primvarBufferSize });
            } else {
                it->second += primvarBufferSize;
            }
        }

        uvPrimvarsToLoad.push_back(geomPrimvars);
    }

    // Allocate what we need on the mesh...

    mesh.setNumFaces(numFaces);
    mesh.setNumVerts(numPoints);

    MeshNormalSpec* specNormals = nullptr;
    if (numNormals > 0) {
        mesh.SpecifyNormals();
        specNormals = mesh.GetSpecifiedNormals();
        specNormals->SetNumFaces(numFaces);
        specNormals->SetNumNormals(numNormals);
    }

    for (const auto& channel : usedChannels) {
        mesh.setMapSupport(channel.first);
        mesh.Map(channel.first).setNumFaces(numFaces);
        mesh.Map(channel.first).setNumVerts(channel.second);
    }

    // If a primvar is explicitly mapped to vertex colors, use it. Otherwise
    // load the display color as fallback.
    bool useDisplayColorForVertexColor = usedChannels.find(VERT_COLOR_MAP) == usedChannels.end();
    if (useDisplayColorForVertexColor) {
        mesh.setMapSupport(VERT_COLOR_MAP);
        mesh.Map(VERT_COLOR_MAP).setNumFaces(numFaces);
        mesh.Map(VERT_COLOR_MAP).setNumVerts(numColors);
    }

    int currentFace = 0;

    int                          vertexOffset = 0;
    int                          normalOffset = 0;
    int                          colorOffset = 0;
    std::unordered_map<int, int> uvOffsets;

    // Copy over the data...

    for (int geomIdx = 0; geomIdx < inputs.size(); ++geomIdx) {

        const auto& usdGeom = inputs[geomIdx];

        // Join all the faces from each subsets.
        for (int subsetIndex = 0; subsetIndex < usdGeom->subsetTopoIndices.size(); ++subsetIndex) {
            const auto& pointIndices = usdGeom->subsetTopoIndices[subsetIndex];
            const auto& primvarIndices = usdGeom->subsetPrimvarIndices[subsetIndex];

            const auto& edgeVis = usdGeom->subsetEdgeVis[subsetIndex];

            // Index/tri counts should always match.
            if (pointIndices.size() != primvarIndices.size()) {
                return false;
            }

            for (int i = 0; i < pointIndices.size(); ++i) {
                const auto v1 = pointIndices[i][0] + vertexOffset;
                const auto v2 = pointIndices[i][1] + vertexOffset;
                const auto v3 = pointIndices[i][2] + vertexOffset;

                mesh.faces[currentFace].setVerts(v1, v2, v3);
                mesh.faces[currentFace].setMatID(usdGeom->materialIds[subsetIndex]);

                const auto vis1 = edgeVis[i][0];
                const auto vis2 = edgeVis[i][1];
                const auto vis3 = edgeVis[i][2];

                mesh.faces[currentFace].setEdgeVis(0, vis1);
                mesh.faces[currentFace].setEdgeVis(1, vis2);
                mesh.faces[currentFace].setEdgeVis(2, vis3);

                const auto pv1 = primvarIndices[i][0];
                const auto pv2 = primvarIndices[i][1];
                const auto pv3 = primvarIndices[i][2];

                // With the data coming from Nitrous, we know indices are the same
                // for all the channels.
                for (const auto& pvChannel : uvPrimvarsToLoad[geomIdx]) {
                    const auto offset = uvOffsets[pvChannel.second];
                    mesh.Map(pvChannel.second)
                        .tf[currentFace]
                        .setTVerts(pv1 + offset, pv2 + offset, pv3 + offset);
                }

                mesh.Map(VERT_COLOR_MAP)
                    .tf[currentFace]
                    .setTVerts(pv1 + colorOffset, pv2 + colorOffset, pv3 + colorOffset);

                if (specNormals) {
                    MeshNormalFace& face = specNormals->Face(currentFace);
                    face.SetNormalID(0, pv1 + normalOffset);
                    face.SetNormalID(1, pv2 + normalOffset);
                    face.SetNormalID(2, pv3 + normalOffset);
                    face.SpecifyAll();
                }
                currentFace++;
            }
        }

        // Copy the points, transforming them into space.

        const auto& points = usdGeom->points;
        const auto  numVerts = static_cast<int>(points.size());
        std::copy_n(
            reinterpret_cast<const Point3*>(points.cdata()),
            numVerts,
            static_cast<Point3*>(mesh.verts) + vertexOffset);

        transforms[geomIdx].TransformPoints(
            static_cast<Point3*>(mesh.verts) + vertexOffset, numVerts);

        vertexOffset += numVerts;

        // Copy over the normals, which also need a transfomation.

        if (specNormals) {
            const auto& normals = usdGeom->normals;
            const auto  size = static_cast<int>(normals.size());
            std::copy_n(
                reinterpret_cast<const Point3*>(normals.cdata()),
                size,
                specNormals->GetNormalArray() + normalOffset);

            for (int idx = normalOffset; idx < normalOffset + size; ++idx) {
                const auto allNormals = specNormals->GetNormalArray();
                allNormals[idx] = VectorTransform(transforms[geomIdx], allNormals[idx]);

                const float lenSq = LengthSquared(allNormals[idx]);
                if (lenSq && (lenSq != 1.0f)) {
                    allNormals[idx] /= std::sqrt(lenSq);
                }
            }

            normalOffset += size;
        }

        // Copy over the UV primvars.

        // At this point we already validated the primvar sizes are all the same.
        const auto primvarSize = static_cast<int>(usdGeom->normals.size());

        for (const auto& entry : uvPrimvarsToLoad[geomIdx]) {
            const auto  idx = entry.first;
            const auto& uvs = usdGeom->uvs[idx].data;
            const auto  channel = entry.second;

            auto& offset = uvOffsets[channel];

            std::copy_n(
                reinterpret_cast<const Point3*>(uvs.cdata()),
                primvarSize,
                mesh.Map(channel).tv + offset);

            for (int j = offset; j < offset + primvarSize; ++j) {
                // Adjust UV coordinate convention.
                mesh.Map(channel).tv[j].y = 1 - mesh.Map(channel).tv[j].y;
            }

            offset += primvarSize;
        }

        // Copy over the display colors...
        if (useDisplayColorForVertexColor) {
            if (!usdGeom->colors.empty()) {
                const auto& vcs = usdGeom->colors;
                std::copy_n(
                    reinterpret_cast<const Point3*>(vcs.cdata()),
                    primvarSize,
                    mesh.Map(VERT_COLOR_MAP).tv + colorOffset);
            } else {
                // If no color is defined, fallback to a grey color to match USD behavior.
                for (int i = 0; i < primvarSize; ++i) {
                    *(mesh.Map(VERT_COLOR_MAP).tv + colorOffset + i) = DEFAULT_COLOR;
                }
            }
            colorOffset += primvarSize;
        }
    }

    if (specNormals) {
        specNormals->SetAllExplicit();
    }

    // Remove any unused points.
    mesh.DeleteIsoVerts();

    this->fingerPrint = sourceDataFingerPrint;

    return true;
}

Mesh* HdMaxTriMesh::GetMesh() const
{
    if (externalMesh != nullptr) {
        return externalMesh;
    }
    return ownedMesh.get();
}

void HdMaxTriMesh::Reset()
{
    fingerPrint = 0;
    if (externalMesh) {
        *externalMesh = {};
        return;
    }
    *ownedMesh = {};
}
size_t HdMaxTriMesh::GetSourceFingerPrint() const { return fingerPrint; }