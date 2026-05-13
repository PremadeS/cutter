#ifndef OMNIBAR_H
#define OMNIBAR_H

#include <QLineEdit>

class MainWindow;

/**
 * @brief Main search bar at the top with auto completion
 */
class Omnibar : public QLineEdit
{
    Q_OBJECT
public:
    explicit Omnibar(MainWindow *main, QWidget *parent = nullptr);

    void refresh(const QStringList &flagList);

private slots:
    void onGotoEntryReturnPressed();

    void restoreCompleter();

public slots:
    void clear();

private:
    void setupCompleter();

    MainWindow *main;
    QStringList flags;
};

#endif // OMNIBAR_H
