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
#include "WriteJobContext.h"

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdWriteJobContext::MaxUsdWriteJobContext(
    UsdStageRefPtr                        stage,
    const std::string&                    filename,
    const MaxUsd::USDSceneBuilderOptions& args,
    bool                                  isUSDZ = false)
    : args(args)
    , stage(stage)
    , filename(filename)
    , isUSDZ(isUSDZ)
{
    const fs::path filePath(filename);
    tokensMap["<filename>"] = filePath.filename().replace_extension("").string();
}

void MaxUsdWriteJobContext::SetNodeToPrimMap(const std::map<INode*, SdfPath>& nodesToPrims)
{
    maxNodesToPrims = nodesToPrims;
}

std::string MaxUsdWriteJobContext::ResolveString(const std::string& input) const
{
    std::string result = input;
    for (auto& token : tokensMap) {
        result = MaxUsd::ResolveToken(result, token.first, token.second);
    }
    return result;
}

const MaterialBindings& MaxUsdWriteJobContext::GetMaterialBindings() const
{
    return materialBindings;
}

void MaxUsdWriteJobContext::SetMaterialBindings(const MaterialBindings& materialBindings)
{
    this->materialBindings = materialBindings;
}

PXR_NAMESPACE_CLOSE_SCOPE
