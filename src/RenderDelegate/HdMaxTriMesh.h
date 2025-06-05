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
#pragma once

#include "RenderDelegateAPI.h"

#include <RenderDelegate/Imaging/HdMaxRenderDelegate.h>

#include <MaxUsd/Utilities/MeshUtils.h>

// Class holding 3dsMax geometry (TriMesh) converted from hydra render geometry.
class HdMaxTriMesh
{
public:
    // A struct holding a triangulated USD geometry. It is assumed that all mapped data (primvars)
    // shares the same indices.
    struct Input
    {
        // The mesh's triangle indices for each subset (need one subset per bound material).
        std::vector<pxr::VtVec3iArray> subsetTopoIndices;
        // Edge visibility information .
        std::vector<pxr::VtVec3iArray> subsetEdgeVis;
        // The primvar data indices for each subset : normals, uv and vertex color.
        std::vector<pxr::VtVec3iArray>            subsetPrimvarIndices;
        pxr::VtVec3fArray                         points;
        pxr::VtVec3fArray                         normals;
        std::vector<MaxUsd::MeshUtils::UvChannel> uvs;
        pxr::VtVec3fArray                         colors;
        // Material ids associated with each subset.
        std::vector<int> materialIds;
        using Ptr = std::shared_ptr<Input>;
    };

    /**
     * Default constructor. When using this constructor, the class will have ownership
     * of the held 3dsMax mesh.
     */
    HdMaxTriMesh() { ownedMesh = std::make_unique<Mesh>(); }

    /**
     * Constructor from an existing 3dsmax Mesh. When using this constructor, the class will
     * NOT have ownership of the mesh.
     * @param mesh The mesh.
     */
    HdMaxTriMesh(Mesh* mesh) { this->externalMesh = mesh; }

    // Delete the copy/move constructors assignment operators.
    HdMaxTriMesh(const HdMaxTriMesh&) = delete;
    HdMaxTriMesh& operator=(const HdMaxTriMesh&) = delete;
    HdMaxTriMesh(HdMaxTriMesh&&) = delete;
    HdMaxTriMesh& operator=(HdMaxTriMesh&&) = delete;

    /**
     * \brief Builds the 3dsmax mesh from the given inputs (hydra geometry data) - i.e. converting the usd/hydra geometry
     * to the 3dsmax mesh format, populating the mesh held by the HdMaxTriMesh instance.
     * \param inputs The USD geometry to convert.
     * \param transforms The USD geometry to convert.
     * \param sourceDataFingerPrint Fingerprint of the source hydra data collection.
     * \param primvarMappingOpts Primvar to channel mapping options.
     * \param unmappedPrimvar Output set of primvars which are not mapped to a channel, and so missing from the resulting mesh.
     * \return True if the conversion to a 3dsmax Mesh was successful, false otherwise.
     */
    RenderDelegateAPI bool Build(
        const std::vector<Input::Ptr>& inputs,
        std::vector<Matrix3>& transforms, // cannot be const because its api is not const-correct.
        size_t                sourceDataFingerPrint,
        const MaxUsd::PrimvarMappingOptions&                     primvarMappingOpts,
        pxr::TfHashSet<pxr::TfToken, pxr::TfToken::HashFunctor>& unmappedPrimvars);

    /**
     * Returns the 3dsmax mesh last generated from the usd render data.
     * Can be null.
     * @return The mesh.
     */
    RenderDelegateAPI Mesh* GetMesh() const;

    /**
     * Returns the fingerprint of the render data collection used to convert the currently held
     * mesh.
     * @return The fingerprint.
     */
    RenderDelegateAPI size_t GetSourceFingerPrint() const;

    /**
     * Clears the mesh and fingerprint.
     */
    RenderDelegateAPI void Reset();

private:
    // The mesh. Only one of these at a time in any instance of HdMaxTriMesh.
    Mesh*                 externalMesh = nullptr;
    std::unique_ptr<Mesh> ownedMesh;
    // Source render data figer print.
    size_t fingerPrint = 0;
};