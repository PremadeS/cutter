#include "AnalysisOptionsWidget.h"

#include "PreferencesDialog.h"
#include "common/Configuration.h"
#include "common/Helpers.h"
#include "core/MainWindow.h"
#include "ui_AnalysisOptionsWidget.h"

static const QHash<QString, const char *> analysisBoundaries {
    { "io.maps.x", QT_TRANSLATE_NOOP("AnalysisOptionsWidget", "All executable maps") },
    { "io.maps", QT_TRANSLATE_NOOP("AnalysisOptionsWidget", "All maps") },
    { "io.map", QT_TRANSLATE_NOOP("AnalysisOptionsWidget", "Current map") },
    { "raw", QT_TRANSLATE_NOOP("AnalysisOptionsWidget", "Raw") },
    { "bin.section", QT_TRANSLATE_NOOP("AnalysisOptionsWidget", "Current mapped section") },
    { "bin.sections", QT_TRANSLATE_NOOP("AnalysisOptionsWidget", "All mapped sections") },
};

AnalysisOptionsWidget::AnalysisOptionsWidget(PreferencesDialog *dialog)
    : QDialog(dialog), ui(new Ui::AnalysisOptionsWidget), mainWindow(dialog->getMainWindow())
{
    ui->setupUi(this);

    checkboxes = { { ui->autonameCheckbox, "analysis.autoname" },
                   { ui->hasnextCheckbox, "analysis.hasnext" },
                   { ui->jmpRefCheckbox, "analysis.jmp.ref" },
                   { ui->jmpTblCheckbox, "analysis.jmp.tbl" },
                   { ui->pushRetCheckBox, "analysis.pushret" },
                   { ui->typesVerboseCheckBox, "analysis.types.verbose" },
                   { ui->verboseCheckBox, "analysis.verbose" } };

    // Create list of options for the analysis.in selector
    createAnalysisInOptionsList();

    // Connect each checkbox from "checkboxes" to the generic signal "checkboxEnabler"
    for (const ConfigCheckbox &confCheckbox : std::as_const(checkboxes)) {
        const QString val = confCheckbox.config;
        QCheckBox &cb = *confCheckbox.checkBox;
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        connect(confCheckbox.checkBox, &QCheckBox::checkStateChanged, this,
                [val, &cb]() { checkboxEnabler(&cb, val); });
#else
        connect(confCheckbox.checkBox, &QCheckBox::stateChanged, this,
                [val, &cb]() { checkboxEnabler(&cb, val); });
#endif
    }

    ui->analyzePushButton->setToolTip(tr("Analyze the program using Rizin's \"aaa\" command"));
    connect(ui->analyzePushButton, &QPushButton::clicked, mainWindow,
            &MainWindow::onActionAnalyzeTriggered);
    connect<void (QComboBox::*)(int)>(ui->analysisInComboBox, &QComboBox::currentIndexChanged, this,
                                      &AnalysisOptionsWidget::updateAnalysisIn);
    connect<void (QSpinBox::*)(int)>(ui->ptrDepthSpinBox, &QSpinBox::valueChanged, this,
                                     &AnalysisOptionsWidget::updateAnalysisPtrDepth);
    connect(ui->preludeLineEdit, &QLineEdit::textChanged, this,
            &AnalysisOptionsWidget::updateAnalysisPrelude);
    updateAnalysisOptionsFromVars();
}

AnalysisOptionsWidget::~AnalysisOptionsWidget() {}

void AnalysisOptionsWidget::checkboxEnabler(QCheckBox *checkBox, const QString &config)
{
    Config()->setConfig(config, checkBox->isChecked());
}

void AnalysisOptionsWidget::updateAnalysisOptionsFromVars()
{
    for (const ConfigCheckbox &confCheckbox : checkboxes) {
        qhelpers::setCheckedWithoutSignals(confCheckbox.checkBox,
                                           Core()->getConfigb(confCheckbox.config));
    }

    // Update the rest of analysis options that are not checkboxes
    ui->analysisInComboBox->setCurrentIndex(
            ui->analysisInComboBox->findData(Core()->getConfig("analysis.in")));
    ui->ptrDepthSpinBox->setValue(Core()->getConfigi("analysis.ptrdepth"));
    ui->preludeLineEdit->setText(Core()->getConfig("analysis.prelude"));
}

void AnalysisOptionsWidget::updateAnalysisIn(int index)
{
    Config()->setConfig("analysis.in", ui->analysisInComboBox->itemData(index).toString());
}

void AnalysisOptionsWidget::updateAnalysisPtrDepth(int value)
{
    Config()->setConfig("analysis.ptrdepth", value);
}

void AnalysisOptionsWidget::updateAnalysisPrelude(const QString &prelude)
{
    Config()->setConfig("analysis.prelude", prelude);
}

void AnalysisOptionsWidget::createAnalysisInOptionsList()
{
    auto mapIter = analysisBoundaries.cbegin();
    ui->analysisInComboBox->blockSignals(true);
    ui->analysisInComboBox->clear();
    for (; mapIter != analysisBoundaries.cend(); ++mapIter) {
        ui->analysisInComboBox->addItem(tr(mapIter.value()), mapIter.key());
    }
    ui->analysisInComboBox->blockSignals(false);
}
