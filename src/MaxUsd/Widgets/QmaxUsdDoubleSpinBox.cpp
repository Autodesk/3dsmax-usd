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

#include "QmaxUsdDoubleSpinBox.h"

#pragma warning(push)
#pragma warning(disable : 4251 4275 4800)
#include <QtCore/QObject>
#include <QtGui/QtGui>
#include <QtWidgets/QtWidgets>
#pragma warning(pop)

namespace MAXUSD_NS_DEF {

QmaxUsdDoubleSpinBox::QmaxUsdDoubleSpinBox(QWidget* parent)
    : QmaxDoubleSpinBox(parent)
{
}

void QmaxUsdDoubleSpinBox::mousePressEvent(QMouseEvent* event)
{
    // Overriding right-click behavior of the QmaxDoubleSpinBox
    if (event->button() == Qt::RightButton) {
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            reset();
            event->accept();
        }
        return;
    }
    QmaxDoubleSpinBox::mousePressEvent(event);
}

void QmaxUsdDoubleSpinBox::contextMenuEvent(QContextMenuEvent* event)
{
    if (QContextMenuEvent::Mouse == event->reason()
        && (!lineEdit()->rect().contains(event->pos())
            || event->modifiers().testFlag(Qt::ControlModifier))) {
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            // create the ctrl-rmb click otherwise we get trap in the context menu handling
            QMouseEvent pressEvt(
                QEvent::MouseButtonPress,
                event->pos(),
                event->globalPos(),
                Qt::RightButton,
                Qt::RightButton,
                Qt::ControlModifier);
            QCoreApplication::sendEvent(this, &pressEvt);
        } else {
            // Do nothing if we hover the buttons, because right clicking on the buttons
            // resets the spin box's value.
            event->accept();
        }
        return;
    }

    QMenu menu;

    QAction* copyAction = menu.addAction(tr("Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setEnabled(lineEdit()->hasSelectedText());
    QObject::connect(copyAction, &QAction::triggered, lineEdit(), &QLineEdit::copy);

    QAction* pasteAction = menu.addAction(tr("Paste"));
    pasteAction->setShortcut(QKeySequence::Paste);
    pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());
    QObject::connect(pasteAction, &QAction::triggered, lineEdit(), &QLineEdit::paste);

    menu.addSeparator();
    QAction* selectAllAction = menu.addAction(tr("Select All"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    QObject::connect(selectAllAction, &QAction::triggered, lineEdit(), &QLineEdit::selectAll);

    menu.addSeparator();

    QAction* resetAction = menu.addAction(tr("Reset to Default\tCTRL+RMB"));
    connect(resetAction, &QAction::triggered, this, &QmaxDoubleSpinBox::reset);

    Q_EMIT contextMenuCustomization(menu);
    if (!menu.isEmpty()) {
        menu.exec(event->globalPos());
    }
    event->accept();
}
} // namespace MAXUSD_NS_DEF