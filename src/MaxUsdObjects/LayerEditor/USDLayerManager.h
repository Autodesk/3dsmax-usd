//
// Copyright 2024 Autodesk
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

struct NotifyInfo;
class USDStageObject;

/**
 * Singleton to manage and save USD Layers.
 */
class USDLayerManager
{
public:
    static USDLayerManager* Instance();

    ~USDLayerManager();

    /**
     * Handles the 3dsMax scene saving. Typically prompting the user for input
     * on what do be done with any dirty USD layers. If called multiple times during
     * a 3dsMax scene save operation, subsequent calls will be no-ops.
     * @return True if successful. False if the save operation should be interrupted.
     */
    bool HandleMaxSceneSave();

    // Delete the copy/move constructors assignment operators.
    USDLayerManager(const USDLayerManager&) = delete;
    USDLayerManager& operator=(const USDLayerManager&) = delete;
    USDLayerManager(USDLayerManager&&) = delete;
    USDLayerManager& operator=(USDLayerManager&&) = delete;

private:
    USDLayerManager();

    static void NotifyFileSave(void* param, NotifyInfo* info);

    static std::unique_ptr<USDLayerManager> instance;

    // Flag keeping track of whether we need to do anything on
    // calls to HandleMaxSceneSave(). Initialized to true when a
    // a scene save operation begins.
    bool mustHandleSave;
};
