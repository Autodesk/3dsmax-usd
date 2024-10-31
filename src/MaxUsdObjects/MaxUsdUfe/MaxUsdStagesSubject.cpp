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
#include "MaxUsdStagesSubject.h"

MaxUsdStagesSubject::MaxUsdStagesSubject()
{
    pxr::TfWeakPtr<MaxUsdStagesSubject> me(this);
    onStageChangeNotice = pxr::TfNotice::Register(me, &MaxUsdStagesSubject::stageChanged);
}

MaxUsdStagesSubject::~MaxUsdStagesSubject() { pxr::TfNotice::Revoke(onStageChangeNotice); }

/*static*/
MaxUsdStagesSubject::Ptr MaxUsdStagesSubject::create()
{
    return pxr::TfCreateRefPtr(new MaxUsdStagesSubject);
}

void MaxUsdStagesSubject::stageChanged(pxr::UsdNotice::ObjectsChanged const& notice)
{
    StagesSubject::stageChanged(notice, notice.GetStage());
}
