
#ifndef ASMOPTIONSWIDGET_H
#define ASMOPTIONSWIDGET_H

#include <QDialog>
#include <QPushButton>
#include <memory>

#include "core/Cutter.h"

class PreferencesDialog;

namespace Ui {
class AsmOptionsWidget;
}

class AsmOptionsWidget : public QDialog
{
    Q_OBJECT

public:
    explicit AsmOptionsWidget(PreferencesDialog *dialog);
    ~AsmOptionsWidget();

private:
    std::unique_ptr<Ui::AsmOptionsWidget> ui;
    struct ConfigCheckbox
    {
        QCheckBox *checkBox;
        QString config;
    };
    QList<ConfigCheckbox> checkboxes;

    void triggerAsmOptionsChanged() const;

private slots:
    void resetToDefault();

    void updateAsmOptionsFromVars();

    void onCmtcolSpinBoxValueChanged(int value);

    void onSyntaxComboBoxCurrentIndexChanged(int index);
    void onCaseComboBoxCurrentIndexChanged(int index);
    void onAsmTabsSpinBoxValueChanged(int value);
    void onAsmTabsOffSpinBoxValueChanged(int value);
    void onNbytesSpinBoxValueChanged(int value);

    void onBytesCheckBoxToggled(bool checked);
    void onVarsubCheckBoxToggled(bool checked);
    void onPreviewCheckBoxToggled(bool checked);

    void onButtonBoxClicked(QAbstractButton *button);

    void commentsComboBoxChanged(int index);
    void asmComboBoxChanged(int index);
    void offsetCheckBoxToggled(bool checked);
    void relOffCheckBoxToggled(bool checked);
    void checkboxEnabler(QCheckBox *checkbox, const QString &config);
};

#endif // ASMOPTIONSWIDGET_H
