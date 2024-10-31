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
#include "ChannelBuilder.h"

#include "Utilities/Logging.h"
#include "Utilities/TranslationUtils.h"

#include <pxr/usdImaging/usdImaging/tokens.h>

namespace MAXUSD_NS_DEF {

ChannelBuilder::ChannelBuilder(MNMesh* mesh, bool leftHandedOrientation) noexcept
{
    this->mesh = mesh;
    this->leftHandedOrientation = leftHandedOrientation;
}

template <class ArrayType>
void ChannelBuilder::FillChannelDataValues(
    pxr::VtValue&                         values,
    std::function<Point3(ArrayType, int)> point3FromValue)
{
    values.Cast<ArrayType>();
    const auto& primvarData = values.UncheckedGet<ArrayType>();
    for (int i = 0; i < primvarData.size(); ++i) {
        Point3 data = point3FromValue(primvarData, i);
        SetChannelDataValue(i, data);
    }
}

bool ChannelBuilder::Build(
    const pxr::UsdAttribute&   attribute,
    const pxr::TfToken&        interpolation,
    const pxr::UsdGeomPrimvar* primvar,
    const pxr::UsdGeomMesh     usdMesh,
    const pxr::UsdTimeCode&    timeCode)
{
    // Make sure the type's dimension is supported.
    const auto dimension = MaxUsd::GetTypeDimension(attribute.GetTypeName());
    if (dimension > 4) {
        MaxUsd::Log::Warn(
            "Attribute {0} on {1} is of dimension {2} and cannot be imported to a 3dsMax channel.",
            attribute.GetName().GetString(),
            attribute.GetPrim().GetPath().GetString(),
            dimension);
        return false;
    }

    pxr::VtValue values;
    attribute.Get(&values, timeCode);

    // Make sure the data can cast to floats, so that it can be loaded into max channels.
    if (!values.CanCast<pxr::VtVec4fArray>() && !values.CanCast<pxr::VtVec3fArray>()
        && !values.CanCast<pxr::VtVec2fArray>() && !values.CanCast<pxr::VtFloatArray>()) {
        MaxUsd::Log::Warn(
            "Attribute {0} on {1} cannot be cast to a float array, and therefore cannot be "
            "imported to a 3dsMax channel.",
            attribute.GetName().GetString(),
            attribute.GetPrim().GetPath().GetString());
        return false;
    }

    const bool      isIndexed = primvar && primvar->IsIndexed();
    pxr::VtIntArray primvarIndices;
    if (isIndexed) {
        primvar->GetIndices(&primvarIndices, timeCode);
    }

    const auto valueCount = values.GetArraySize();
    if (!MaxUsd::ValidateMappedDataForMesh(
            valueCount, primvarIndices, *mesh, interpolation, isIndexed)) {
        MaxUsd::Log::Warn(
            "The data of {0} on {1} is badly formed, and therefore cannot be imported to a 3dsMax "
            "channel.",
            attribute.GetName().GetString(),
            attribute.GetPrim().GetPath().GetString());
        return false;
    }

    SetupChannel(mesh->FNum(), int(valueCount));

    // Fill the channel's vertex values, padding with zeros if necessary for a type.
    switch (dimension) {
    case 1:
        FillChannelDataValues<pxr::VtFloatArray>(
            values, [](const pxr::VtFloatArray& data, int index) {
                return Point3(data[index], 0.0f, 0.0f);
            });
        break;
    case 2:
        FillChannelDataValues<pxr::VtVec2fArray>(
            values, [](const pxr::VtVec2fArray& data, int index) {
                return Point3(data[index][0], data[index][1], 0.0f);
            });
        break;
    case 3:
        FillChannelDataValues<pxr::VtVec3fArray>(
            values, [](const pxr::VtVec3fArray& data, int index) {
                return Point3(data[index][0], data[index][1], data[index][2]);
            });
        break;
    case 4:
        MaxUsd::Log::Warn(
            "Attribute {0} on {1} is of dimension 4, it will be cropped to 3 dimensions in order "
            "to import it to a 3dsMax channel.",
            attribute.GetName().GetString(),
            attribute.GetPrim().GetPath().GetString());
        FillChannelDataValues<pxr::VtVec4fArray>(
            values, [](const pxr::VtVec4fArray& data, int index) {
                return Point3(data[index][0], data[index][1], data[index][2]); // 4th value ignored.
            });
        break;
    }

    int faceVertexIndex = 0;
    for (int i = 0; i < mesh->FNum(); ++i) {
        const int degree = mesh->F(i)->deg;
        CreateChannelFace(i, degree);
        for (int j = 0; j < degree; ++j) {
            // One value per vertex.
            if (interpolation == pxr::UsdGeomTokens->vertex
                || interpolation == pxr::UsdGeomTokens->varying) {
                const auto vertexIndex = mesh->F(i)->vtx[j];
                const int  dataIndex = isIndexed ? primvarIndices[vertexIndex] : vertexIndex;
                SetChannelFaceData(i, j, dataIndex);
            }
            // One value per face-vertex
            else if (interpolation == pxr::UsdGeomTokens->faceVarying) {
                const int dataIndex = isIndexed ? primvarIndices[faceVertexIndex] : faceVertexIndex;
                SetChannelFaceData(i, j, dataIndex);
                faceVertexIndex++;
            }
            // One value per face.
            else if (interpolation == pxr::UsdGeomTokens->uniform) {
                const auto dataIndex = isIndexed ? primvarIndices[i] : i;
                SetChannelFaceData(i, j, dataIndex);
            }
            // One value for the whole mesh : pxr::UsdGeomTokens->constant
            else {
                SetChannelFaceData(i, j, 0);
            }
        }
        // If the USD geometry has a left handed orientation, make sure the channel faces are
        // flipped to match Max's orientation. We only need to flip the faces for "faceVarying"
        // data. Indeed, for vertex varying data, we fetched the indices from the imported mesh's
        // face, which is already flipped. For "uniform" and "constant" interpolations, the data
        // indices will be the same across the whole face, and so flipping the vertex indice order
        // would make no difference.
        if (leftHandedOrientation && interpolation == pxr::UsdGeomTokens->faceVarying) {
            FlipChannelFace(i);
        }
    }
    FinalizeChannel();
    return true;
}

NormalsBuilder::NormalsBuilder(MNMesh* mesh, bool leftHandedOrientation) noexcept
    : ChannelBuilder(mesh, leftHandedOrientation)
{
}

void NormalsBuilder::SetupChannel(int faceCount, int dataCount)
{
    // Setup explicit normals.
    mesh->SpecifyNormals();
    specifiedNormals = mesh->GetSpecifiedNormals();
    specifiedNormals->SetParent(mesh);

    specifiedNormals->NAlloc(dataCount);
    specifiedNormals->SetNumNormals(dataCount);
    specifiedNormals->SetNumFaces(faceCount);
}

void NormalsBuilder::SetChannelDataValue(int index, const Point3& value)
{
    specifiedNormals->Normal(index) = value;
}

void NormalsBuilder::CreateChannelFace(int faceIndex, int faceDegree)
{
    specifiedNormals->Face(faceIndex).SetDegree(faceDegree);
    specifiedNormals->Face(faceIndex).SpecifyAll();
}

void NormalsBuilder::SetChannelFaceData(int faceIndex, int faceCorner, int dataIndex)
{
    specifiedNormals->SetNormalIndex(faceIndex, faceCorner, dataIndex);
}

void NormalsBuilder::FlipChannelFace(int faceIndex) { specifiedNormals->Face(faceIndex).Flip(); }

void NormalsBuilder::FinalizeChannel()
{
    specifiedNormals->SetAllExplicit();
    specifiedNormals->CheckNormals();
    mesh->InvalidateGeomCache();
}

MapBuilder::MapBuilder(MNMesh* mesh, int channelId, bool leftHandedOrientation) noexcept
    : ChannelBuilder(mesh, leftHandedOrientation)
{
    this->channelIndex = channelId;
}

void MapBuilder::SetupChannel(int faceCount, int dataCount)
{
    mesh->SetMapNum(std::max(channelIndex + 1, mesh->MNum()));
    map = mesh->M(channelIndex);
    map->ClearAndFree();
    map->ClearFlag(MN_DEAD);
    map->setNumFaces(faceCount);
    map->setNumVerts(dataCount);
}

void MapBuilder::SetChannelDataValue(int dataIndex, const Point3& value)
{
    map->v[dataIndex] = value;
}

void MapBuilder::CreateChannelFace(int faceIndex, int faceDegree)
{
    map->F(faceIndex)->SetSize(faceDegree);
}

void MapBuilder::SetChannelFaceData(int faceIndex, int faceCorner, int dataIndex)
{
    map->F(faceIndex)->tv[faceCorner] = dataIndex;
}

void MapBuilder::FlipChannelFace(int faceIndex) { map->F(faceIndex)->Flip(); }

void MapBuilder::FinalizeChannel()
{
    // Nothing to do.
}
} // namespace MAXUSD_NS_DEF
