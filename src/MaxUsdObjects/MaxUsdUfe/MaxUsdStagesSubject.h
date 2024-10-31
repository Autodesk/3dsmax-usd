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

#include <usdUfe/ufe/StagesSubject.h>

/**
 * \brief Derives the usdUfe stage subject - so that we can use the "global" notifications,
 * targeting all stages.
 */
class MaxUsdStagesSubject : public UsdUfe::StagesSubject
{
public:
    typedef PXR_NS::TfRefPtr<MaxUsdStagesSubject> Ptr;

    /**
     * \brief Constructor.
     */
    MaxUsdStagesSubject();

    /**
     * \brief Destructor.
     */
    ~MaxUsdStagesSubject();

    /**
     * \brief Creates a new MaxUsdStageSubject instance.
     * \return the instance.
     */
    static MaxUsdStagesSubject::Ptr create();

    /**
     * \brief Responds to the global "any stage has changed objects" notification, which
     * we forward to the base class.
     * \param notice The objects changed notice.
     */
    void stageChanged(pxr::UsdNotice::ObjectsChanged const& notice);

private:
    /// Handle for the onStageChange notice so that we can revoke it upon destruction.
    pxr::TfNotice::Key onStageChangeNotice;
};
