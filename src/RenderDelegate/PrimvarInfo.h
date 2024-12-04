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

#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

//! Primvar data and interpolation, from the scene delegate
struct PrimvarSource
{
    PrimvarSource(VtValue d, HdInterpolation interp)
        : data(d)
        , interpolation(interp)
    {
    }

    //! a copy-on-write handle to the actual primvar buffer
    VtValue data;

    //! the interpolation mode to be used.
    HdInterpolation interpolation;
};

//! Everything about a primvar
struct PrimvarInfo
{
    PrimvarInfo(const PrimvarSource& source)
        : source(source)
    {
    }

    PrimvarSource source;
};

using PrimvarInfoMap
    = std::unordered_map<TfToken, std::unique_ptr<PrimvarInfo>, TfToken::HashFunctor>;

PXR_NAMESPACE_CLOSE_SCOPE
