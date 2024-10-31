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
#include "MaxUSDAPI.h"

#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvar.h>

#include <MaxUsd.h>
#include <max.h>

namespace MAXUSD_NS_DEF {

class MaxUSDAPI ChannelBuilder
{
public:
    explicit ChannelBuilder(MNMesh* mesh, bool leftHandedOrientation) noexcept;

    struct Config
    {
        pxr::TfToken primvarName;
    };
    bool Build(
        const pxr::UsdAttribute&   attribute,
        const pxr::TfToken&        interpolation,
        const pxr::UsdGeomPrimvar* primvar,
        pxr::UsdGeomMesh           usdMesh,
        const pxr::UsdTimeCode&    timeCode);

protected:
    virtual void SetupChannel(int faceCount, int dataCount) = 0;
    virtual void SetChannelDataValue(int dataIndex, const Point3&) = 0;
    virtual void CreateChannelFace(int faceIndex, int faceDegree) = 0;
    virtual void SetChannelFaceData(int faceIndex, int faceCorner, int dataIndex) = 0;
    virtual void FlipChannelFace(int faceIndex) = 0;
    virtual void FinalizeChannel() = 0;

    MNMesh* mesh;
    bool    leftHandedOrientation;

private:
    template <class ArrayType>
    void FillChannelDataValues(
        pxr::VtValue&                         values,
        std::function<Point3(ArrayType, int)> point3FromValue);
};

class MaxUSDAPI NormalsBuilder : public ChannelBuilder
{
public:
    explicit NormalsBuilder(MNMesh* mesh, bool leftHandedOrientation = false) noexcept;

protected:
    void         SetupChannel(int faceCount, int dataCount) override;
    void         SetChannelDataValue(int dataIndex, const Point3&) override;
    virtual void CreateChannelFace(int faceIndex, int faceDegree) override;
    virtual void SetChannelFaceData(int faceIndex, int faceCorner, int dataIndex) override;
    virtual void FlipChannelFace(int faceIndex) override;
    virtual void FinalizeChannel() override;

    MNNormalSpec* specifiedNormals;
};

class MaxUSDAPI MapBuilder : public ChannelBuilder
{
public:
    MapBuilder(MNMesh* mesh, int channelId, bool leftHandedOrientation = false) noexcept;

protected:
    void         SetupChannel(int faceCount, int dataCount) override;
    void         SetChannelDataValue(int dataIndex, const Point3&) override;
    virtual void CreateChannelFace(int faceIndex, int faceDegree) override;
    virtual void SetChannelFaceData(int faceIndex, int faceCorner, int dataIndex) override;
    virtual void FlipChannelFace(int faceIndex) override;
    virtual void FinalizeChannel() override;

    MNMap* map;
    int    channelIndex;
};

} // namespace MAXUSD_NS_DEF