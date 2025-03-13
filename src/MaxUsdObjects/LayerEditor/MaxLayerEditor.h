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
 * Singleton to open/close the USD Layer Editor.
 */
class MaxLayerEditor
{
public:
    static MaxLayerEditor* Instance();

    ~MaxLayerEditor();

    /**
     * Initialize the 3dsMax USD Layer Editor, setting up some DCC function hooks.
     */
    static void Initialize();

    /**
     * \brief Opens the USDLayerEditor.
     */
    void Open();

    /**
     * \brief Closes the USDLayerEditor.
     */
    void Close();

    /**
     * \brief Opens the given stage in the USD Layer Editor UI.
     * \param stage The USD Stage object to open in the USD Layer Editor UI.
     */
    void OpenStage(USDStageObject* stageObject);

    // Delete the copy/move constructors assignment operators.
    MaxLayerEditor(const MaxLayerEditor&) = delete;
    MaxLayerEditor& operator=(const MaxLayerEditor&) = delete;
    MaxLayerEditor(MaxLayerEditor&&) = delete;
    MaxLayerEditor& operator=(MaxLayerEditor&&) = delete;

private:
    MaxLayerEditor();

    static void OnSceneReset(void* param, NotifyInfo* info);

    static std::unique_ptr<MaxLayerEditor> instance;
};
