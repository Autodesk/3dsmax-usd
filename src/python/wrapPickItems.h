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

#include <UFEUI/Views/explorer.h>

#include <ufe/selection.h>

#include <memory.h>
#include <qdialog.h>
#include <qobject.h>

class WrappingPickModeCallback
    : public QObject
    , public UfeUi::Explorer::PickMode::Callback
{
    Q_OBJECT

public:
    WrappingPickModeCallback(const UfeUi::Explorer::PickMode* pickMode);
    ~WrappingPickModeCallback() override;

    void selected(const Ufe::Path& path) override;
    void deSelected(const Ufe::Path& path) override;
    void exited(bool userCancelled) override;

    const Ufe::Selection& selectedPrims() const { return _selection; };

Q_SIGNALS:
    void selectedSignal(const Ufe::Path& path);
    void deSelectedSignal(const Ufe::Path& path);
    void exitedSignal(bool userCancelled);

    void selectionChanged(const Ufe::Selection& selection);

private:
    const UfeUi::Explorer::PickMode* _pickMode = nullptr;
    Ufe::Selection                   _selection;
};

class QGeometryChangedDialog : public QDialog
{
    Q_OBJECT

public:
    QGeometryChangedDialog(QWidget* parent = nullptr);

Q_SIGNALS:
    void geometryChanged(const QRect& geometry);

protected:
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
};