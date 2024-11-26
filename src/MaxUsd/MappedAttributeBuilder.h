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
#pragma once

#pragma warning(push)
#pragma warning(disable : 4275) // non dll-interface class 'boost::python::api::object' used as base
                                // for dll-interface struct 'boost::python::detail::list_base'
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvar.h>
#pragma warning(pop)
#include "MaxUSDAPI.h"
#include "MeshConversion/MeshFacade.h"
#include "Utilities/DictionaryOptionProvider.h"

#include <pxr/base/tf/token.h>

#include <MaxUsd.h>

PXR_NAMESPACE_OPEN_SCOPE
// clang-format off
#define MAXUSD_MAPPED_ATTRIBUTE_BUILDER_TOKENS \
	(version) \
	(primvarName) \
	(primvarType) \
	(autoExpandType)
// clang-format on
TF_DECLARE_PUBLIC_TOKENS(
    MaxUsdMappedAttributeBuilder,
    MaxUSDAPI,
    MAXUSD_MAPPED_ATTRIBUTE_BUILDER_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

namespace MAXUSD_NS_DEF {

class MappedAttributeBuilder
{
public:
    enum class MaxUSDAPI Type
    {
        TexCoord2fArray,
        TexCoord3fArray,
        FloatArray,
        Float2Array,
        Float3Array,
        Color3fArray,
    };

    class Config : public DictionaryOptionProvider
    {
    private:
        void SetAutoExpandType(bool autoExpandType);
        void SetPrimvarType(const Type& type);
        // An empty primvar name means the associated channel will not be exported.
        void                            SetPrimvarName(const pxr::TfToken& primvarName);
        static const pxr::VtDictionary& GetDefaultDictionary();

    public:
        MaxUSDAPI Config() { options = GetDefaultDictionary(); }

        MaxUSDAPI Config(const pxr::VtDictionary& config);

        MaxUSDAPI Config(const pxr::TfToken& primvarName, Type type, bool autoExpandType = false)
        {
            options = GetDefaultDictionary();
            SetPrimvarName(primvarName);
            SetPrimvarType(type);
            SetAutoExpandType(autoExpandType);
        }

        MaxUSDAPI bool operator==(const Config& config) const
        {
            return GetPrimvarName() == config.GetPrimvarName()
                && GetPrimvarType() == config.GetPrimvarType()
                && IsAutoExpandType() == config.IsAutoExpandType();
        }
        // An empty primvar name means the associated channel will not be exported.
        MaxUSDAPI const pxr::TfToken& GetPrimvarName() const;
        MaxUSDAPI Type                GetPrimvarType() const;
        MaxUSDAPI bool                IsAutoExpandType() const;
    };

    class MappedData
    {
    private:
        const Point3*                           data;
        size_t                                  dataCount;
        const std::shared_ptr<std::vector<int>> faceDataIndices;

    public:
        MappedData() = delete;
        MaxUSDAPI explicit MappedData(
            const Point3*                           data,
            size_t                                  dataCount,
            const std::shared_ptr<std::vector<int>> faceDataIndices)
            : data(data)
            , dataCount(dataCount)
            , faceDataIndices(faceDataIndices)
        {
        }
        MaxUSDAPI const Point3* GetData() const { return data; }
        MaxUSDAPI const size_t  GetDataCount() const { return dataCount; }
        MaxUSDAPI const std::shared_ptr<std::vector<int>> GetFaceDataIndices() const
        {
            return faceDataIndices;
        }
    };

    class DataLayout
    {
    private:
        pxr::TfToken interpolation;
        bool         indexed;

    public:
        DataLayout() = delete;
        MaxUSDAPI explicit DataLayout(const pxr::TfToken interpolation, bool indexed)
            : interpolation(interpolation)
            , indexed(indexed)
        {
        }
        MaxUSDAPI const pxr::TfToken& GetInterpolation() const { return interpolation; }
        MaxUSDAPI bool                IsIndexed() const { return indexed; }
    };

    /**
     * \brief Constructor.
     * \param maxMesh The max mesh on which the data is mapped.
     * \param data The mapped data.
     */
    MaxUSDAPI MappedAttributeBuilder(MeshFacade& maxMesh, std::shared_ptr<MappedData> data);
    /**
     * \brief Constructor. This overload was added to allow passing a temporary MeshFacade as parameter.
     */
    MaxUSDAPI MappedAttributeBuilder(MeshFacade&& maxMesh, std::shared_ptr<MappedData> data)
        : MappedAttributeBuilder(maxMesh, data)
    {
    }

    /**
     * \brief Creates a new primvar onto the target USD mesh and populate it with the the mapped data.
     * \param target The target USD mesh where to create the new primvar.
     * \param config The configuration for the new primvar.
     * \param timeCode The timecode at which to set the data.
     * \param animated Whether or not the primvar is intended to be animated.
     * \return True on success, false otherwise.
     */
    MaxUSDAPI bool BuildPrimvar(
        pxr::UsdGeomMesh&       target,
        const Config&           config,
        const pxr::UsdTimeCode& timeCode,
        bool                    animated) const;

    /**
     * \brief Populates a given attribute with the mapped data.
     * \param attribute The attribute to populate.
     * \param primvar The primvar associated with the attribute, null if the attribute is not owned by a primvar.
     * \param layout The data layout (interpolation scheme and whether indexing is used)
     * \param timeCode The timecode at which to populate the attribute.
     * \return True on success, false otherwise.
     */
    MaxUSDAPI bool PopulateAttribute(
        pxr::UsdAttribute&      attribute,
        const DataLayout&       layout,
        pxr::UsdGeomPrimvar*    primvar,
        const pxr::UsdTimeCode& timeCode) const;

    /**
     * \brief Infers the data layout i.e. the interpolation scheme and whether it should be indexed or not.
     * \return The inferred data layout.
     */
    MaxUSDAPI DataLayout InferAttributeDataLayout() const;

    /**
     * \brief Returns the SdfValueTypeName associated with this type.
     * \param type The type for which to get the name.
     * \return The value type name.
     */
    static MaxUSDAPI pxr::SdfValueTypeName GetValueTypeName(const Type& type);

    /**
     * \brief Returns the dimension of the given type.
     * \param type The type for which to get the dimension.
     * \return The dimension of the type.
     */
    static MaxUSDAPI size_t GetTypeDimension(const Type& type);

    /**
     * \brief Returns a type equivalent to the given type for a given dimension. For example
     * the equivalent to texcoord2farray for 3 dimensions would be texcoord3farray. When no
     * direct equivalent exists, return a float array of the requested dimension. If an unkown type,
     * fallback to float3Array.
     * \param type The type for which to get an equivalent type.
     * \param dimension The required dimension of the equivalent type. Should be 1, 2, or 3, any other value
     * will be clamped in that range.
     * \return The equivalent type in the given dimension.
     */
    static MaxUSDAPI Type GetEquivalentType(const Type& type, int dimension);

private:
    // Faces indices of the mesh.
    std::shared_ptr<const std::vector<int>> faceIndices;
    int                                     vertexCount;
    std::shared_ptr<MappedData>             mappedData;
};
} // namespace MAXUSD_NS_DEF
