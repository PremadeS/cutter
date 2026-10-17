#ifndef CUTTERTEST_H
#define CUTTERTEST_H

#include <QObject>
#include <QtTest/QtTest>

// TODO: doc that this is to be subclasses when writing a (unit?) test
class CutterTest : public QObject
{
    Q_OBJECT

public:
    explicit CutterTest(bool initCore = false, bool initConfig = false, QObject *parent = nullptr);

protected:
    /**
     * @brief Flag controlling whether CutterCore needs to be initialized
     *
     * Setting this to true will automatically initialize Configuration as well
     */
    bool initCore = false;

    /**
     * @brief Flag controlling whether Configuration needs to be initialized
     */
    bool initConfig = false;

    /**
     * @brief Suppresses Qt connection warnings generated when connecting a null sender object
     *
     * Useful for isolating unit tests where slots are invoked directly (e.g., via QMetaObject)
     * or signals are handled manually, avoiding the need to instantiate full sender objects
     * and introducing heavy dependencies.
     *
     * @param signal Member function pointer to the signal (e.g., &CutterCore::seekChanged)
     */
    template<typename SignalFunc>
    void ignoreNullConnect(SignalFunc signal)
    {
        // TODO: should we do this?? I don't think so
        if (initCore) {
            return;
        }

        const QMetaMethod metaSignal = QMetaMethod::fromSignal(signal);
        if (!metaSignal.isValid()) {
            return;
        }

        const QString sigSignature = QString::fromUtf8(metaSignal.methodSignature());
        const QString className = QString::fromUtf8(metaSignal.enclosingMetaObject()->className());

        const QString stringMsg =
                QString("QObject::connect: Cannot connect (null)::%1").arg(sigSignature);
        QTest::ignoreMessage(QtWarningMsg, stringMsg.toUtf8().constData());

        const QString pmfPattern =
                QString("QObject::connect\\(%1, .*\\): invalid nullptr parameter").arg(className);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(pmfPattern));
    }

protected slots:
    virtual void initTestCase();
    virtual void cleanupTestCase();
    virtual void init();
    virtual void cleanup();
};

#endif // CUTTERTEST_H