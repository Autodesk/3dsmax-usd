#pragma once

#include <pxr/base/vt/dictionary.h>

#include <QtWidgets/qdialog.h>
#include <QtWidgets/qwidget.h>

namespace Ui {
class ExportOptionsDialog;
}

class ExportOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportOptionsDialog(QWidget* parent = nullptr);
    ~ExportOptionsDialog();

    static pxr::VtDictionary ShowOptionsDialog(
        const std::string&       jobContext,
        QWidget*                 parent,
        const pxr::VtDictionary& options);

private:
    const std::unique_ptr<Ui::ExportOptionsDialog> ui;
};