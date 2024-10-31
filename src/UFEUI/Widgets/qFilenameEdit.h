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
#ifndef UFEUI_Q_FILENAME_EDIT_H
#define UFEUI_Q_FILENAME_EDIT_H

#include <UFEUI/ufeUiAPI.h>

#include <QtWidgets/QWidget>

namespace UfeUi {

class QFilenameEditPrivate;

/** A widget consisting of a QLineEdit with a QToolbutton to pick a file. */
class UFEUIAPI QFilenameEdit : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString filename READ filename WRITE setFilename NOTIFY filenameChanged);
    Q_PROPERTY(bool readOnly READ readOnly WRITE setReadOnly);
    Q_PROPERTY(QString filter READ filter WRITE setFilter);
    Q_PROPERTY(QString caption READ caption WRITE setCaption);

public:
    explicit QFilenameEdit(QWidget* parent = nullptr);
    ~QFilenameEdit() override;

    QString filename() const;
    void    setFilename(const QString& filename);

    QString initialDirectory() const;
    void    setInitialDirectory(const QString& initialDirectory);

    /** The base directory is the directory to which the filename is seen as
     * relative to. The real (absolute) path to the file would be the
     * combination of the base directory and the relative filename appended. */
    QString baseDirectory() const;
    void    setBaseDirectory(const QString& baseDirectory);

    bool readOnly() const;
    void setReadOnly(bool readOnly);

    QString caption() const;
    void    setCaption(const QString& caption);

    QString filter() const;
    void    setFilter(const QString& filter);

Q_SIGNALS:
    void filenameChanged(const QString& filename);

private:
    const std::unique_ptr<QFilenameEditPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QFilenameEdit)
};

} // namespace UfeUi

#endif // UFEUI_Q_FILENAME_EDIT_H
