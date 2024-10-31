//
// Copyright 2021 Apple
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
// © 2023 Autodesk, Inc. All rights reserved.
//
#pragma once

#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/base/tf/declarePtrs.h>
#include <pxr/base/tf/refPtr.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_REF_PTRS(MaxUsdImportChaser);

/// \brief base class for plugin chasers which are plugins that run after the
/// core usdImport out of 3ds Max.
///
/// Chasers should save necessary data when constructed. The constructor receives
/// the Context. Save what you need from it so that you can make use of the information
/// at the Chaser execution later.
///
/// Chasers should not modify the structure of the USD file. Use this to make
/// small changes or to add attributes, in a non-destructive way, to an imported stage.
class MaxUsdImportChaser : public TfRefBase
{
public:
    ~MaxUsdImportChaser() override { }

    /// Do custom post-processing that needs to run after the main MaxUsd
    /// import loop.
    /// At this point, all data has been authored to the stage (except for
    /// any custom data that you'll author in this step).
    virtual bool MaxUSDAPI PostImport();
};
PXR_NAMESPACE_CLOSE_SCOPE