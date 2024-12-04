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
#include "utils.h"

#include <UFEUI/editCommand.h>

#include <ufe/runTimeMgr.h>
#include <ufe/scene.h>
#include <ufe/undoableCommandMgr.h>

#include <QtCore/QTimer>
#include <QtWidgets/QApplication.h>
#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    // Setup a QT application, to allow instanciating qt widgets in tests.
    QScopedPointer<QApplication> app(new QApplication(argc, argv));

    QTimer::singleShot(0, [&]() {
        ::testing::InitGoogleTest(&argc, argv);

        // Setup test UFE runtime.
        Ufe::RunTimeMgr::Handlers handlers;

        handlers.hierarchyHandler = UfeUiTest::TestHierarchyHandler::create();
        handlers.object3dHandler = std::make_shared<UfeUiTest::TestObject3dHandler>();

        Ufe::RunTimeMgr::instance().register_("testRuntime", handlers);

        class TestScene : public Ufe::Scene
        {
        };
        Ufe::Scene::initializeInstance(std::make_shared<TestScene>());

        class TestUndoableCommandMgr : public Ufe::UndoableCommandMgr
        {
        public:
            TestUndoableCommandMgr() = default;
        };
        Ufe::UndoableCommandMgr::initializeInstance(std::make_shared<TestUndoableCommandMgr>());

        class TestEditCommand : public UfeUi::EditCommand
        {
        public:
            using UfeUi::EditCommand::EditCommand;
            void pre() override { };
            void post() override { };
        };
        auto create = [](const Ufe::Path&                 path,
                         const Ufe::UndoableCommand::Ptr& cmd,
                         const std::string&               cmdString) {
            return std::make_shared<TestEditCommand>(path, cmd, cmdString);
        };
        UfeUi::EditCommand::initializeCreator(create);

        const auto returnCode = RUN_ALL_TESTS();
        app->exit(returnCode);
    });
    return app->exec();
}
