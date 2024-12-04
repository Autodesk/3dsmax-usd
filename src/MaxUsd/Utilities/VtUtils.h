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

namespace MAXUSD_NS_DEF {
namespace Vt {

// VtArrays have copy-on-write semantics. Because of this, non-const access to the raw data
// forces a copy. Unfortunately many function signatures on the 3dsmax side are not const-correct,
// and we must supply non-const pointers. This helper bypasses the copy via a const-cast.
template <class T, class U> T* GetNoCopy(const pxr::VtArray<U>& array)
{
    auto data = const_cast<U*>(array.cdata());
    return reinterpret_cast<T*>(data);
}

} // namespace Vt
} // namespace MAXUSD_NS_DEF