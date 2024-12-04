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
#include "Mocks/MockStdMat.h"

#include <MaxUsd/Translators/LastResortUSDPreviewSurfaceWriter.h>
#include <MaxUsd/Translators/RegistryHelper.h>
#include <MaxUsd/Translators/ShaderWriterRegistry.h>

#include <pxr/usd/usdShade/shader.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

// ShaderWriter placeholder for testing purposes
// empty ShaderWriter exposing only the methods needed to make the system work
class ShaderWriterTest : public MaxUsdShaderWriter
{
public:
    ShaderWriterTest(Mtl* material, const SdfPath& usdPath, MaxUsdWriteJobContext& jobCtx)
        : MaxUsdShaderWriter(material, usdPath, jobCtx) { };

    static ContextSupport CanExport(const MaxUsd::USDSceneBuilderOptions& exportArgs)
    {
        // the placeholder will only export materials to UsdPreviewSurface
        if (exportArgs.GetConvertMaterialsTo() == UsdImagingTokens->UsdPreviewSurface) {
            return ContextSupport::Supported;
        }
        return ContextSupport::Unsupported;
    }
};

PXR_MAXUSD_REGISTER_SHADER_WRITER(PHYSICALMATERIAL_CLASS_ID, ShaderWriterTest)

TEST(ShaderWriteRegistryTest, FindRegisteredShaderWriter)
{
    // default UsdSceneBuilderOptions converts to UsdPreviewSurface
    MaxUsd::USDSceneBuilderOptions exportArgs;
    exportArgs.SetConvertMaterialsTo(UsdImagingTokens->UsdPreviewSurface);
    MaxUsdShaderWriterRegistry::WriterFactoryFn writerFn
        = MaxUsdShaderWriterRegistry::Find(PHYSICALMATERIAL_CLASS_ID, exportArgs);
    EXPECT_NE(writerFn, nullptr);
}

TEST(ShaderWriteRegistryTest, InvalidContextOnRegisteredShaderWriter)
{
    MaxUsd::USDSceneBuilderOptions exportArgs;
    exportArgs.SetAllMaterialConversions({ TfToken("Arnold") });
    exportArgs.SetConvertMaterialsTo(TfToken("Arnold"));

    MaxUsdShaderWriterRegistry::WriterFactoryFn writerFn
        = MaxUsdShaderWriterRegistry::Find(PHYSICALMATERIAL_CLASS_ID, exportArgs);
    EXPECT_EQ(writerFn, nullptr);
}

TEST(ShaderWriteRegistryTest, NoRegisteredShaderWriterDummyFallback)
{
    MaxUsd::USDSceneBuilderOptions exportArgs;
    exportArgs.SetConvertMaterialsTo(UsdImagingTokens->UsdPreviewSurface);
    MaxUsdShaderWriterRegistry::WriterFactoryFn writerFn = MaxUsdShaderWriterRegistry::Find(
        Class_ID(0xd00f1e00L, 0xbe77e500L) /* PBR Material (Metal / Rough) */, exportArgs);

    auto                  dummyName = std::string {};
    MockStdMat            mtl;
    MaxUsdWriteJobContext ctx { pxr::UsdStage::CreateInMemory(), dummyName, exportArgs, false };
    auto                  writer = writerFn(&mtl, pxr::SdfPath { "/mtl" }, ctx);
    auto dummyWriter = dynamic_cast<LastResortUSDPreviewSurfaceWriter*>(writer.get());

    EXPECT_NE(nullptr, dummyWriter);

    exportArgs.SetUseLastResortUSDPreviewSurfaceWriter(false);
    MaxUsdShaderWriterRegistry::WriterFactoryFn writerFnNoFallback
        = MaxUsdShaderWriterRegistry::Find(
            Class_ID(0xd00f1e00L, 0xbe77e500L) /* PBR Material (Metal / Rough) */, exportArgs);
    EXPECT_EQ(nullptr, writerFnNoFallback);
}

TEST(ShaderWriteRegistryTest, NoWriterDummyFallbackWhenNoTarget)
{
    MaxUsd::USDSceneBuilderOptions              exportArgs;
    MaxUsdShaderWriterRegistry::WriterFactoryFn writerFn = MaxUsdShaderWriterRegistry::Find(
        Class_ID(0xd00f1e00L, 0xbe77e500L) /* PBR Material (Metal / Rough) */, exportArgs);

    EXPECT_EQ(nullptr, writerFn);
}

TEST(ShaderWriterRegistryTest, TargetAgnosticMaterial)
{
    auto mats = MaxUsdShaderWriterRegistry::GetAllTargetAgnosticMaterials();
#if MAX_VERSION_MAJOR < 26
    auto expectedMats = std::vector<Class_ID> {};
    EXPECT_EQ(mats, expectedMats);
#else
    // TODO - to reenable the test, the PXR_PLUGINPATH_NAME env var must be defined
    // with the path location for the default component translators (MaxUsd_Translator json)
    GTEST_SKIP();
    // Material Switcher is target agnostic, Class_ID(0x4ecd74a6, 0x0)
    auto expectedMats = std::vector<Class_ID> { Class_ID(0x4ecd74a6, 0x0) };
    EXPECT_EQ(mats, expectedMats);
#endif
}