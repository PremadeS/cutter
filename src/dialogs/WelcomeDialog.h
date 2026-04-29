#ifndef WELCOMEDIALOG_H
#define WELCOMEDIALOG_H

#include <QDialog>

namespace Ui {

/**
 * @class WelcomeDialog
 * @brief The WelcomeDialog class will show the user the Welcome windows
 *  upon first execution of Cutter.
 *
 * Upon first execution of Cutter, the WelcomeDialog would be showed to the user.
 * The Welcome dialog would be showed after a reset of Cutter's preferences by the user.
 */

class WelcomeDialog;
}

class WelcomeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WelcomeDialog(QWidget *parent = nullptr);
    ~WelcomeDialog();

private slots:
    void onThemeComboBoxCurrentIndexChanged(int index);
    void onLanguageCurrentIndexChanged(int index);
    void onCheckUpdateButtonClicked();
    void onContinueButtonClicked();
    void onUpdatesCheckBoxStateChanged(int state);

private:
    Ui::WelcomeDialog *ui;
};

#endif // WELCOMEDIALOG_H
