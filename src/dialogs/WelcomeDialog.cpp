#include "WelcomeDialog.h"

#include "AboutDialog.h"
#include "CutterConfig.h"
#include "common/Helpers.h"
#include "core/MainWindow.h"
#include "ui_WelcomeDialog.h"

/**
 * @brief Constructs a WelcomeDialog object
 * @param parent
 */
WelcomeDialog::WelcomeDialog(QWidget *parent) : QDialog(parent), ui(new Ui::WelcomeDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
    ui->logoSvgWidget->load(Config()->getLogoFile());
    ui->versionLabel->setText("<font color='#a4a9b2'>" + tr("Version ") + CUTTER_VERSION_FULL
                              + "</font>");
    ui->themeComboBox->setCurrentIndex(Config()->getInterfaceTheme());

    const QSignalBlocker s(ui->updatesCheckBox);
    ui->updatesCheckBox->setChecked(Config()->getAutoUpdateEnabled());

    auto langs = Config()->getAvailableTranslations();
    for (auto &lang : langs) {
        ui->languageComboBox->addItem(lang.name, lang.locale);
    }

    auto matchingLang =
            std::find_if(langs.begin(), langs.end(), [](const Configuration::LangInfo &v) {
                return v.locale == Config()->getCurrLocale();
            });
    if (matchingLang == langs.end()) {
        matchingLang =
                std::find_if(langs.begin(), langs.end(), [](const Configuration::LangInfo &v) {
                    return v.locale.language() == QLocale::English;
                });
    }
    if (matchingLang != langs.end()) {
        ui->languageComboBox->setCurrentIndex(matchingLang - langs.begin());
    } else {
        ui->languageComboBox->setCurrentText("English");
    }

    connect(ui->languageComboBox,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &WelcomeDialog::onLanguageCurrentIndexChanged);

    Config()->adjustColorThemeDarkness();
}

/**
 * @brief Destroys the WelcomeDialog
 */
WelcomeDialog::~WelcomeDialog()
{
    delete ui;
}

/**
 * @brief change Cutter's QT Theme as selected by the user
 * @param index - a Slot being called after theme's value changes its index
 */
void WelcomeDialog::onThemeComboBoxCurrentIndexChanged(int index)
{
    Config()->setInterfaceTheme(index);

    // make sure that Cutter's logo changes its color according to the selected theme
    ui->logoSvgWidget->load(Config()->getLogoFile());
}

/**
 * @brief change Cutter's interface language as selected by the user
 * @param index - a Slot being called after language combo box value changes its index
 */
void WelcomeDialog::onLanguageCurrentIndexChanged(int)
{
    const QVariant language = ui->languageComboBox->currentData();
    if (language.canConvert<QLocale>()) {
        Config()->setLocale(language.toLocale());
    }

    QMessageBox mb(this);
    mb.setWindowTitle(tr("Language settings"));
    mb.setText(tr("Language will be changed after next application start."));
    mb.setIcon(QMessageBox::Information);
    mb.setStandardButtons(QMessageBox::Ok);
    mb.exec();
}

/**
 * @brief show Cutter's About dialog
 */
void WelcomeDialog::onCheckUpdateButtonClicked()
{
    auto *a = new AboutDialog(this);
    a->setAttribute(Qt::WA_DeleteOnClose);
    a->open();
}

/**
 * @brief accept user preferences, close the window and continue Cutter's execution
 */
void WelcomeDialog::onContinueButtonClicked()
{
    accept();
}

void WelcomeDialog::onUpdatesCheckBoxStateChanged(int)
{
    Config()->setAutoUpdateEnabled(!Config()->getAutoUpdateEnabled());
}
