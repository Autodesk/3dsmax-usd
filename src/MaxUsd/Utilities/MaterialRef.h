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
#include <MaxUsd/MaxUSDAPI.h>

#include <MaxUsd.h>
#include <ref.h>
#include <stdmat.h>

namespace MAXUSD_NS_DEF {

/**
 * \brief Wrapper for a Max Material, protecting it from garbage collection by keeping
 * a reference to it.
 */
class MaxUSDAPI MaterialRef : private SingleRefMaker
{
public:
    MaterialRef(MtlBase* material)
    {
        this->SetRef(material);
        this->SetAutoDropRefOnShutdown(AutoDropRefOnShutdown::PrePluginShutdown);
        this->material = material;
    }

    template <class T> T* GetAs() const { return dynamic_cast<T*>(material); }

private:
    MtlBase* material;
};
} // namespace MAXUSD_NS_DEF