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

#include <MaxUsd/Translators/ReadJobContext.h>

PXR_NAMESPACE_OPEN_SCOPE

class MaxUsdReadJobContextWrapper
{
public:
    MaxUsdReadJobContextWrapper(MaxUsdReadJobContext& context);

    INT_PTR GetNodeHandle(const SdfPath& path, bool findAncestors) const;

    /// \brief Record maxNode prim as being created path.
    void RegisterNodeHandle(const SdfPath& path, INT_PTR maxNodeHandle);

    bool GetPruneChildren() const;

    void SetPruneChildren(bool prune);

    const UsdStageRefPtr GetStage() const;

    static INT_PTR GetAnimHandle(ReferenceTarget* ref);

    static ReferenceTarget* GetReferenceTarget(INT_PTR handle);

    operator const MaxUsdReadJobContext&() const { return readContext; }

    operator MaxUsdReadJobContext&() { return readContext; }

private:
    MaxUsdReadJobContextWrapper() = delete;
    MaxUsdReadJobContext& readContext;
};

PXR_NAMESPACE_CLOSE_SCOPE