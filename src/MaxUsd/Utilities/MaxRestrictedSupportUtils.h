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

#include "MaxSupportUtils.h"

#include <Graphics/InstanceDisplayGeometry.h>
#include <Graphics/UpdateDisplayContext.h>

#include <MaxUsd/MaxUSDAPI.h>

namespace MaxRestrictedSDKSupport {
namespace Graphics {
namespace ViewportInstancing {

using InstanceDisplayGeometry = MaxSDK::Graphics::ViewportInstancing::InstanceDisplayGeometry;
using InstanceData = MaxSDK::Graphics::ViewportInstancing::InstanceData;

MaxUSDAPI void
CreateInstanceData(InstanceDisplayGeometry* instanceGeometry, const InstanceData& data);
MaxUSDAPI void
UpdateInstanceData(InstanceDisplayGeometry* instanceGeometry, const InstanceData& data);
MaxUSDAPI void SetInstanceDataMatrices(InstanceData& data, std::vector<Matrix3>& matrices);
MaxUSDAPI void ClearInstanceData(InstanceData& data);
} // namespace ViewportInstancing
} // namespace Graphics
} // namespace MaxRestrictedSDKSupport