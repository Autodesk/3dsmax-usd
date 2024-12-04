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
#include "TestUtils.h"

#include <MaxUsd/MeshConversion/MaxMeshConversionOptions.h>
#include <MaxUsd/MeshConversion/MeshConverter.h>

#include <pxr/usd/usdGeom/primvarsAPI.h>

#include <gtest/gtest.h>
#include <max.h>

TEST(MapChannelConversionTests, MapChannelsConversion)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/cube");
    auto       usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
    auto       cube = TestUtils::CreateCube(false);
    MaxUsd::MaxMeshConversionOptions options;
    options.SetNormalsMode(MaxUsd::MaxMeshConversionOptions::NormalsMode::None);
    // Export all maps.
    cube.SetMapNum(MAX_MESHMAPS);
    for (int i = -NUM_HIDDENMAPS; i < MAX_MESHMAPS; ++i) {
        // Initialize with defaults depending on the channel.
        // MAP_ALPA : Vertices fully opaque 1.0f.
        // MAP_SHADING : White (1.0f,1.0f,0.f).
        // 0 (vertex color): White (1.0f,1.0f,0.f)
        // UV channels : Basic planar mapping.
        cube.InitMap(i);
        const auto& config = options.GetChannelPrimvarConfig(i);
        // Force the default mapping.
        options.SetChannelPrimvarConfig(
            i,
            MaxUsd::MappedAttributeBuilder::Config(
                config.GetPrimvarName(), config.GetPrimvarType(), false));
    }

    MaxUsd::MeshConverter                         converter;
    std::map<MtlID, pxr::VtIntArray>              materialIdToFacesMap;
    MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
    converter.ConvertToUSDMesh(
        MaxUsd::MeshFacade { &cube },
        stage,
        path,
        options,
        usdMesh,
        pxr::UsdTimeCode::Default(),
        materialIdToFacesMap,
        false,
        intervals);

    const std::vector<pxr::UsdGeomPrimvar> primvars
        = pxr::UsdGeomPrimvarsAPI(usdMesh).GetAuthoredPrimvars();
    ASSERT_EQ(cube.MNum() + NUM_HIDDENMAPS, primvars.size());

    // Test using the default mappings.
    for (int i = -NUM_HIDDENMAPS; i < MAX_MESHMAPS; ++i) {
        const auto& config = options.GetChannelPrimvarConfig(i);

        auto primvar
            = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar(config.GetPrimvarName());
        ASSERT_TRUE(primvar.IsDefined());
        EXPECT_EQ(
            MaxUsd::MappedAttributeBuilder::GetValueTypeName(config.GetPrimvarType()),
            primvar.GetTypeName());

        switch (MaxUsd::MappedAttributeBuilder::GetTypeDimension(config.GetPrimvarType())) {
        case 1: {
            pxr::VtFloatArray values;
            primvar.Get(&values);
            EXPECT_FALSE(values.empty());
            break;
        }
        case 2: {
            pxr::VtVec2fArray values;
            primvar.Get(&values);
            EXPECT_FALSE(values.empty());
            break;
        }
        case 3: {
            pxr::VtVec3fArray values;
            primvar.Get(&values);
            EXPECT_FALSE(values.empty());
            break;
        }
        }
    }

#ifdef TEST_OUTPUT_USD_FILES
    std::string exportPath = TestUtils::GetOutputDirectory();
    exportPath.append("/MapChannelConversionTests/AllChannels.usda");
    stage->Export(exportPath);
#endif
}
// Test
