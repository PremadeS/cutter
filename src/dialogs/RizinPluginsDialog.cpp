#include "RizinPluginsDialog.h"
#include "ui_RizinPluginsDialog.h"

#include "core/Cutter.h"
#include "common/Helpers.h"
#include "plugins/PluginManager.h"

RizinPluginsDialog::RizinPluginsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::RizinPluginsDialog)
{
    ui->setupUi(this);

    for (const auto &plugin : Core()->getBinPluginDescriptions()) {
        auto *item = new QTreeWidgetItem();
        item->setText(0, plugin.name);
        item->setText(1, plugin.description);
        item->setText(2, plugin.license);
        item->setText(3, plugin.type);
        ui->rzBinTreeWidget->addTopLevelItem(item);
    }
    ui->rzBinTreeWidget->sortByColumn(0, Qt::AscendingOrder);
    qhelpers::adjustColumns(ui->rzBinTreeWidget, 0);

    for (const auto &plugin : Core()->getRIOPluginDescriptions()) {
        auto *item = new QTreeWidgetItem();
        item->setText(0, plugin.name);
        item->setText(1, plugin.description);
        item->setText(2, plugin.license);
        item->setText(3, plugin.permissions);
        ui->rzIoTreeWidget->addTopLevelItem(item);
    }
    ui->rzIoTreeWidget->sortByColumn(0, Qt::AscendingOrder);
    qhelpers::adjustColumns(ui->rzIoTreeWidget, 0);

    for (const auto &plugin : Core()->getRCorePluginDescriptions()) {
        auto *item = new QTreeWidgetItem();
        item->setText(0, plugin.name);
        item->setText(1, plugin.description);
        item->setText(2, plugin.license);
        ui->rzCoreTreeWidget->addTopLevelItem(item);
    }
    ui->rzCoreTreeWidget->sortByColumn(0, Qt::AscendingOrder);
    qhelpers::adjustColumns(ui->rzCoreTreeWidget, 0);

    for (const auto &plugin : Core()->getRAsmPluginDescriptions()) {
        auto *item = new QTreeWidgetItem();
        item->setText(0, plugin.name);
        item->setText(1, plugin.architecture);
        item->setText(2, plugin.cpus);
        item->setText(3, plugin.version);
        item->setText(4, plugin.description);
        item->setText(5, plugin.license);
        item->setText(6, plugin.author);
        ui->rzAsmTreeWidget->addTopLevelItem(item);
    }
    ui->rzAsmTreeWidget->sortByColumn(0, Qt::AscendingOrder);
    qhelpers::adjustColumns(ui->rzAsmTreeWidget, 0);
}

RizinPluginsDialog::~RizinPluginsDialog()
{
    delete ui;
}
