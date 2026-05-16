#include "DebugOptionsWidget.h"

#include "PreferencesDialog.h"
#include "common/Configuration.h"
#include "common/Helpers.h"
#include "ui_DebugOptionsWidget.h"

#include <QComboBox>
#include <QFontDialog>
#include <QLabel>
#include <QShortcut>
#include <QTimer>

DebugOptionsWidget::DebugOptionsWidget(PreferencesDialog *dialog)
    : QDialog(dialog), ui(new Ui::DebugOptionsWidget)
{
    ui->setupUi(this);

    updateDebugPlugin();

    connect(ui->pluginComboBox, &QComboBox::currentTextChanged, this,
            &DebugOptionsWidget::onDebugPluginChanged);
}

DebugOptionsWidget::~DebugOptionsWidget() {}

void DebugOptionsWidget::updateDebugPlugin()
{
    ui->traceContinue->setChecked(Config()->getConfigBool("dbg.trace_continue"));
    connect(ui->traceContinue, &QCheckBox::toggled, this,
            [](bool checked) { Config()->setConfig("dbg.trace_continue", checked); });
    ui->esilBreakOnInvalid->setChecked(Config()->getConfigBool("esil.breakoninvalid"));
    connect(ui->esilBreakOnInvalid, &QCheckBox::toggled, this,
            [](bool checked) { Config()->setConfig("esil.breakoninvalid", checked); });

    disconnect(ui->pluginComboBox, &QComboBox::currentTextChanged, this,
               &DebugOptionsWidget::onDebugPluginChanged);

    const QStringList plugins = Core()->getDebugPlugins();
    for (const QString &str : plugins) {
        ui->pluginComboBox->addItem(str);
    }

    const QString plugin = Core()->getActiveDebugPlugin();
    ui->pluginComboBox->setCurrentText(plugin);

    connect(ui->pluginComboBox, &QComboBox::currentTextChanged, this,
            &DebugOptionsWidget::onDebugPluginChanged);

    const QString stackSize = Core()->getConfig("esil.stack.size");
    ui->stackSize->setText(stackSize);
    ui->stackSize->setPlaceholderText(stackSize);
    const QString stackAddr = Core()->getConfig("esil.stack.addr");
    ui->stackAddr->setText(stackAddr);
    ui->stackAddr->setPlaceholderText(stackAddr);
    connect(ui->stackAddr, &QLineEdit::editingFinished, this, &DebugOptionsWidget::updateStackAddr);
    connect(ui->stackSize, &QLineEdit::editingFinished, this, &DebugOptionsWidget::updateStackSize);
}

void DebugOptionsWidget::onDebugPluginChanged(const QString &plugin)
{
    Core()->setDebugPlugin(plugin);
}

void DebugOptionsWidget::updateStackSize()
{
    const QString newSize = ui->stackSize->text();
    Core()->setConfig("esil.stack.size", newSize);
    ui->stackSize->setPlaceholderText(newSize);
}

void DebugOptionsWidget::updateStackAddr()
{
    const QString newAddr = ui->stackAddr->text();
    Core()->setConfig("esil.stack.addr", newAddr);
    ui->stackAddr->setPlaceholderText(newAddr);
}
