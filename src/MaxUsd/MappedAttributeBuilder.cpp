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
#include "MappedAttributeBuilder.h"

#include "Utilities/Logging.h"
#include "Utilities/MeshUtils.h"
#include "Utilities/TranslationUtils.h"
#include "Utilities/TypeUtils.h"
#include "Utilities/VtDictionaryUtils.h"

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/sdf/valueTypeName.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usdGeom/primvar.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

#include <algorithm>

PXR_NAMESPACE_OPEN_SCOPE
TF_DEFINE_PUBLIC_TOKENS(MaxUsdMappedAttributeBuilder, MAXUSD_MAPPED_ATTRIBUTE_BUILDER_TOKENS);
PXR_NAMESPACE_CLOSE_SCOPE

using namespace pxr;

namespace MAXUSD_NS_DEF {

/**
 * \brief Wrapper for float arrays of dimension 1, 2 or 3.
 * Useful to reuse code when outputting to attributes of different dimensions.
 */
class FloatNArray
{

    size_t                             dimension;
    std::unique_ptr<pxr::VtFloatArray> floatArray;
    std::unique_ptr<pxr::VtVec2fArray> float2Array;
    std::unique_ptr<pxr::VtVec3fArray> float3Array;

public:
    FloatNArray(size_t dimension, size_t initialSize) noexcept
    {
        // Clamp dimension between 1 and 3.
        this->dimension = std::max(size_t(1), std::min(dimension, size_t(3)));
        switch (dimension) {
        case 1: floatArray = std::make_unique<pxr::VtFloatArray>(initialSize); break;
        case 2: float2Array = std::make_unique<pxr::VtVec2fArray>(initialSize); break;
        case 3: float3Array = std::make_unique<pxr::VtVec3fArray>(initialSize); break;
        }
    }

    bool AssignToAttribute(pxr::UsdAttribute& attribute, pxr::UsdTimeCode timeCode) const
    {
        switch (dimension) {
        case 1: return attribute.Set(*floatArray, timeCode);
        case 2: return attribute.Set(*float2Array, timeCode);
        case 3: return attribute.Set(*float3Array, timeCode);
        }
        return false;
    }

    void Set(size_t index, const Point3& p)
    {
        switch (dimension) {
        case 1: (*floatArray)[index] = p.x; break;
        case 2: (*float2Array)[index] = pxr::GfVec2f(p.x, p.y); break;
        case 3: (*float3Array)[index] = pxr::GfVec3f(p.x, p.y, p.z); break;
        }
    }
};

const VtDictionary& MappedAttributeBuilder::Config::GetDefaultDictionary()
{
    static VtDictionary   defaultDict;
    static std::once_flag once;
    std::call_once(once, []() {
        defaultDict[MaxUsdMappedAttributeBuilder->version] = 1;
        defaultDict[MaxUsdMappedAttributeBuilder->primvarName] = TfToken();
        defaultDict[MaxUsdMappedAttributeBuilder->primvarType]
            = static_cast<int>(Type::TexCoord2fArray);
        defaultDict[MaxUsdMappedAttributeBuilder->autoExpandType] = false;
    });

    return defaultDict;
}

MappedAttributeBuilder::Config::Config(const pxr::VtDictionary& config) { options = config; }

void MappedAttributeBuilder::Config::SetPrimvarName(const pxr::TfToken& primvarName)
{
    options[MaxUsdMappedAttributeBuilder->primvarName] = primvarName;
}

const pxr::TfToken& MappedAttributeBuilder::Config::GetPrimvarName() const
{
    return VtDictionaryGet<TfToken>(options, MaxUsdMappedAttributeBuilder->primvarName);
}

void MappedAttributeBuilder::Config::SetPrimvarType(const Type& type)
{
    options[MaxUsdMappedAttributeBuilder->primvarType] = static_cast<int>(type);
}

MappedAttributeBuilder::Type MappedAttributeBuilder::Config::GetPrimvarType() const
{
    return static_cast<Type>(
        VtDictionaryGet<int>(options, MaxUsdMappedAttributeBuilder->primvarType));
}

void MappedAttributeBuilder::Config::SetAutoExpandType(bool autoExpandType)
{
    options[MaxUsdMappedAttributeBuilder->autoExpandType] = autoExpandType;
}

bool MappedAttributeBuilder::Config::IsAutoExpandType() const
{
    return VtDictionaryGet<bool>(options, MaxUsdMappedAttributeBuilder->autoExpandType);
}

MappedAttributeBuilder::MappedAttributeBuilder(
    MeshFacade&                 maxMesh,
    std::shared_ptr<MappedData> data)
{
    this->faceIndices = maxMesh.FaceIndices();
    this->vertexCount = maxMesh.VertexCount();
    this->mappedData = data;
}

MappedAttributeBuilder::DataLayout MappedAttributeBuilder::InferAttributeDataLayout() const
{
    // If the values are all identical, we can use constant interpolation.
    // Check if all values are equal by comparing each value with the previous one.
    const auto allValuesEqual = mappedData->GetDataCount() == 1
        || std::equal(mappedData->GetData() + 1,
                      mappedData->GetData() + mappedData->GetDataCount(),
                      mappedData->GetData());
    if (allValuesEqual) {
        return DataLayout { pxr::UsdGeomTokens->constant, false };
    }

    const auto& faceDataIndices = *mappedData->GetFaceDataIndices();

    if (faceIndices->size() != faceDataIndices.size()) {
        DbgAssert(0 && "Map channel topology mismatch. Unable to infer layout.");
        return DataLayout { pxr::UsdGeomTokens->faceVarying, true };
    }

    // If the number of mapped data is the same as the number of mapped data indices, we can assume
    // right away that the interpolation is "face varying", and we will not be using an index
    // (unless building one ourselves later on.)
    if (mappedData->GetDataCount() == faceDataIndices.size()) {
        return DataLayout { pxr::UsdGeomTokens->faceVarying, false };
    }

    // If the mapped data indices and vertex indices match perfectly, we can use vertex
    // interpolation directly, without indexing.
    const auto indicesAreEqual = std::equal(
        faceDataIndices.begin(), faceDataIndices.end(), faceIndices->begin(), faceIndices->end());
    if (indicesAreEqual) {
        return DataLayout { pxr::UsdGeomTokens->vertex, false };
    }

    // Check if we can use vertex interpolation. For that, each vertex must be mapped
    // to a unique piece of data.
    std::unordered_map<int, int> vertexIdToDataId;
    bool                         vertexInterpolation = true;
    for (int i = 0; i < faceDataIndices.size(); ++i) {
        auto index = (*faceIndices)[i];
        if (faceDataIndices[i] == index) {
            vertexIdToDataId[index] = faceDataIndices[i];
            continue;
        }
        // The indices may not be in order...
        auto it = vertexIdToDataId.find(index);
        if (it == vertexIdToDataId.end()) {
            vertexIdToDataId[index] = faceDataIndices[i];
        } else {
            // We got the same vertex mapped to different data. We cannot use vertex
            // interpolation.
            if (it->second != faceDataIndices[i]) {
                vertexInterpolation = false;
                break;
            }
        }
    }

    if (vertexInterpolation) {
        // If using vertex interpolation and we have exactly the same amount of data and verties, we
        // shouldn't need an index. Although the data will need to be reordered.
        return DataLayout { pxr::UsdGeomTokens->vertex, mappedData->GetDataCount() != vertexCount };
    }
    return DataLayout { pxr::UsdGeomTokens->faceVarying, true };
}

pxr::SdfValueTypeName MappedAttributeBuilder::GetValueTypeName(const Type& type)
{
    switch (type) {
    case Type::TexCoord2fArray: return pxr::SdfValueTypeNames->TexCoord2fArray;
    case Type::TexCoord3fArray: return pxr::SdfValueTypeNames->TexCoord3fArray;
    case Type::FloatArray: return pxr::SdfValueTypeNames->FloatArray;
    case Type::Float2Array: return pxr::SdfValueTypeNames->Float2Array;
    case Type::Float3Array: return pxr::SdfValueTypeNames->Float3Array;
    case Type::Color3fArray: return pxr::SdfValueTypeNames->Color3fArray;
    }
    return pxr::SdfValueTypeNames->Float3Array;
}

size_t MappedAttributeBuilder::GetTypeDimension(const Type& type)
{
    const auto typeName = GetValueTypeName(type);
    return MaxUsd::GetTypeDimension(typeName);
}

MappedAttributeBuilder::Type
MappedAttributeBuilder::GetEquivalentType(const Type& type, int dimension)
{
    // Equivalent types for different dimensions. When no equivalent type exists, fallback to a
    // floatN array. {type, {dimension1, dimension2, dimension3}}
    static const std::map<Type, std::vector<Type>> equivalentTypes = {
        { Type::FloatArray, { Type::FloatArray, Type::Float2Array, Type::Float3Array } },
        { Type::Float2Array, { Type::FloatArray, Type::Float2Array, Type::Float3Array } },
        { Type::Float3Array, { Type::FloatArray, Type::Float2Array, Type::Float3Array } },
        { Type::TexCoord2fArray,
          { Type::FloatArray, Type::TexCoord2fArray, Type::TexCoord3fArray } },
        { Type::TexCoord3fArray,
          { Type::FloatArray, Type::TexCoord2fArray, Type::TexCoord3fArray } },
        { Type::Color3fArray, { Type::FloatArray, Type::Float2Array, Type::Color3fArray } },
    };
    // Clamp the dimension between 1 and 3.
    const size_t clampedDimension = std::max(1, std::min(dimension, 3));
    Type         resolvedType = Type::Float3Array;
    if (equivalentTypes.find(type) != equivalentTypes.end()) {
        resolvedType = type;
    }
    return equivalentTypes.at(resolvedType)[clampedDimension - 1];
}

bool MappedAttributeBuilder::BuildPrimvar(
    pxr::UsdGeomMesh&       target,
    const Config&           config,
    const pxr::UsdTimeCode& timeCode,
    bool                    animated) const
{

    // Inferring the data layout is costly and the result could change over the course of an
    // animation. Always use faceVarying/indexed when exporting an animation.
    DataLayout layout
        = animated ? DataLayout(pxr::UsdGeomTokens->faceVarying, true) : InferAttributeDataLayout();

    auto typeDimension = GetTypeDimension(config.GetPrimvarType());
    // If autoExpandType is off, use the type directly, otherwise, check if the data fits.
    // If not, use an equivalent higher dimension type.
    Type primvarType = config.GetPrimvarType();
    auto IsAlmostZero = [](float value) { return abs(value) < FLT_EPSILON; };
    if (config.IsAutoExpandType() && typeDimension < 3) {
        int requiredDimension = 1;
        for (int i = 0; i < mappedData->GetDataCount(); i++) {
            if (!IsAlmostZero(mappedData->GetData()[i].z)) {
                requiredDimension = 3;
                break;
            }
            if (requiredDimension < 2 && !IsAlmostZero(mappedData->GetData()[i].y)) {
                requiredDimension = 2;
            }
        }
        if (requiredDimension > typeDimension) {
            primvarType = GetEquivalentType(config.GetPrimvarType(), requiredDimension);
        }
    }

    auto primvar = pxr::UsdGeomPrimvarsAPI(target).CreatePrimvar(
        config.GetPrimvarName(), GetValueTypeName(primvarType), layout.GetInterpolation());

    if (!primvar.IsDefined()) {
        MaxUsd::Log::Error(
            "Unable to create the primvar {0} on {1}. The given name may be a reserved keyword or "
            "invalid.",
            config.GetPrimvarName().data(),
            target.GetPath().GetString());
        return false;
    }
    auto attribute = primvar.GetAttr();
    return PopulateAttribute(attribute, layout, &primvar, timeCode);
}

bool MappedAttributeBuilder::PopulateAttribute(
    pxr::UsdAttribute&      attribute,
    const DataLayout&       layout,
    pxr::UsdGeomPrimvar*    primvar,
    const pxr::UsdTimeCode& timeCode) const
{
    const auto& interpolation = layout.GetInterpolation();
    const bool  indexed = layout.IsIndexed();

    const auto dimension = MaxUsd::GetTypeDimension(attribute.GetTypeName());

    if (interpolation == pxr::UsdGeomTokens->constant) {
        // All values are the same...
        FloatNArray values(dimension, 1);
        values.Set(0, mappedData->GetData()[0]);
        return values.AssignToAttribute(attribute, timeCode);
    }

    if (interpolation == pxr::UsdGeomTokens->faceVarying && !indexed) {
        FloatNArray values(dimension, mappedData->GetDataCount());
        for (int i = 0; i < mappedData->GetFaceDataIndices()->size(); ++i) {
            const int index = (*mappedData->GetFaceDataIndices())[i];
            values.Set(i, mappedData->GetData()[index]);
        }
        return values.AssignToAttribute(attribute, timeCode);
    }

    if (interpolation == pxr::UsdGeomTokens->vertex) {
        // Use an ordered map, as we will want the mapped data indices ordered to match the
        // vertices. Vertex index to mapped data index.
        std::map<int, int> vertexIndexToDataIndex;
        for (int i = 0; i < faceIndices->size(); ++i) {
            vertexIndexToDataIndex[(*faceIndices)[i]] = (*mappedData->GetFaceDataIndices())[i];
            if (vertexIndexToDataIndex.size() == vertexCount) {
                break;
            }
        }

        if (!indexed) {
            FloatNArray values(dimension, mappedData->GetDataCount());
            // Copy over the mapped data...
            for (int i = 0; i < mappedData->GetDataCount(); ++i) {
                values.Set(i, mappedData->GetData()[vertexIndexToDataIndex[i]]);
            }
            return values.AssignToAttribute(attribute, timeCode);
        }

        // Indexed as primvar
        if (primvar != nullptr) {
            pxr::VtIntArray mappedDataIndices;

            // Add the primvar values in the same order. vertexIndexToDataIndex may have missing
            // entries in case of unused vertices, make sure we still add a primvar value for every
            // index.
            int nextRequiredIndex = 0;

            for (auto const& dataIndex : vertexIndexToDataIndex) {
                while (dataIndex.first != nextRequiredIndex) {
                    // Vertex at this index is not used, pad with data index 0.
                    mappedDataIndices.push_back(0);
                    nextRequiredIndex++;
                }
                mappedDataIndices.push_back(dataIndex.second);
                nextRequiredIndex++;
            }

            while (nextRequiredIndex < vertexCount) {
                // Vertex at this index is not used, pad with data index 0.
                mappedDataIndices.push_back(0);
                nextRequiredIndex++;
            }

            FloatNArray values(dimension, mappedData->GetDataCount());
            // Copy the over the data...
            for (int i = 0; i < mappedData->GetDataCount(); ++i) {
                values.Set(i, mappedData->GetData()[i]);
            }
            return values.AssignToAttribute(attribute, timeCode)
                && primvar->SetIndices(mappedDataIndices, timeCode);
        }

        // As attribute
        // No indexing on attributes. Flatten to one data point per vertex and use the same order as
        // vertex indices.
        FloatNArray values(dimension, vertexIndexToDataIndex.size());
        int         idx = 0;
        for (auto const& dataIndex : vertexIndexToDataIndex) {
            values.Set(idx++, mappedData->GetData()[dataIndex.second]);
        }
        return values.AssignToAttribute(attribute, timeCode);
    }

    // Primvar with face varying interpolation.
    if (primvar != nullptr) {
        FloatNArray values(dimension, mappedData->GetDataCount());
        for (size_t i = 0; i < mappedData->GetDataCount(); ++i) {
            values.Set(i, mappedData->GetData()[i]);
        }
        pxr::VtArray<int> mappedDataIndices(mappedData->GetFaceDataIndices()->size());
        std::copy(
            mappedData->GetFaceDataIndices()->begin(),
            mappedData->GetFaceDataIndices()->end(),
            mappedDataIndices.data());
        return values.AssignToAttribute(attribute, timeCode)
            && primvar->SetIndices(mappedDataIndices, timeCode);
    }

    // Attribute with face varying interpolation.
    FloatNArray values(dimension, mappedData->GetFaceDataIndices()->size());
    for (int i = 0; i < mappedData->GetFaceDataIndices()->size(); ++i) {
        auto index = (*mappedData->GetFaceDataIndices())[i];
        values.Set(i, mappedData->GetData()[index]);
    }
    return values.AssignToAttribute(attribute, timeCode);
}

} // namespace MAXUSD_NS_DEF