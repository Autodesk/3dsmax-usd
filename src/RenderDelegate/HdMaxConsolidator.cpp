//
// Copyright 2023 Autodesk
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
#include "HdMaxConsolidator.h"

#include "Imaging/HdMaxRenderDelegate.h"
#include "SelectionRenderItem.h"

#include <MaxUsd/Utilities/VtUtils.h>

#include <Graphics/CustomRenderItemHandle.h>
#include <Graphics/StandardMaterialHandle.h>

#include <tbb/parallel_for.h>

HdMaxConsolidator::HdMaxConsolidator(
    const std::shared_ptr<pxr::HdMaxRenderDelegate>& renderDelegate)
{
    this->renderDelegate = renderDelegate;
}

bool HdMaxConsolidator::Config::operator==(const Config& config) const
{
    return this->strategy == config.strategy && this->visualize == config.visualize
        && this->maxTriangles == config.maxTriangles && this->maxCellSize == config.maxCellSize
        && this->maxInstanceCount == config.maxInstanceCount
        && this->displaySettings == config.displaySettings
        && this->staticDelay == config.staticDelay;
}

const HdMaxConsolidator::Config& HdMaxConsolidator::GetConfig() const { return this->config; }

void HdMaxConsolidator::SetConfig(const Config& config) { this->config = config; }

HdMaxConsolidator::OutputPtr HdMaxConsolidator::GetConsolidation(const pxr::UsdTimeCode& time) const
{
    const auto it = consolidationCache.find(time);
    if (it == consolidationCache.end()) {
        return nullptr;
    }
    return it->second;
}

void HdMaxConsolidator::GenerateInputs(
    const HdMaxRenderData* primRenderData,
    int                    subsetIndex,
    size_t                 numTriWithSameMaterial,
    std::vector<Input>&    inputs) const
{
    // For instanced data being consolidated, we might need to split them into multiple
    // consolidated mesh.
    const std::vector<Matrix3>* transforms = nullptr;

    std::shared_ptr<std::vector<Matrix3>> singleTransformArray = nullptr;
    if (!primRenderData->shadedSubsets[0].IsInstanced()) {
        singleTransformArray = std::make_shared<std::vector<Matrix3>>();
        singleTransformArray->push_back(MaxUsd::ToMaxMatrix3(primRenderData->transform));
        transforms = singleTransformArray.get();
    }
    // Instanced geometry.
    else {
        transforms = &primRenderData->instancer->GetTransforms();
    }

    // By looking at the total number of triangles sharing the same material as the subset, and not
    // the subset's own size, we can "align" the subsets with the same materials in the same
    // consolidated meshes, and thus reduce memory usage, as vertex buffers can then be shared.
    const auto maxInstancesPerInput = config.maxCellSize / numTriWithSameMaterial;
    DbgAssert(maxInstancesPerInput);
    if (!maxInstancesPerInput) {
        return;
    }

    const auto numInputs
        = int(std::ceil(double(transforms->size()) / double(maxInstancesPerInput)));

    auto nextInstanceStartIdx = 0;
    inputs.reserve(numInputs);
    for (int j = 0; j < numInputs; ++j) {
        const auto multiPartIndex = numInputs > 1 ? j : Input::InvalidMultipartIndex;

        Input input;
        input.primPath = primRenderData->rPrimPath;
        input.subsetIndex = subsetIndex;
        input.indices = primRenderData->shadedSubsets[subsetIndex].indices;
        input.wireIndices = primRenderData->shadedSubsets[subsetIndex].wireIndices;
        input.points = primRenderData->points;
        input.normals = primRenderData->normals;

        // For now we do not support high-quality mode in the viewport.
        // Only support one UV channel. Make sure to use the uv channel that
        // is used for the diffuse color map. Default to the first uv channel.
        auto uvIndex = 0;
        auto it = primRenderData->materialDiffuseColorUvPrimvars.find(
            primRenderData->shadedSubsets[subsetIndex].materiaId);
        if (it != primRenderData->materialDiffuseColorUvPrimvars.end()) {
            for (int i = 0; i < primRenderData->uvs.size(); ++i) {
                if (it->second == primRenderData->uvs[i].varname) {
                    uvIndex = i;
                    break;
                }
            }
        }
        input.uvs = primRenderData->uvs.empty() ? pxr::VtVec3fArray {}
                                                : primRenderData->uvs[uvIndex].data;

        input.dirtyBits = primRenderData->shadedSubsets[subsetIndex].dirtyBits;
        input.multipartIndex = multiPartIndex;
        // Figure out instances that will be in this input...
        auto start = transforms->begin() + nextInstanceStartIdx;
        auto end = start + (maxInstancesPerInput) > transforms->end()
            ? transforms->end()
            : start + (maxInstancesPerInput);
        input.transforms.insert(input.transforms.begin(), start, end);

        const bool instanced = primRenderData->shadedSubsets[subsetIndex].IsInstanced();
        if (instanced) {
            // Get the selected instances from the instancer to configure our input.
            const auto& instanceSelection = primRenderData->instancer->GetSelection();
            const auto  selectionStart = instanceSelection.begin() + nextInstanceStartIdx;
            const auto  selectionEnd
                = selectionStart + (maxInstancesPerInput) > instanceSelection.end()
                ? instanceSelection.end()
                : selectionStart + (maxInstancesPerInput);
            input.selection.insert(input.selection.begin(), selectionStart, selectionEnd);
        } else {
            DbgAssert(input.transforms.size() == 1);
            input.selection.push_back(primRenderData->selected);
        }

        DbgAssert(!input.transforms.empty());

        nextInstanceStartIdx += static_cast<int>(maxInstancesPerInput);
        inputs.push_back(input);
    }
}

void HdMaxConsolidator::BuildConsolidationCells(
    const std::vector<HdMaxRenderData*>&                               renderData,
    std::map<MaxSDK::Graphics::BaseMaterialHandle, std::vector<Cell>>& cells,
    const pxr::UsdTimeCode&                                            time)
{
    const auto existingConsolidation = GetConsolidation(time);

    // Collect geometry that can be consolidated.
    for (const auto& primRenderData : renderData) {
        // No geometry -> nothing to consolidate. Only points and normals are absolutely required.
        if (primRenderData->points.empty() || primRenderData->normals.empty()) {
            continue;
        }

        bool hasSplitInstances = false;

        // Figure out the "actual" material subsets from the final material in the viewport.
        // Indeed, if all geomSubsets are bound the same material, we can treat it as a single mesh.
        // We want to, as much as possible, avoid duplication of vertex buffers and so we try to fit
        // subsets with the same materials, on the same consolidated meshes. Therefor we consider
        // the total number of triangles sharing the same material, if multiple subsets use the same
        // materials.
        std::vector<SubsetInfo>                                              subsetInfos;
        std::vector<std::pair<MaxSDK::Graphics::BaseMaterialHandle, size_t>> materialTris;
        ComputeSubsetInfo(*primRenderData, subsetInfos, materialTris);

        for (int i = 0; i < primRenderData->shadedSubsets.size(); i++) {
            const auto it = std::find_if(
                materialTris.begin(), materialTris.end(), [&subsetInfos, i](const auto& pair) {
                    return pair.first == subsetInfos[i].material;
                });

            const auto numTriWithSameMaterial = it->second;

            if (numTriWithSameMaterial > config.maxTriangles
                || // Too many triangles to consider consolidation.
                numTriWithSameMaterial > config.maxCellSize
                || // More triangles than what can fit in a cell.
                primRenderData->instancer->GetNumInstances() * materialTris.size()
                    > config.maxInstanceCount) // Too many instances (consider how many subsets we
                                               // have.)
            {
                continue;
            }

            // Was it already consolidated?
            if (primRenderData->shadedSubsets[i].inConsolidation) {
                continue;
            }

            if (!primRenderData->shadedSubsets[i].geometry) {
                continue;
            }

            // We can consolidate geometry sharing the same material.
            MaxSDK::Graphics::BaseMaterialHandle consolidationKey = subsetInfos[i].material;

            // Generate the consolidation input(s) from the prim's render data. For instanced
            // geometry we might need multiple inputs, as we might have to distribute the instances
            // over multiple cells.
            std::vector<Input> inputs;
            GenerateInputs(primRenderData, i, numTriWithSameMaterial, inputs);

            hasSplitInstances |= inputs.size() > 1;

            for (const auto& input : inputs) {
                auto& materialCells = cells[consolidationKey];
                // No cells yet for this material, create one.
                if (materialCells.empty()) {
                    materialCells.push_back(
                        Cell { input.indices.size() * input.transforms.size(), { input } });
                }
                // Already have a cell(s), add to the first cell that can receive it without going
                // over the max size.
                else {
                    bool foundSuitableCell = false;
                    for (auto& cell : materialCells) {
                        const auto totalInputTris = input.indices.size() * input.transforms.size();
                        if (cell.numTris + totalInputTris > config.maxCellSize) {
                            continue;
                        }

                        // When some prim has instances with several material subsets split over
                        // multiple consolidated meshes, it is much simpler to manage if the subsets
                        // of the same instances end up in the same merged meshes. Having different
                        // subsets of different instances makes our life much more complicated when
                        // later figuring out where things end up in the consolidation. For sanity,
                        // make sure the subsets only share merged meshes with the same instances.
                        if (hasSplitInstances) {

                            // Consolidating instances can be very memory intensive. Don't use a
                            // cell if it's already partially used by another prim. This way, we
                            // might reduce duplication of vertex buffers, as more instance subsets
                            // will be able to fit on the same cell.

                            // Consider the following example (everything here has the same
                            // material) :

                            // Some previously processed geometry :
                            // a -> simple mesh
                            // b -> simple mesh

                            // A set of  instances with subsets :
                            // subset1_1 -> mesh subset1 of instance 1
                            // subset1_2 -> mesh subset1 of instance 2
                            // subset2_1 -> mesh subset2 of instance 1
                            // subset2_2 -> mesh subset2 of instance 2

                            // Say we currently have a single cell with remaining space available,
                            // but not enough to fit all subsets of the instances: cell 1 : [a,b, ]
                            // We could append to the cell part of the instances, and create a new
                            // cell, which could give us : cell 1 : [a, b, subset1_1,
                            // subset1_1,subset1_2] cell 2 : [,subset2_2, subset2_2] However, this
                            // is not great, as we need a copy of the vertex buffers for the
                            // instance in both cells. Instead, we want : cell 1 : [a, b,    ] cell
                            // 2 : [subset1_1, subset1_1,subset1_2 ,subset2_2, subset2_2] This way,
                            // we only need a single copy of the vertices, for cell 2.

                            const auto otherPrim = std::find_if(
                                cell.inputs.begin(), cell.inputs.end(), [&input](const Input& i) {
                                    return i.primPath != input.primPath;
                                });
                            if (otherPrim != cell.inputs.end()) {
                                continue;
                            }

                            // Do we already have a subset for this prim in the cell?
                            const auto it = std::find_if(
                                cell.inputs.begin(), cell.inputs.end(), [&input](const Input& i) {
                                    return i.primPath == input.primPath;
                                });

                            if (it != cell.inputs.end()) {
                                // If it's not the same exact instances, don't use this cell.
                                const auto sameInstances = std::equal(
                                    it->transforms.begin(),
                                    it->transforms.end(),
                                    input.transforms.begin(),
                                    input.transforms.end());
                                if (!sameInstances) {
                                    continue;
                                }
                            }
                        }

                        cell.inputs.push_back(input);
                        cell.numTris += totalInputTris;
                        foundSuitableCell = true;
                        break;
                    }
                    if (!foundSuitableCell) {
                        // Should not happen, would/should have been split into multiple inputs.
                        const auto numTris = input.indices.size();
                        DbgAssert(numTris * input.transforms.size() <= config.maxCellSize);
                        materialCells.push_back(
                            Cell { numTris * input.transforms.size(), { input } });
                    }
                }
            }
        }
    }
}

HdMaxConsolidator::OutputPtr HdMaxConsolidator::BuildConsolidation(
    const std::vector<HdMaxRenderData*>&        renderData,
    const pxr::UsdTimeCode&                     time,
    const MaxSDK::Graphics::BaseMaterialHandle& wireMaterial)
{
    // If a consolidation already exists, we will append to it.
    auto existingConsolidation = GetConsolidation(time);
    std::map<MaxSDK::Graphics::BaseMaterialHandle, std::vector<Cell>> cells;

    // Build the consolidation cells, i.e. we are figuring out what prims will be combined together.
    BuildConsolidationCells(renderData, cells, time);

    // Consolidate!

    // For each material, we can have one or more cells.
    for (const auto& materialCells : cells) {
        for (const auto& cell : materialCells.second) {
            const auto count = std::accumulate(
                cell.inputs.begin(), cell.inputs.end(), 0, [](int total, const Input& input) {
                    return total + static_cast<int>(input.transforms.size());
                });

            // Don't consolidate the cell if in only has a single input - unless that input is part
            // of a prim render data that must be split over multiple cell, then we still need to
            // consolidate so the prim is complete overall. (This can be the case for instances
            // spread over multiple cells...the last cell might only have one instance)
            if (count == 0
                || (count == 1 && cell.inputs[0].multipartIndex == Input::InvalidMultipartIndex)) {
                continue;
            }

            // If no consolidation output object existed, create it now, and cache it.
            if (!existingConsolidation) {
                existingConsolidation = std::make_shared<Output>();
                consolidationCache[time] = existingConsolidation;
            }

            // Create the object that will own the consolidated render data for this cell.
            ConsolidatedGeomPtr result = std::make_shared<ConsolidatedGeom>();
            result->material = materialCells.first;

            // Consolidate the vertex buffers.
            MaxSDK::Graphics::VertexBufferHandleArray vertexBuffers;

            BuildVertexBuffers(cell, vertexBuffers, *result);

            // For some reason, this can dramatically reduce the overall memory consumption
            // when the GPU memory overflows to the ram. In principle and according to OGS
            // engineers, it is non-standard to call RealizeToHWMemory() in DCC code - but it should
            // be safe here.
            for (auto& buffer : vertexBuffers) {
                buffer.RealizeToHWMemory(true);
            }

            // Build a shaded render item for the consolidated cell.
            {
                const auto simpleRenderGeometry = new MaxSDK::Graphics::SimpleRenderGeometry {};
                simpleRenderGeometry->SetPrimitiveType(MaxSDK::Graphics::PrimitiveTriangleList);
                // Use the same stream requirements as non-consolidated USD render data.
                simpleRenderGeometry->SetSteamRequirement(
                    HdMaxRenderData::GetRequiredStreams(false /*wire*/));

                // Build the consolidated index buffer.
                MaxSDK::Graphics::IndexBufferHandle indexBuffer;
                BuildIndexBuffer(cell, indexBuffer, false /*wireframe*/);
                simpleRenderGeometry->SetIndexBuffer(indexBuffer);
                simpleRenderGeometry->SetPrimitiveCount(indexBuffer.GetNumberOfIndices() / 3);

                // Assign the vertex buffers.
                for (int i = 0; i < vertexBuffers.length(); ++i) {
                    simpleRenderGeometry->AddVertexBuffer(vertexBuffers[i]);
                }

                result->renderItem.Initialize();
                result->renderItem.SetRenderGeometry(simpleRenderGeometry);
                result->renderItem.SetVisibilityGroup(MaxSDK::Graphics::RenderItemVisible_Shaded);

                // Setup the render item used to display selection highlighting. It uses the same
                // render geometry / buffers.
                const auto usdRenderItem = new SelectionRenderItem(
                    static_cast<MaxSDK::Graphics::IRenderGeometryPtr>(simpleRenderGeometry), false);
                result->renderItemSelection.Initialize();
                result->renderItemSelection.SetCustomImplementation(usdRenderItem);
                result->renderItemSelection.SetVisibilityGroup(
                    MaxSDK::Graphics::RenderItemVisible_Shaded);

                // Material setup.
                if (config.visualize) // Useful for debugging consolidation.
                {
                    auto mat = MaxSDK::Graphics::StandardMaterialHandle {};
                    mat.Initialize();
                    float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                    float g = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                    float b = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                    mat.SetDiffuse({ r, g, b });
                    mat.SetAmbient({ r, g, b });
                    result->renderItem.SetCustomMaterial(mat);
                    result->renderItemSelection.SetCustomMaterial(mat);
                } else {
                    result->renderItem.SetCustomMaterial(materialCells.first);
                    result->renderItemSelection.SetCustomMaterial(materialCells.first);
                }
            }

            // Build wireframe render item for the consolidated cell.
            {
                const auto simpleRenderGeometry = new MaxSDK::Graphics::SimpleRenderGeometry {};
                simpleRenderGeometry->SetPrimitiveType(MaxSDK::Graphics::PrimitiveLineList);
                // Use the same stream requirements as non-consolidated USD render data.
                simpleRenderGeometry->SetSteamRequirement(
                    HdMaxRenderData::GetRequiredStreams(true));

                // Build the wire index buffer.
                MaxSDK::Graphics::IndexBufferHandle indexBuffer;
                BuildIndexBuffer(cell, indexBuffer, true /*wireframe*/);

                simpleRenderGeometry->SetIndexBuffer(indexBuffer);
                simpleRenderGeometry->SetPrimitiveCount(indexBuffer.GetNumberOfIndices() / 2);

                // Assign the vertex buffers, they are shared with the shaded geometry.
                // UVs are not needed for wireframe geometry.
                simpleRenderGeometry->AddVertexBuffer(vertexBuffers[HdMaxRenderData::PointsBuffer]);
                simpleRenderGeometry->AddVertexBuffer(
                    vertexBuffers[HdMaxRenderData::NormalsBuffer]);
                simpleRenderGeometry->AddVertexBuffer(
                    vertexBuffers[HdMaxRenderData::SelectionBuffer]);

                result->wireframeRenderItem.Initialize();
                result->wireframeRenderItem.SetVisibilityGroup(
                    MaxSDK::Graphics::RenderItemVisible_Wireframe);
                result->wireframeRenderItem.SetRenderGeometry(simpleRenderGeometry);

                const auto usdRenderItem = new SelectionRenderItem(
                    static_cast<MaxSDK::Graphics::IRenderGeometryPtr>(simpleRenderGeometry), true);
                result->wireframeRenderItemSelection.Initialize();
                result->wireframeRenderItemSelection.SetCustomImplementation(usdRenderItem);
                result->wireframeRenderItemSelection.SetVisibilityGroup(
                    MaxSDK::Graphics::RenderItemVisible_Wireframe);
                result->wireframeRenderItemSelection.SetCustomMaterial(wireMaterial);
            }

            existingConsolidation->geoms->push_back(result);
            for (const auto& input : cell.inputs) {
                existingConsolidation->primToGeom[{ input.primPath, input.subsetIndex }].push_back(
                    result);
                // Flag the render data as part of a consolidated mesh.
                const auto rdIdx = renderDelegate->GetRenderDataIndex(input.primPath);
                auto&      renderData = renderDelegate->GetRenderData(rdIdx);
                renderData.shadedSubsets[input.subsetIndex].inConsolidation = true;
                // Track what exactly goes in the consolidation. Important : here we store the index
                // of the render data in the delegate to speed up access later on. This index can
                // change so when we use it again, we will use
                // renderDelegate->SafeGetRenderData(...), which may fallback to using the prim
                // path.
                existingConsolidation->consolidatedRenderData.push_back(
                    { rdIdx, input.primPath, input.subsetIndex });
            }
        }
    }
    if (existingConsolidation) {
        const auto totalSubsets = std::accumulate(
            renderData.begin(), renderData.end(), 0, [](int total, const auto& data) {
                return total + static_cast<int>(data->shadedSubsets.size());
            });
        existingConsolidation->sourceRenderData.clear();
        existingConsolidation->sourceRenderData.reserve(totalSubsets);
        for (const auto& rd : renderData) {
            for (int i = 0; i < rd->shadedSubsets.size(); ++i) {
                existingConsolidation->sourceRenderData.push_back(
                    { renderDelegate->GetRenderDataIndex(rd->rPrimPath), rd->rPrimPath, i });
            }
        }
    }
    return existingConsolidation;
}

void HdMaxConsolidator::BuildIndexBuffer(
    const Cell&                          cell,
    MaxSDK::Graphics::IndexBufferHandle& output,
    bool                                 wireframe) const
{
    size_t                        totalNumIndices = 0;
    std::vector<std::vector<int>> baseIndices { cell.inputs.size() };

    auto getIndicesData = [&wireframe](const Input& input) {
        return !wireframe ? MaxUsd::Vt::GetNoCopy<UINT, pxr::GfVec3i>(input.indices)
                          : MaxUsd::Vt::GetNoCopy<UINT, int>(input.wireIndices);
    };

    auto getIndicesSize = [&wireframe](const Input& input) {
        return !wireframe ? input.indices.size() * 3 : input.wireIndices.size();
    };

    for (const auto& input : cell.inputs) {
        auto numInstances = input.transforms.size();

        totalNumIndices += getIndicesSize(input) * numInstances;
    }

    output.Initialize(MaxSDK::Graphics::IndexTypeInt, totalNumIndices);

    std::vector<std::vector<std::pair<int*, int>>> targets(cell.inputs.size());

    size_t baseIndex = 0;
    int*   pDestIdx = (int*)output.Lock(0, 0, MaxSDK::Graphics::WriteDiscardAcess);

    for (int inputIndex = 0; inputIndex < cell.inputs.size(); ++inputIndex) {
        const auto& input = cell.inputs[inputIndex];

        bool bumpBaseIndex = false;
        if (inputIndex + 1 < cell.inputs.size()) {
            bumpBaseIndex = input.primPath != cell.inputs[inputIndex + 1].primPath;
        }
        for (int i = 0; i < input.transforms.size(); ++i) {
            targets[inputIndex].emplace_back(
                pDestIdx, static_cast<int>(baseIndex) + i * static_cast<int>(input.points.size()));
            pDestIdx += getIndicesSize(input);
        }
        if (bumpBaseIndex) {
            baseIndex += input.points.size() * input.transforms.size();
        }
    }

    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, cell.inputs.size()), [&](tbb::blocked_range<size_t> r) {
            for (size_t inputIndex = r.begin(); inputIndex < r.end(); ++inputIndex) {
                const auto& input = cell.inputs[inputIndex];
                for (int i = 0; i < input.transforms.size(); ++i) {
                    const auto srcArray = getIndicesData(input);
                    AppendIndexBuffer(
                        targets[inputIndex][i].first,
                        srcArray,
                        targets[inputIndex][i].second,
                        getIndicesSize(input));
                    pDestIdx += getIndicesSize(input);
                }
            }
        });
    output.Unlock();
}

void HdMaxConsolidator::BuildVertexBuffers(
    const Cell&                                cell,
    MaxSDK::Graphics::VertexBufferHandleArray& outputs,
    ConsolidatedGeom&                          result)
{
    size_t numData = 0;

    for (const auto& input : cell.inputs) {
        auto numInstances = input.transforms.size();
        numData += numInstances;
    }

    // Figure out where in the consolidated mesh each prim's geometry will go.
    size_t baseVertexOffset = 0;
    for (int i = 0; i < cell.inputs.size(); ++i) {
        // Only increment the base offset when we switch prim, indeed, we can have
        // multiple inputs for the same prim if there are UdGeomSubsets, but in this
        // case they still all share the same vertex buffers.
        bool bumpBaseIndex = false;
        if (i + 1 < cell.inputs.size()) {
            bumpBaseIndex = cell.inputs[i].primPath != cell.inputs[i + 1].primPath;
        }

        // The prim can have multiple inputs because of usd geom subsets, but the vertex buffers
        // are the same, and so only need to create the mapping once.
        if (result.dataMapping.find(cell.inputs[i].primPath) == result.dataMapping.end()) {
            Mapping mapping;
            mapping.primPath = cell.inputs[i].primPath;
            mapping.vertexCount = cell.inputs[i].points.size();
            mapping.offsets.reserve(cell.inputs[i].transforms.size());
            mapping.multipartIndex = cell.inputs[i].multipartIndex;

            // Create the offset for each one of the instances.
            for (int k = 0; k < cell.inputs[i].transforms.size(); ++k) {
                mapping.offsets.push_back(
                    static_cast<int>(baseVertexOffset)
                    + k * static_cast<int>(cell.inputs[i].points.size()));
            }
            result.dataMapping.insert({ mapping.primPath, mapping });
        }

        if (bumpBaseIndex || i == cell.inputs.size() - 1) {
            baseVertexOffset += cell.inputs[i].points.size() * cell.inputs[i].transforms.size();
        }
    }

    // baseVertexOffset is now equal to the total size we need for vertex buffers.
    MaxSDK::Graphics::VertexBufferHandle positionsBuffer;
    positionsBuffer.Initialize(
        sizeof(Point3), baseVertexOffset, nullptr, MaxSDK::Graphics::BufferUsageDynamic);
    MaxSDK::Graphics::VertexBufferHandle normalsBuffer;
    normalsBuffer.Initialize(
        sizeof(Point3), baseVertexOffset, nullptr, MaxSDK::Graphics::BufferUsageDynamic);
    MaxSDK::Graphics::VertexBufferHandle selectionBuffer;
    selectionBuffer.Initialize(
        sizeof(Point3), baseVertexOffset, nullptr, MaxSDK::Graphics::BufferUsageDynamic);
    MaxSDK::Graphics::VertexBufferHandle uvBuffer;
    uvBuffer.Initialize(
        sizeof(Point3), baseVertexOffset, nullptr, MaxSDK::Graphics::BufferUsageDynamic);

    outputs.removeAll();
    outputs.append(positionsBuffer);
    outputs.append(normalsBuffer);
    outputs.append(selectionBuffer);
    outputs.append(uvBuffer);

    bool hasSelectionHighlight = false;
    UpdateVertexBuffers(outputs, cell.inputs, result.dataMapping, true, hasSelectionHighlight);
    result.hasActiveSelection = hasSelectionHighlight;
}

void HdMaxConsolidator::AppendIndexBuffer(int* pDestIdx, UINT* pSrcIdx, int baseIndex, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        pDestIdx[i] = baseIndex + pSrcIdx[i];
    }
}

void HdMaxConsolidator::ComputeSubsetInfo(
    const HdMaxRenderData&                                                renderData,
    std::vector<SubsetInfo>&                                              subsetInfos,
    std::vector<std::pair<MaxSDK::Graphics::BaseMaterialHandle, size_t>>& materialTris) const
{
    subsetInfos.resize(renderData.shadedSubsets.size());
    for (int i = 0; i < renderData.shadedSubsets.size(); ++i) {
        subsetInfos[i].material = renderData.ResolveViewportMaterial(
            renderData.shadedSubsets[i], config.displaySettings, false);
        subsetInfos[i].numTris = renderData.shadedSubsets[i].indices.size();
    }
    for (const auto& info : subsetInfos) {
        auto it = std::find_if(materialTris.begin(), materialTris.end(), [&info](const auto& pair) {
            return pair.first == info.material;
        });
        if (it != materialTris.end()) {
            it->second += info.numTris;
            continue;
        }
        materialTris.emplace_back(info.material, info.numTris);
    }
}

void HdMaxConsolidator::UpdateVertexBuffers(
    MaxSDK::Graphics::VertexBufferHandleArray&                       toUpdate,
    const std::vector<Input>&                                        inputs,
    const pxr::TfHashMap<pxr::SdfPath, Mapping, pxr::SdfPath::Hash>& mappings,
    bool                                                             fullUpdate,
    bool&                                                            hasSelectionHighlight)
{

    auto getRawBuffer = [](const Input& input, size_t streamIndex) {
        switch (streamIndex) {
        case HdMaxRenderData::PointsBuffer:
            return MaxUsd::Vt::GetNoCopy<Point3, pxr::GfVec3f>(input.points);
        case HdMaxRenderData::NormalsBuffer:
            return MaxUsd::Vt::GetNoCopy<Point3, pxr::GfVec3f>(input.normals);
        case HdMaxRenderData::UvsBuffer:
            return MaxUsd::Vt::GetNoCopy<Point3, pxr::GfVec3f>(input.uvs);
        }
        return (Point3*)nullptr;
    };

    auto getBufferSize = [](const Input& input, size_t streamIndex) {
        switch (streamIndex) {
        case HdMaxRenderData::PointsBuffer: return int(input.points.size());
        case HdMaxRenderData::NormalsBuffer: return int(input.normals.size());
        case HdMaxRenderData::UvsBuffer: return int(input.uvs.size());
        }
        return 0;
    };

    auto getIsDirty = [](const Input& input, size_t streamIndex) {
        switch (streamIndex) {
        case HdMaxRenderData::PointsBuffer:
            return (input.dirtyBits & HdMaxChangeTracker::DirtyPoints) != 0
                || (input.dirtyBits & HdMaxChangeTracker::DirtyTransforms) != 0;
        case HdMaxRenderData::NormalsBuffer:
            return (input.dirtyBits & HdMaxChangeTracker::DirtyNormals) != 0
                || (input.dirtyBits & HdMaxChangeTracker::DirtyTransforms) != 0;
        case HdMaxRenderData::SelectionBuffer:
            return (input.dirtyBits & HdMaxChangeTracker::DirtySelectionHighlight) != 0;
        case HdMaxRenderData::UvsBuffer:
            return (input.dirtyBits & HdMaxChangeTracker::DirtyUvs) != 0;
        }
        return false;
    };

    // Unless we know for sure we want to update everything, try to figure out the range of vertices
    // that must be updated, so we can tell nitrous to only lock on the required vertices. Also
    // figure out what buffers actually need to be updated.
    size_t            firstVertexIndex = 0;
    size_t            size = 0;
    std::vector<bool> dirtyBuffers(toUpdate.length());
    if (!fullUpdate) {
        size_t minIndex = toUpdate[0].GetNumberOfVertices() - 1;
        size_t maxIndex = 0;
        for (const auto& input : inputs) {
            const auto it = mappings.find(input.primPath);
            if (it == mappings.end()) {
                continue;
            }
            const auto mapping = it->second;
            minIndex = std::min(static_cast<size_t>(mapping.offsets.front()), minIndex);
            maxIndex = std::max(
                static_cast<size_t>(mapping.offsets.back()) + mapping.vertexCount - 1, minIndex);
        }

        firstVertexIndex = minIndex;
        size = maxIndex - minIndex + 1;

        for (const auto& input : inputs) {
            for (int i = 0; i < toUpdate.length(); ++i) {
                if (!dirtyBuffers[i]) {
                    if (getIsDirty(input, i)) {
                        dirtyBuffers[i] = true;
                    }
                }
            }
        }
    }

    std::atomic<bool> foundSelectionHighlight = false;

    // Update the buffers!
    for (size_t bufferIndex = 0; bufferIndex < toUpdate.length(); bufferIndex++) {
        if (!fullUpdate && !dirtyBuffers[bufferIndex]) {
            continue;
        }

        auto& vertexBuffer = toUpdate[bufferIndex];
        auto  destBuffer = reinterpret_cast<Point3*>(
            vertexBuffer.Lock(firstVertexIndex, size, MaxSDK::Graphics::WriteDiscardAcess));
        auto endOfBuffer = destBuffer + vertexBuffer.GetNumberOfVertices() - 1;

        // Process all inputs in parallel - TBB is pretty clever in deciding the degree of
        // parallelism.
        tbb::parallel_for(
            tbb::blocked_range<size_t>(0, inputs.size()), [&](tbb::blocked_range<size_t> r) {
                for (size_t i = r.begin(); i < r.end(); ++i) {
                    // Is this input part of this geometry?
                    const auto it = mappings.find(inputs[i].primPath);
                    if (it == mappings.end()) {
                        continue;
                    }

                    // If the input is for an instanced prim that spreads over multiple consolidated
                    // geometries, make sure we are updating right one.
                    if (it->second.multipartIndex != inputs[i].multipartIndex) {
                        continue;
                    }

                    // Unless we are force updating everything, only update the vertices of "dirty"
                    // inputs.
                    if (!getIsDirty(inputs[i], bufferIndex) && !fullUpdate) {
                        continue;
                    }

                    Point3* srcBuffer = nullptr;
                    size_t  srcBufferSize = 0;

                    // Fetch the source buffer data, except for the selection buffer, we just need
                    // to look at the selection flags of the input to know if we should fill in the
                    // selection buffer with ones or zeros in the output consolidated mesh.
                    if (bufferIndex != HdMaxRenderData::SelectionBuffer) {
                        // Some buffers may not exist (uvs may not be loaded).
                        srcBuffer = getRawBuffer(inputs[i], bufferIndex);
                        if (!srcBuffer) {
                            continue;
                        }
                        // Empty buffer, nothing for us to do.
                        srcBufferSize = getBufferSize(inputs[i], bufferIndex);
                        if (!srcBufferSize) {
                            continue;
                        }
                    }

                    // Process all instances this input owns.
                    for (int k = 0; k < inputs[i].transforms.size(); ++k) {
                        const int vertOffset = it->second.offsets[k];

                        // Pointer to were this inputs starts in the consolidated vertex buffer.
                        Point3* inputDestBuffer = destBuffer + vertOffset - firstVertexIndex;

                        const size_t numVerts = inputs[i].points.size();

                        // Special case for selection - there is no source vertex buffer, in
                        // non-consolidated geometry, we do not need one in the prim render data, we
                        // just need a flag.
                        if (bufferIndex == HdMaxRenderData::SelectionBuffer) {
                            const bool highlight = inputs[i].selection[k];
                            const auto selectionValue
                                = highlight ? Point3(1, 1, 1) : Point3(0, 0, 0);
                            std::fill(inputDestBuffer, inputDestBuffer + numVerts, selectionValue);
                            if (highlight) {
                                foundSelectionHighlight = true;
                            }
                            continue;
                        }

                        // All other vertex buffers should be the same size, but that is not
                        // enforced. In this case, we would still only write up to numVerts, which
                        // is what we allocated.
                        DbgAssert(numVerts == srcBufferSize);

                        Matrix3 transform = inputs[i].transforms[k];

                        tbb::parallel_for(
                            tbb::blocked_range<size_t>(0, numVerts),
                            [&](tbb::blocked_range<size_t> vRange) {
                                for (size_t v = vRange.begin(); v < vRange.end(); ++v) {
                                    if (v >= srcBufferSize) {
                                        inputDestBuffer[v] = {};
                                        continue;
                                    }

                                    const bool outOfRange = inputDestBuffer + v > endOfBuffer;
                                    DbgAssert(!outOfRange);
                                    if (outOfRange) {
                                        continue;
                                    }

                                    // Points, need to bake the transform.
                                    if (bufferIndex == HdMaxRenderData::PointsBuffer) {
                                        inputDestBuffer[v] = srcBuffer[v] * transform;
                                    }
                                    // Normal, transform the vector
                                    else if (bufferIndex == HdMaxRenderData::NormalsBuffer) {
                                        inputDestBuffer[v]
                                            = VectorTransform(transform, srcBuffer[v]);
                                        const float lenSq = LengthSquared(inputDestBuffer[v]);
                                        if (lenSq && (lenSq != 1.0f)) {
                                            inputDestBuffer[v] /= std::sqrt(lenSq);
                                        }
                                    }
                                    // UVs stay as-is.
                                    else {
                                        inputDestBuffer[v] = srcBuffer[v];
                                    }
                                }
                            });
                    }
                }
            });
        vertexBuffer.Unlock();
    }

    // If selection is dirty, update the hasSelectionHighlight flag, this allows the caller to pass
    // in the last value it had for it, and use the value as is after the call.
    if (fullUpdate || dirtyBuffers[HdMaxRenderData::SelectionBuffer]) {
        hasSelectionHighlight = foundSelectionHighlight;
    }
}

void HdMaxConsolidator::Reset()
{
    // Clearing any and all existing consolidations. Make sure to flag everything that was
    // consolidated as dirty again, as we are not handling it anymore.
    for (const auto& consolidation : consolidationCache) {
        for (const auto& renderDataInfo : consolidation.second->sourceRenderData) {
            auto& renderData
                = renderDelegate->SafeGetRenderData(renderDataInfo.index, renderDataInfo.primPath);
            if (renderData.rPrimPath.IsEmpty()) {
                // Render data that was previously consolidated no longer exists (deactivated
                // maybe).
                continue;
            }
            // We may get into a situation, for example if a prim changes render purpose during
            // variant selection, where a previously consolidated prim, still exists, but was
            // re-created from scratch, and is not fully initialized, as it doesn't need to be
            // displayed yet. Make sure the subset we are looking to dirty still exists.
            if (renderDataInfo.subsetIdx >= renderData.shadedSubsets.size()) {
                continue;
            }
            HdMaxChangeTracker::SetDirty(
                renderData.shadedSubsets[renderDataInfo.subsetIdx].dirtyBits,
                HdMaxChangeTracker::AllDirty);
            renderData.shadedSubsets[renderDataInfo.subsetIdx].inConsolidation = false;
        }
    }
    consolidationCache.clear();
}

void HdMaxConsolidator::GetConsolidatedPrimSubsets(
    const pxr::UsdTimeCode& time,
    PrimSubsetSet&          consolidatedPrimSubsets) const
{
    const auto it = consolidationCache.find(time);
    if (it == consolidationCache.end()) {
        return;
    }
    auto& consolidation = it->second;
    consolidatedPrimSubsets.reserve(consolidation->primToGeom.size());
    for (const auto& entry : consolidation->primToGeom) {
        consolidatedPrimSubsets.insert(entry.first);
    }
}

void HdMaxConsolidator::UpdateConsolidation(
    const std::vector<HdMaxRenderData*>& renderData,
    const pxr::UsdTimeCode&              previousTime,
    const pxr::UsdTimeCode&              newTime)
{
    const auto it = consolidationCache.find(previousTime);
    if (it == consolidationCache.end()) {
        return;
    }

    // Is there a consolidation we should attempt to update?
    const auto currentConsolidation = it->second;
    if (!currentConsolidation || currentConsolidation->geoms->empty()) {
        return;
    }

    // Figure out if any prim's data that is part of the current consolidation is dirty and
    // if so, if it can be updated.
    struct PrimConsolidationData
    {
        HdMaxRenderData* primRenderData;
        std::vector<int> consolidatedSubsets;
    };
    std::vector<PrimConsolidationData> dirtyConsolidatedData;
    bool                               consolidationIsDirty = false;
    bool                               breakConsolidation = false;

    // Keep track of how many of the prims we need to render are in the existing consolidation, if
    // prims which are part of the consolidation are no longer required (for example they were
    // hidden from view), we need to break the consolidation.
    size_t inConsolidation = 0;
    for (const auto& primData : renderData) {
        std::vector<int> consolidatedSubsets;
        for (int i = 0; i < primData->shadedSubsets.size(); ++i) {
            auto& data = primData->shadedSubsets[i];
            if (!data.inConsolidation) {
                continue;
            }

            inConsolidation++;

            // Not dirty -> nothing to update for this render data.
            if (data.dirtyBits == HdMaxChangeTracker::Clean) {
                continue;
            }

            consolidatedSubsets.push_back(i);

            // Data is dirty, and not in dynamic mode, break.
            if (previousTime != newTime && config.strategy != Strategy::Dynamic) {
                breakConsolidation = true;
            }

            // Some changes to the data prevent us from updating the vertex buffers, typically when
            // size of meshes change.
            breakConsolidation
                = breakConsolidation
                || HdMaxChangeTracker::CheckDirty(
                      data.dirtyBits,
                      HdMaxChangeTracker::DirtyIndices
                          | // No support for index buffer update, not as common.
                          HdMaxChangeTracker::DirtyIndicesSize | HdMaxChangeTracker::DirtyPointsSize
                          | HdMaxChangeTracker::DirtyNormalsSize | HdMaxChangeTracker::DirtyUvsSize
                          | HdMaxChangeTracker::DirtyVertexColorsSize
                          | HdMaxChangeTracker::DirtyTransformsSize
                          | HdMaxChangeTracker::DirtyVisibility
                          | HdMaxChangeTracker::DirtyMaterial);

            if (breakConsolidation) {
                break;
            }
        }

        if (!consolidatedSubsets.empty()) {
            dirtyConsolidatedData.push_back({ primData, consolidatedSubsets });
            consolidationIsDirty |= true;
        }
    }

    // If render data that was previous consolidated, is no longer there, we need to break...
    breakConsolidation |= inConsolidation != currentConsolidation->primToGeom.size();

    if (breakConsolidation) {
        Reset();
        return;
    }

    // Do the update...
    if (consolidationIsDirty) {
        std::unordered_map<MaxSDK::Graphics::Identifier, int> updateMap {};
        for (const auto& primData : dirtyConsolidatedData) {
            for (int i = 0; i < primData.consolidatedSubsets.size(); ++i) {
                auto material = primData.primRenderData->ResolveViewportMaterial(
                    primData.primRenderData->shadedSubsets[primData.consolidatedSubsets[i]],
                    config.displaySettings,
                    false);
                updateMap[material.GetObjectID()]++;
            }
        }

        // Render data which needs updating in the consolidated geometries. For each material, a set
        // of inputs.
        std::unordered_map<MaxSDK::Graphics::Identifier, std::vector<Input>> updateData {};

        for (const auto& entry : updateMap) {
            updateData[entry.first].reserve(entry.second);
        }

        for (const auto& primData : dirtyConsolidatedData) {
            // Figure out the "actual" material subsets from the final material in the viewport.
            // Indeed, if all geomSubsets are bound the same material, we can treat it as a single
            // mesh. We want to, as much as possible, avoid duplication of vertex buffers and so we
            // try to fit subsets with the same materials, on the same consolidated meshes. Therefor
            // we consider the total number of triangles sharing the same material, if multiple
            // subsets use the same materials.
            std::vector<SubsetInfo>                                              subsetInfos;
            std::vector<std::pair<MaxSDK::Graphics::BaseMaterialHandle, size_t>> materialTris;
            ComputeSubsetInfo(*primData.primRenderData, subsetInfos, materialTris);

            for (int i = 0; i < primData.consolidatedSubsets.size(); i++) {
                auto subsetIdx = primData.consolidatedSubsets[i];

                const auto it = std::find_if(
                    materialTris.begin(),
                    materialTris.end(),
                    [&subsetInfos, subsetIdx](const auto& pair) {
                        return pair.first == subsetInfos[subsetIdx].material;
                    });

                const auto numTriWithSameMaterial = it->second;

                std::vector<Input> inputs;
                GenerateInputs(primData.primRenderData, subsetIdx, numTriWithSameMaterial, inputs);
                auto mat = primData.primRenderData->ResolveViewportMaterial(
                    primData.primRenderData->shadedSubsets[primData.consolidatedSubsets[i]],
                    config.displaySettings,
                    false);
                for (const auto& input : inputs) {
                    updateData[mat.GetObjectID()].push_back(input);
                }
            }
        }

        for (const auto& geom : *currentConsolidation->geoms) {
            // Update the vertex buffers. The shaded and wireframe render items share the same
            // buffers. Track whether the consolidated geometry is build from selected prims. We
            // need to know this to select the right render item(s) later.
            UpdateVertexBuffers(
                geom->renderItem.GetRenderGeometry()->GetVertexBuffers(),
                updateData[geom->material.GetObjectID()],
                geom->dataMapping,
                false,
                geom->hasActiveSelection);
        }
    }

    // Remove the previous time from the cache.
    consolidationCache.erase(previousTime);

    // Finally, if we have a valid consolidation, add it to the cache.
    consolidationCache[newTime] = currentConsolidation;
}

MaxSDK::Graphics::RenderItemHandle&
HdMaxConsolidator::ConsolidatedGeom::GetRenderItem(bool wireframe)
{
    if (wireframe) {
        if (hasActiveSelection) {
            return wireframeRenderItemSelection;
        }
        return wireframeRenderItem;
    }

    if (hasActiveSelection) {
        return renderItemSelection;
    }
    return renderItem;
}