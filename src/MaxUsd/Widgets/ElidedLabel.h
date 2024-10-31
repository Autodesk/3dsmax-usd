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
#include <MaxUsd/MaxUSDAPI.h>

#include <QtWidgets/qframe.h>

/**
 * \brief A simple label class that is able to draw elided text, since the standard
 * QLabel doesn't support it.
 */
class MaxUSDAPI ElidedLabel : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(Qt::TextElideMode elideMode READ elideMode WRITE setElideMode)

public:
    explicit ElidedLabel(QWidget* parent = nullptr);
    explicit ElidedLabel(const QString& text, QWidget* parent = nullptr);

    void    setText(const QString& text);
    QString text() const;

    void setItalic(bool italic);

    Qt::Alignment alignment() const;
    void          setAlignment(Qt::Alignment);

    Qt::TextElideMode elideMode() const;
    void              setElideMode(Qt::TextElideMode);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString           labelText;
    Qt::Alignment     align = Qt::AlignLeft | Qt::AlignVCenter;
    Qt::TextElideMode mode = Qt::ElideRight;
    bool              isItalic = false;
};