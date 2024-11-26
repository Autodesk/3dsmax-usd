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
#include "qFilenameEdit.h"

#include <QtWidgets/QtWidgets>

namespace UfeUi {

class QFilenameEditPrivate
{
public:
    QFilenameEditPrivate(QFilenameEdit* q)
        : q_ptr(q)
    {
    }

    QLineEdit*   lineEdit;
    QToolButton* toolButton;

    QString initialDirectory;
    QString lastDirectory;
    QString baseDirectory;

    QString caption;
    QString filter;

    QFilenameEdit* q_ptr;
    Q_DECLARE_PUBLIC(QFilenameEdit)
};

QFilenameEdit::QFilenameEdit(QWidget* parent /* = nullptr */)
    : QWidget(parent)
    , d_ptr { new QFilenameEditPrivate(this) }
{
    Q_D(QFilenameEdit);
    auto l = new QHBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);

    d->lineEdit = new QLineEdit(this);
    l->addWidget(d->lineEdit, 1);

    d->toolButton = new QToolButton(this);
    d->toolButton->setText(tr("..."));
    l->addWidget(d->toolButton, 0);

    connect(d->toolButton, &QToolButton::clicked, this, [this] {
        Q_D(QFilenameEdit);

        QString lastDir = d->lastDirectory;
        QString lastFile = d->lineEdit->text();
        if (!d->baseDirectory.isEmpty()) {
            auto baseDir = QDir(d->baseDirectory);
            if (baseDir.exists()) {
                lastFile = baseDir.absoluteFilePath(lastFile);
            }
        }
        QFileInfo fi(lastFile);
        if (fi.isFile()) {
            lastDir = fi.absolutePath();
        }
        if (lastDir.isEmpty()) {
            lastDir = d->initialDirectory;
        }
        QFileDialog dlg(this, d->caption, lastDir, d->filter);
        dlg.setFileMode(QFileDialog::FileMode::AnyFile);
        dlg.setAcceptMode(QFileDialog::AcceptMode::AcceptOpen);
        if (fi.exists()) {
            dlg.selectFile(lastFile);
        }
        if (dlg.exec() == QFileDialog::Accepted) {
            auto selectedFiles = dlg.selectedFiles();
            if (selectedFiles.isEmpty()) {
                return;
            }
            QString newFilename = selectedFiles.front();

            if (!d->baseDirectory.isEmpty()) {
                auto baseDir = QDir(d->baseDirectory);
                if (baseDir.exists()) {
                    newFilename = QDir::toNativeSeparators(baseDir.relativeFilePath(newFilename));
                }
            }

            setFilename(newFilename);
            Q_EMIT filenameChanged(newFilename);
        }
    });

    connect(d->lineEdit, &QLineEdit::editingFinished, this, [this] {
        Q_D(QFilenameEdit);
        QString newFilename = d->lineEdit->text();
        setFilename(newFilename);
        Q_EMIT filenameChanged(newFilename);
    });
}

QFilenameEdit::~QFilenameEdit() = default;

QString QFilenameEdit::filename() const
{
    Q_D(const QFilenameEdit);
    return d->lineEdit->text();
}

void QFilenameEdit::setFilename(const QString& filename)
{
    Q_D(QFilenameEdit);
    auto fileInfo = QFileInfo(filename);
    if (d->lineEdit->text() != filename) {
        d->lineEdit->setText(filename);
    }
    if (fileInfo.isAbsolute()) {
        d->lastDirectory = fileInfo.absolutePath();
    } else if (!d->baseDirectory.isEmpty()) {
        auto baseDir = QDir(d->baseDirectory);
        d->lastDirectory = QFileInfo(baseDir.absoluteFilePath(filename)).absolutePath();
    } else {
        d->lastDirectory = d->initialDirectory;
    }
}

QString QFilenameEdit::initialDirectory() const
{
    Q_D(const QFilenameEdit);
    return d->initialDirectory;
}

void QFilenameEdit::setInitialDirectory(const QString& initialDirectory)
{
    Q_D(QFilenameEdit);
    d->initialDirectory = initialDirectory;
    d->lastDirectory = initialDirectory;
}

QString QFilenameEdit::baseDirectory() const
{
    Q_D(const QFilenameEdit);
    return d->baseDirectory;
}

void QFilenameEdit::setBaseDirectory(const QString& baseDirectory)
{
    Q_D(QFilenameEdit);
    d->baseDirectory = baseDirectory;
}

bool QFilenameEdit::readOnly() const
{
    Q_D(const QFilenameEdit);
    return d->lineEdit->isReadOnly();
}

void QFilenameEdit::setReadOnly(bool readOnly)
{
    Q_D(QFilenameEdit);
    d->lineEdit->setReadOnly(readOnly);
    d->toolButton->setDisabled(readOnly);
}

QString QFilenameEdit::caption() const
{
    Q_D(const QFilenameEdit);
    return d->caption;
}

void QFilenameEdit::setCaption(const QString& caption)
{
    Q_D(QFilenameEdit);
    d->caption = caption;
}

QString QFilenameEdit::filter() const
{
    Q_D(const QFilenameEdit);
    return d->filter;
}

void QFilenameEdit::setFilter(const QString& filter)
{
    Q_D(QFilenameEdit);
    d->filter = filter;
}

} // namespace UfeUi
