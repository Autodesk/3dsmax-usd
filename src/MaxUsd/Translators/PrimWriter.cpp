//
// Copyright 2016 Pixar
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
// © 2022 Autodesk, Inc. All rights reserved.
//
#include "PrimWriter.h"

#include "WriteJobContext.h"

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdPrimWriter::MaxUsdPrimWriter(const MaxUsdWriteJobContext& jobCtx, INode* node)
    : writeJobCtx(jobCtx)
    , node(node)
{
}

MaxUsdPrimWriter::~MaxUsdPrimWriter() { }

const MaxUsd::USDSceneBuilderOptions& MaxUsdPrimWriter::GetExportArgs() const
{
    return writeJobCtx.GetArgs();
}

const MaxUsdWriteJobContext& MaxUsdPrimWriter::GetJobContext() const { return writeJobCtx; }

INode* MaxUsdPrimWriter::GetNode() const { return node; }

Interval MaxUsdPrimWriter::GetValidityInterval(const TimeValue& time)
{
    // Default to the validity interval of the max object being exported, roughly speaking
    // this means the writer will be called as the object changes.
    return node->EvalWorldState(time).obj->ObjectValidity(time);
}

const UsdStageRefPtr& MaxUsdPrimWriter::GetUsdStage() const { return writeJobCtx.GetUsdStage(); }

const std::string& MaxUsdPrimWriter::GetFilename() const { return writeJobCtx.GetFilename(); }

boost::python::dict MaxUsdPrimWriter::GetNodesToPrims() const
{
    boost::python::dict allNodesPrims;
    for (const auto& perNode : writeJobCtx.GetNodesToPrimsMap()) {
        allNodesPrims[perNode.first->GetHandle()] = perNode.second;
    }
    return allNodesPrims;
}

PXR_NAMESPACE_CLOSE_SCOPE
