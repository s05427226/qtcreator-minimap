/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qdeclarativeview.h>

#ifdef hz
#undef hz
#endif
#ifdef Q_WS_MAEMO_5
#  include <QMaemo5ValueButton>
#  include <QMaemo5ListPickSelector>
#  include <QWidgetAction>
#  include <QStringListModel>
#  include "ui_recopts_maemo5.h"
#else
#  include "ui_recopts.h"
#endif

#include <qdeclarativeviewobserver.h>
#include <qdeclarativeobserverservice.h>

#include "crumblepath.h"
#include "qmlruntime.h"
#include <qdeclarativecontext.h>
#include <qdeclarativeengine.h>
#include <qdeclarativenetworkaccessmanagerfactory.h>
#include "qdeclarative.h"
#include <QAbstractAnimation>
#ifndef NO_PRIVATE_HEADERS
#include <private/qabstractanimation_p.h>
#endif

#include <QSettings>
#include <QXmlStreamReader>
#include <QBuffer>
#include <QNetworkReply>
#include <QNetworkCookieJar>
#include <QNetworkDiskCache>
#include <QNetworkAccessManager>
#include <QSignalMapper>
#include <QDeclarativeComponent>
#include <QWidget>
#include <QApplication>
#include <QTranslator>
#include <QDir>
#include <QTextBrowser>
#include <QFile>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QProgressDialog>
#include <QProcess>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QTimer>
#include <QGraphicsObject>
#include <QNetworkProxyFactory>
#include <QKeyEvent>
#include <QToolBar>
#include <QMutex>
#include <QMutexLocker>
#include "proxysettings.h"
#include "deviceorientation.h"
#include <QInputDialog>

#ifdef GL_SUPPORTED
#include <QGLWidget>
#endif

#include <qt_private/qdeclarativedebughelper_p.h>

#include <qdeclarativetester.h>
#include "jsdebuggeragent.h"

QT_BEGIN_NAMESPACE

class Runtime : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isActiveWindow READ isActiveWindow NOTIFY isActiveWindowChanged)
    Q_PROPERTY(DeviceOrientation::Orientation orientation READ orientation NOTIFY orientationChanged)

public:
    static Runtime* instance()
    {
        static Runtime *instance = 0;
        if (!instance)
            instance = new Runtime;
        return instance;
    }

    bool isActiveWindow() const { return activeWindow; }
    void setActiveWindow(bool active)
    {
        if (active == activeWindow)
            return;
        activeWindow = active;
        emit isActiveWindowChanged();
    }

    DeviceOrientation::Orientation orientation() const { return DeviceOrientation::instance()->orientation(); }

Q_SIGNALS:
    void isActiveWindowChanged();
    void orientationChanged();

private:
    Runtime(QObject *parent=0) : QObject(parent), activeWindow(false)
    {
        connect(DeviceOrientation::instance(), SIGNAL(orientationChanged()),
                this, SIGNAL(orientationChanged()));
    }

    bool activeWindow;
};



#if defined(Q_WS_MAEMO_5)

class Maemo5PickerAction : public QWidgetAction {
    Q_OBJECT
public:
    Maemo5PickerAction(const QString &text, QActionGroup *actions, QObject *parent)
        : QWidgetAction(parent), m_text(text), m_actions(actions)
    { }

    QWidget *createWidget(QWidget *parent)
    {
	QMaemo5ValueButton *button = new QMaemo5ValueButton(m_text, parent);
	button->setValueLayout(QMaemo5ValueButton::ValueUnderTextCentered);
        QMaemo5ListPickSelector *pick = new QMaemo5ListPickSelector(button);
	button->setPickSelector(pick);
	if (m_actions) {
	    QStringList sl;
	    int curIdx = -1, idx = 0;
	    foreach (QAction *a, m_actions->actions()) {
		sl << a->text();
		if (a->isChecked())
		    curIdx = idx;
		idx++;
            }
	    pick->setModel(new QStringListModel(sl));
	    pick->setCurrentIndex(curIdx);
	} else {
	    button->setEnabled(false);
	}
	connect(pick, SIGNAL(selected(QString)), this, SLOT(emitTriggered()));
	return button;
    }

private slots:
    void emitTriggered()
    {
	QMaemo5ListPickSelector *pick = qobject_cast<QMaemo5ListPickSelector *>(sender());
	if (!pick)
	    return;
	int idx = pick->currentIndex();

	if (m_actions && idx >= 0 && idx < m_actions->actions().count())
	    m_actions->actions().at(idx)->trigger();
    }

private:
    QString m_text;
    QPointer<QActionGroup> m_actions;
};

#endif // Q_WS_MAEMO_5

static struct { const char *name, *args; } ffmpegprofiles[] = {
    {"Maximum Quality", "-sameq"},
    {"High Quality", "-qmax 2"},
    {"Medium Quality", "-qmax 6"},
    {"Low Quality", "-qmax 16"},
    {"Custom ffmpeg arguments", ""},
    {0,0}
};

class RecordingDialog : public QDialog, public Ui::RecordingOptions {
    Q_OBJECT

public:
    RecordingDialog(QWidget *parent) : QDialog(parent)
    {
        setupUi(this);
#ifndef Q_WS_MAEMO_5
        hz->setValidator(new QDoubleValidator(hz));
#endif
        for (int i=0; ffmpegprofiles[i].name; ++i) {
            profile->addItem(ffmpegprofiles[i].name);
        }
    }

    void setArguments(QString a)
    {
        int i;
        for (i=0; ffmpegprofiles[i].args[0]; ++i) {
            if (ffmpegprofiles[i].args == a) {
                profile->setCurrentIndex(i);
                args->setText(QLatin1String(ffmpegprofiles[i].args));
                return;
            }
        }
        customargs = a;
        args->setText(a);
        profile->setCurrentIndex(i);
    }

    QString arguments() const
    {
        int i = profile->currentIndex();
        return ffmpegprofiles[i].args[0] ? QLatin1String(ffmpegprofiles[i].args) : customargs;
    }

    void setOriginalSize(const QSize &s)
    {
        QString str = tr("Original (%1x%2)").arg(s.width()).arg(s.height());

#ifdef Q_WS_MAEMO_5
        sizeCombo->setItemText(0, str);
#else
        sizeOriginal->setText(str);
        if (sizeWidth->value()<=1) {
            sizeWidth->setValue(s.width());
            sizeHeight->setValue(s.height());
        }
#endif
    }

    void showffmpegOptions(bool b)
    {
#ifdef Q_WS_MAEMO_5
        profileLabel->setVisible(b);
        profile->setVisible(b);
        ffmpegHelp->setVisible(b);
        args->setVisible(b);
#else
        ffmpegOptions->setVisible(b);
#endif
    }

    void showRateOptions(bool b)
    {
#ifdef Q_WS_MAEMO_5
        rateLabel->setVisible(b);
        rateCombo->setVisible(b);
#else
        rateOptions->setVisible(b);
#endif
    }

    void setVideoRate(int rate)
    {
#ifdef Q_WS_MAEMO_5
        int idx;
        if (rate >= 60)
            idx = 0;
        else if (rate >= 50)
            idx = 2;
        else if (rate >= 25)
            idx = 3;
        else if (rate >= 24)
            idx = 4;
        else if (rate >= 20)
            idx = 5;
        else if (rate >= 15)
            idx = 6;
        else
            idx = 7;
        rateCombo->setCurrentIndex(idx);
#else
        if (rate == 24)
            hz24->setChecked(true);
        else if (rate == 25)
            hz25->setChecked(true);
        else if (rate == 50)
            hz50->setChecked(true);
        else if (rate == 60)
            hz60->setChecked(true);
        else {
            hzCustom->setChecked(true);
            hz->setText(QString::number(rate));
        }
#endif
    }

    int videoRate() const
    {
#ifdef Q_WS_MAEMO_5
        switch (rateCombo->currentIndex()) {
            case 0: return 60;
            case 1: return 50;
            case 2: return 25;
            case 3: return 24;
            case 4: return 20;
            case 5: return 15;
            case 7: return 10;
            default: return 60;
        }
#else
        if (hz24->isChecked())
            return 24;
        else if (hz25->isChecked())
            return 25;
        else if (hz50->isChecked())
            return 50;
        else if (hz60->isChecked())
            return 60;
        else {
            return hz->text().toInt();
        }
#endif
    }

    QSize videoSize() const
    {
#ifdef Q_WS_MAEMO_5
        switch (sizeCombo->currentIndex()) {
            case 0: return QSize();
            case 1: return QSize(640,480);
            case 2: return QSize(320,240);
            case 3: return QSize(1280,720);
            default: return QSize();
        }
#else
        if (sizeOriginal->isChecked())
            return QSize();
        else if (size720p->isChecked())
            return QSize(1280,720);
        else if (sizeVGA->isChecked())
            return QSize(640,480);
        else if (sizeQVGA->isChecked())
            return QSize(320,240);
        else
            return QSize(sizeWidth->value(), sizeHeight->value());
#endif
    }



private slots:
    void pickProfile(int i)
    {
        if (ffmpegprofiles[i].args[0]) {
            args->setText(QLatin1String(ffmpegprofiles[i].args));
        } else {
            args->setText(customargs);
        }
    }

    void storeCustomArgs(QString s)
    {
        setArguments(s);
    }

private:
    QString customargs;
};

class PersistentCookieJar : public QNetworkCookieJar {
public:
    PersistentCookieJar(QObject *parent) : QNetworkCookieJar(parent) { load(); }
    ~PersistentCookieJar() { save(); }

    virtual QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const
    {
        QMutexLocker lock(&mutex);
        return QNetworkCookieJar::cookiesForUrl(url);
    }

    virtual bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
    {
        QMutexLocker lock(&mutex);
        return QNetworkCookieJar::setCookiesFromUrl(cookieList, url);
    }

private:
    void save()
    {
        QMutexLocker lock(&mutex);
        QList<QNetworkCookie> list = allCookies();
        QByteArray data;
        foreach (QNetworkCookie cookie, list) {
            if (!cookie.isSessionCookie()) {
                data.append(cookie.toRawForm());
                data.append("\n");
            }
        }
        QSettings settings;
        settings.setValue("Cookies",data);
    }

    void load()
    {
        QMutexLocker lock(&mutex);
        QSettings settings;
        QByteArray data = settings.value("Cookies").toByteArray();
        setAllCookies(QNetworkCookie::parseCookies(data));
    }

    mutable QMutex mutex;
};

class NetworkAccessManagerFactory : public QDeclarativeNetworkAccessManagerFactory
{
public:
    NetworkAccessManagerFactory() : cacheSize(0) {}
    ~NetworkAccessManagerFactory() {}

    QNetworkAccessManager *create(QObject *parent);

    void setupProxy(QNetworkAccessManager *nam)
    {
        class SystemProxyFactory : public QNetworkProxyFactory
        {
        public:
            virtual QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query)
            {
                QString protocolTag = query.protocolTag();
                if (httpProxyInUse && (protocolTag == "http" || protocolTag == "https")) {
                    QList<QNetworkProxy> ret;
                    ret << httpProxy;
                    return ret;
                }
#ifdef Q_OS_WIN
		// systemProxyForQuery can take insanely long on Windows (QTBUG-10106)
                return QNetworkProxyFactory::proxyForQuery(query);
#else
                return QNetworkProxyFactory::systemProxyForQuery(query);
#endif
            }
            void setHttpProxy (QNetworkProxy proxy)
            {
                httpProxy = proxy;
                httpProxyInUse = true;
            }
            void unsetHttpProxy ()
            {
                httpProxyInUse = false;
            }
        private:
            bool httpProxyInUse;
            QNetworkProxy httpProxy;
        };

        SystemProxyFactory *proxyFactory = new SystemProxyFactory;
        if (ProxySettings::httpProxyInUse())
            proxyFactory->setHttpProxy(ProxySettings::httpProxy());
        else
            proxyFactory->unsetHttpProxy();
        nam->setProxyFactory(proxyFactory);
    }

    void setCacheSize(int size) {
        if (size != cacheSize) {
            cacheSize = size;
        }
    }

    static PersistentCookieJar *cookieJar;
    QMutex mutex;
    int cacheSize;
};

PersistentCookieJar *NetworkAccessManagerFactory::cookieJar = 0;

static void cleanup_cookieJar()
{
    delete NetworkAccessManagerFactory::cookieJar;
    NetworkAccessManagerFactory::cookieJar = 0;
}

QNetworkAccessManager *NetworkAccessManagerFactory::create(QObject *parent)
{
    QMutexLocker lock(&mutex);
    QNetworkAccessManager *manager = new QNetworkAccessManager(parent);
    if (!cookieJar) {
        qAddPostRoutine(cleanup_cookieJar);
        cookieJar = new PersistentCookieJar(0);
    }
    manager->setCookieJar(cookieJar);
    cookieJar->setParent(0);
    setupProxy(manager);
    if (cacheSize > 0) {
        QNetworkDiskCache *cache = new QNetworkDiskCache;
        cache->setCacheDirectory(QDir::tempPath()+QLatin1String("/qml-viewer-network-cache"));
        cache->setMaximumCacheSize(cacheSize);
        manager->setCache(cache);
    } else {
        manager->setCache(0);
    }
    qDebug() << "created new network access manager for" << parent;
    return manager;
}

//
// Event filter that ensures the crumble path width is always the canvas width
//
class CrumblePathResizer : public QObject
{
    Q_OBJECT
public:
    CrumblePathResizer(CrumblePath *crumblePathWidget, QObject *parent = 0) :
        QObject(parent),
        m_crumblePathWidget(crumblePathWidget)
    {
    }

    bool eventFilter(QObject *obj, QEvent *event)
    {
        if (event->type() == QEvent::Resize) {
            QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
            m_crumblePathWidget->resize(resizeEvent->size().width(), m_crumblePathWidget->height());
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QWidget *m_crumblePathWidget;
};

QString QDeclarativeViewer::getVideoFileName()
{
    QString title = convertAvailable || ffmpegAvailable ? tr("Save Video File") : tr("Save PNG Frames");
    QStringList types;
    if (ffmpegAvailable) types += tr("Common Video files")+QLatin1String(" (*.avi *.mpeg *.mov)");
    if (convertAvailable) types += tr("GIF Animation")+QLatin1String(" (*.gif)");
    types += tr("Individual PNG frames")+QLatin1String(" (*.png)");
    if (ffmpegAvailable) types += tr("All ffmpeg formats (*.*)");
    return QFileDialog::getSaveFileName(this, title, "", types.join(";; "));
}

QDeclarativeViewer::QDeclarativeViewer(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
      , loggerWindow(new LoggerWidget(this))
      , frame_stream(0)
      , orientation(0)
      , showWarningsWindow(0)
      , designModeBehaviorAction(0)
      , m_scriptOptions(0)
      , tester(0)
      , useQmlFileBrowser(true)
      , m_centralWidget(0)
      , m_crumblePathWidget(0)
      , translator(0)
{
    QDeclarativeViewer::registerTypes();
    setWindowTitle(tr("Qt QML Viewer"));
#ifdef Q_WS_MAEMO_5
    setAttribute(Qt::WA_Maemo5StackedWindow);
//    setPalette(QApplication::palette("QLabel"));
#endif

    devicemode = false;
    canvas = 0;
    record_autotime = 0;
    record_rate = 50;
    record_args += QLatin1String("-sameq");

    recdlg = new RecordingDialog(this);
    connect(recdlg->pickfile, SIGNAL(clicked()), this, SLOT(pickRecordingFile()));
    senseFfmpeg();
    senseImageMagick();
    if (!ffmpegAvailable)
        recdlg->showffmpegOptions(false);
    if (!ffmpegAvailable && !convertAvailable)
        recdlg->showRateOptions(false);
    QString warn;
    if (!ffmpegAvailable) {
        if (!convertAvailable)
            warn = tr("ffmpeg and ImageMagick not available - no video output");
        else
            warn = tr("ffmpeg not available - GIF and PNG outputs only");
        recdlg->warning->setText(warn);
    } else {
        recdlg->warning->hide();
    }

    canvas = new QDeclarativeView(this);
    observer = new QmlJSDebugger::QDeclarativeViewObserver(canvas, this);
    new QmlJSDebugger::JSDebuggerAgent(canvas->engine());
    if (!(flags & Qt::FramelessWindowHint)) {
        m_crumblePathWidget = new CrumblePath(canvas);
#ifndef Q_WS_MAC
        m_crumblePathWidget->setStyleSheet("QWidget { border-bottom: 1px solid black; }");
#endif
        m_crumblePathWidget->setVisible(observer->designModeBehavior());

        // CrumblePath is not in a layout, so that it overlays the central widget
        // The event filter ensures that its width stays in sync nevertheless
        CrumblePathResizer *resizer = new CrumblePathResizer(m_crumblePathWidget, m_crumblePathWidget);
        canvas->installEventFilter(resizer);
    }

    m_centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_centralWidget);
    layout->setMargin(0);
    layout->setSpacing(0);


    layout->addWidget(canvas);
    m_centralWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    canvas->setAttribute(Qt::WA_OpaquePaintEvent);
    canvas->setAttribute(Qt::WA_NoSystemBackground);

    canvas->setFocus();

    QObject::connect(observer, SIGNAL(reloadRequested()), this, SLOT(reload()));
    QObject::connect(canvas, SIGNAL(sceneResized(QSize)), this, SLOT(sceneResized(QSize)));
    QObject::connect(canvas, SIGNAL(statusChanged(QDeclarativeView::Status)), this, SLOT(statusChanged()));
    if (m_crumblePathWidget) {
        QObject::connect(observer, SIGNAL(inspectorContextCleared()), m_crumblePathWidget, SLOT(clear()));
        QObject::connect(observer, SIGNAL(inspectorContextPushed(QString)), m_crumblePathWidget, SLOT(pushElement(QString)));
        QObject::connect(observer, SIGNAL(inspectorContextPopped()), m_crumblePathWidget, SLOT(popElement()));
        QObject::connect(m_crumblePathWidget, SIGNAL(elementClicked(int)), observer, SLOT(setObserverContext(int)));
        QObject::connect(observer, SIGNAL(designModeBehaviorChanged(bool)), m_crumblePathWidget, SLOT(setVisible(bool)));
    }
    QObject::connect(canvas->engine(), SIGNAL(quit()), QCoreApplication::instance (), SLOT(quit()));

    QObject::connect(warningsWidget(), SIGNAL(opened()), this, SLOT(warningsWidgetOpened()));
    QObject::connect(warningsWidget(), SIGNAL(closed()), this, SLOT(warningsWidgetClosed()));

    if (!(flags & Qt::FramelessWindowHint)) {
        createMenu();
        changeOrientation(orientation->actions().value(0));
    } else {
        setMenuBar(0);
    }

    setCentralWidget(m_centralWidget);

    namFactory = new NetworkAccessManagerFactory;
    canvas->engine()->setNetworkAccessManagerFactory(namFactory);

    connect(&autoStartTimer, SIGNAL(timeout()), this, SLOT(autoStartRecording()));
    connect(&autoStopTimer, SIGNAL(timeout()), this, SLOT(autoStopRecording()));
    connect(&recordTimer, SIGNAL(timeout()), this, SLOT(recordFrame()));
    connect(DeviceOrientation::instance(), SIGNAL(orientationChanged()),
            this, SLOT(orientationChanged()), Qt::QueuedConnection);
    autoStartTimer.setSingleShot(true);
    autoStopTimer.setSingleShot(true);
    recordTimer.setSingleShot(false);

    QObject::connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(appAboutToQuit()));
}

QDeclarativeViewer::~QDeclarativeViewer()
{
    delete loggerWindow;
    canvas->engine()->setNetworkAccessManagerFactory(0);
    delete namFactory;
}

void QDeclarativeViewer::setDesignModeBehavior(bool value)
{
    if (designModeBehaviorAction)
        designModeBehaviorAction->setChecked(value);
    observer->setDesignModeBehavior(value);
}

void QDeclarativeViewer::enableExperimentalGestures()
{
    canvas->viewport()->grabGesture(Qt::TapGesture,Qt::DontStartGestureOnChildren|Qt::ReceivePartialGestures|Qt::IgnoredGesturesPropagateToParent);
    canvas->viewport()->grabGesture(Qt::TapAndHoldGesture,Qt::DontStartGestureOnChildren|Qt::ReceivePartialGestures|Qt::IgnoredGesturesPropagateToParent);
    canvas->viewport()->grabGesture(Qt::PanGesture,Qt::DontStartGestureOnChildren|Qt::ReceivePartialGestures|Qt::IgnoredGesturesPropagateToParent);
    canvas->viewport()->grabGesture(Qt::PinchGesture,Qt::DontStartGestureOnChildren|Qt::ReceivePartialGestures|Qt::IgnoredGesturesPropagateToParent);
    canvas->viewport()->grabGesture(Qt::SwipeGesture,Qt::DontStartGestureOnChildren|Qt::ReceivePartialGestures|Qt::IgnoredGesturesPropagateToParent);
    canvas->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
}

QDeclarativeView *QDeclarativeViewer::view() const
{
    return canvas;
}

LoggerWidget *QDeclarativeViewer::warningsWidget() const
{
    return loggerWindow;
}

void QDeclarativeViewer::createMenu()
{
    QAction *openAction = new QAction(tr("&Open..."), this);
    openAction->setShortcut(QKeySequence("Ctrl+O"));
    connect(openAction, SIGNAL(triggered()), this, SLOT(openFile()));

    QAction *reloadAction = new QAction(tr("&Reload"), this);
    reloadAction->setShortcut(QKeySequence("Ctrl+R"));
    connect(reloadAction, SIGNAL(triggered()), this, SLOT(reload()));

    QAction *snapshotAction = new QAction(tr("&Take Snapshot"), this);
    snapshotAction->setShortcut(QKeySequence("F3"));
    connect(snapshotAction, SIGNAL(triggered()), this, SLOT(takeSnapShot()));

    recordAction = new QAction(tr("Start Recording &Video"), this);
    recordAction->setShortcut(QKeySequence("F9"));
    connect(recordAction, SIGNAL(triggered()), this, SLOT(toggleRecordingWithSelection()));
#ifdef NO_PRIVATE_HEADERS
    recordAction->setEnabled(false);
#endif

    QAction *recordOptions = new QAction(tr("Video &Options..."), this);
    connect(recordOptions, SIGNAL(triggered()), this, SLOT(chooseRecordingOptions()));

    QMenu *playSpeedMenu = new QMenu(tr("Animation Speed"), this);
    QActionGroup *playSpeedMenuActions = new QActionGroup(this);
    playSpeedMenuActions->setExclusive(true);

    QAction *speedAction = playSpeedMenu->addAction(tr("1x"), this, SLOT(changeAnimationSpeed()));
    speedAction->setCheckable(true);
    speedAction->setChecked(true);
    animationSpeed = 1.0f;
    speedAction->setData(1.0f);
    playSpeedMenuActions->addAction(speedAction);

    speedAction = playSpeedMenu->addAction(tr("0.5x"), this, SLOT(changeAnimationSpeed()));
    speedAction->setCheckable(true);
    speedAction->setData(2.0f);
    playSpeedMenuActions->addAction(speedAction);

    speedAction = playSpeedMenu->addAction(tr("0.25x"), this, SLOT(changeAnimationSpeed()));
    speedAction->setCheckable(true);
    speedAction->setData(4.0f);
    playSpeedMenuActions->addAction(speedAction);

    speedAction = playSpeedMenu->addAction(tr("0.125x"), this, SLOT(changeAnimationSpeed()));
    speedAction->setCheckable(true);
    speedAction->setData(8.0f);
    playSpeedMenuActions->addAction(speedAction);

    speedAction = playSpeedMenu->addAction(tr("0.1x"), this, SLOT(changeAnimationSpeed()));
    speedAction->setCheckable(true);
    speedAction->setData(10.0f);
    playSpeedMenuActions->addAction(speedAction);

    pauseAnimationsAction = playSpeedMenu->addAction(tr("Pause"), this, SLOT(setAnimationsPaused(bool)));
    pauseAnimationsAction->setCheckable(true);
    pauseAnimationsAction->setShortcut(QKeySequence("Ctrl+."));

    animationStepAction = playSpeedMenu->addAction(tr("Pause and step"), this, SLOT(stepAnimations()));
    animationStepAction->setShortcut(QKeySequence("Ctrl+,"));

    animationSetStepAction = playSpeedMenu->addAction(tr("Set step"), this, SLOT(setAnimationStep()));
    m_stepSize = 40;

    QAction *playSpeedAction = new QAction(tr("Animations"), this);
    playSpeedAction->setMenu(playSpeedMenu);

    showWarningsWindow = new QAction(tr("Show Warnings"), this);
    showWarningsWindow->setCheckable((true));
    showWarningsWindow->setChecked(loggerWindow->isVisible());
    connect(showWarningsWindow, SIGNAL(triggered(bool)), this, SLOT(showWarnings(bool)));

    designModeBehaviorAction = new QAction(tr("&Observer Mode"), this);
    designModeBehaviorAction->setShortcut(QKeySequence("Ctrl+D"));
    designModeBehaviorAction->setCheckable(true);
    designModeBehaviorAction->setChecked(observer->designModeBehavior());
    designModeBehaviorAction->setEnabled(QmlJSDebugger::QDeclarativeObserverService::hasDebuggingClient());
    connect(designModeBehaviorAction, SIGNAL(triggered(bool)), this, SLOT(setDesignModeBehavior(bool)));
    connect(observer, SIGNAL(designModeBehaviorChanged(bool)), designModeBehaviorAction, SLOT(setChecked(bool)));
    connect(QmlJSDebugger::QDeclarativeObserverService::instance(), SIGNAL(debuggingClientChanged(bool)), designModeBehaviorAction, SLOT(setEnabled(bool)));

    QAction *proxyAction = new QAction(tr("HTTP &Proxy..."), this);
    connect(proxyAction, SIGNAL(triggered()), this, SLOT(showProxySettings()));

    QAction *fullscreenAction = new QAction(tr("Full Screen"), this);
    fullscreenAction->setCheckable(true);
    connect(fullscreenAction, SIGNAL(triggered()), this, SLOT(toggleFullScreen()));

    QAction *rotateOrientation = new QAction(tr("Rotate orientation"), this);
    rotateOrientation->setShortcut(QKeySequence("Ctrl+T"));
    connect(rotateOrientation, SIGNAL(triggered()), this, SLOT(rotateOrientation()));

    orientation = new QActionGroup(this);
    orientation->setExclusive(true);
    connect(orientation, SIGNAL(triggered(QAction*)), this, SLOT(changeOrientation(QAction*)));

    QAction *portraitAction = new QAction(tr("Portrait"), this);
    portraitAction->setCheckable(true);
    QAction *landscapeAction = new QAction(tr("Landscape"), this);
    landscapeAction->setCheckable(true);
    QAction *portraitInvAction = new QAction(tr("Portrait (inverted)"), this);
    portraitInvAction->setCheckable(true);
    QAction *landscapeInvAction = new QAction(tr("Landscape (inverted)"), this);
    landscapeInvAction->setCheckable(true);

    QAction *aboutAction = new QAction(tr("&About Qt..."), this);
    connect(aboutAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

    QAction *quitAction = new QAction(tr("&Quit"), this);
    quitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

    QMenuBar *menu = menuBar();
    if (!menu)
	return;

#if defined(Q_WS_MAEMO_5)
    menu->addAction(openAction);
    menu->addAction(reloadAction);

    menu->addAction(snapshotAction);
    menu->addAction(recordAction);

    menu->addAction(recordOptions);
    menu->addAction(proxyAction);

    menu->addAction(playSpeedMenu);
    menu->addAction(showWarningsWindow);

    orientation->addAction(landscapeAction);
    orientation->addAction(portraitAction);
    menu->addAction(new Maemo5PickerAction(tr("Set orientation"), orientation, this));
    menu->addAction(fullscreenAction);
    return;
#endif // Q_WS_MAEMO_5

    QMenu *fileMenu = menu->addMenu(tr("&File"));
    fileMenu->addAction(openAction);
    fileMenu->addAction(reloadAction);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction);

#if !defined(Q_OS_SYMBIAN)
    QMenu *recordMenu = menu->addMenu(tr("&Recording"));
    recordMenu->addAction(snapshotAction);
    recordMenu->addAction(recordAction);

    QMenu *debugMenu = menu->addMenu(tr("&Debugging"));
    debugMenu->addAction(playSpeedAction);
    debugMenu->addAction(showWarningsWindow);
    debugMenu->addAction(designModeBehaviorAction);
#endif // ! Q_OS_SYMBIAN

    QMenu *settingsMenu = menu->addMenu(tr("S&ettings"));
    settingsMenu->addAction(proxyAction);
#if !defined(Q_OS_SYMBIAN)
    settingsMenu->addAction(recordOptions);
    settingsMenu->addMenu(loggerWindow->preferencesMenu());
#else // ! Q_OS_SYMBIAN
    settingsMenu->addAction(fullscreenAction);
#endif // Q_OS_SYMBIAN
    settingsMenu->addAction(rotateOrientation);

    QMenu *propertiesMenu = settingsMenu->addMenu(tr("Properties"));

    orientation->addAction(portraitAction);
    orientation->addAction(landscapeAction);
    orientation->addAction(portraitInvAction);
    orientation->addAction(landscapeInvAction);
    propertiesMenu->addActions(orientation->actions());

    QMenu *helpMenu = menu->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAction);
}

void QDeclarativeViewer::showProxySettings()
{
    ProxySettings settingsDlg (this);

    connect (&settingsDlg, SIGNAL (accepted()), this, SLOT (proxySettingsChanged ()));

    settingsDlg.exec();
}

void QDeclarativeViewer::proxySettingsChanged()
{
    reload ();
}

void QDeclarativeViewer::rotateOrientation()
{
    QAction *current = orientation->checkedAction();
    QList<QAction *> actions = orientation->actions();
    int index = actions.indexOf(current);
    if (index < 0)
        return;

    QAction *newOrientation = actions[(index + 1) % actions.count()];
    changeOrientation(newOrientation);
}

void QDeclarativeViewer::toggleFullScreen()
{
    if (isFullScreen())
        showMaximized();
    else
        showFullScreen();
}

void QDeclarativeViewer::showWarnings(bool show)
{
    loggerWindow->setVisible(show);
}

void QDeclarativeViewer::warningsWidgetOpened()
{
    showWarningsWindow->setChecked(true);
}

void QDeclarativeViewer::warningsWidgetClosed()
{
    showWarningsWindow->setChecked(false);
}

void QDeclarativeViewer::takeSnapShot()
{
    static int snapshotcount = 1;
    QString snapFileName = QString(QLatin1String("snapshot%1.png")).arg(snapshotcount);
    QPixmap::grabWidget(canvas).save(snapFileName);
    qDebug() << "Wrote" << snapFileName;
    ++snapshotcount;
}

void QDeclarativeViewer::pickRecordingFile()
{
    QString fileName = getVideoFileName();
    if (!fileName.isEmpty())
        recdlg->file->setText(fileName);
}

void QDeclarativeViewer::chooseRecordingOptions()
{
    // File
    recdlg->file->setText(record_file);

    // Size
    recdlg->setOriginalSize(canvas->size());

    // Rate
    recdlg->setVideoRate(record_rate);


    // Profile
    recdlg->setArguments(record_args.join(" "));
    if (recdlg->exec()) {
        // File
        record_file = recdlg->file->text();
        // Size
        record_outsize = recdlg->videoSize();
        // Rate
        record_rate = recdlg->videoRate();
        // Profile
        record_args = recdlg->arguments().split(" ",QString::SkipEmptyParts);
    }
}

void QDeclarativeViewer::toggleRecordingWithSelection()
{
    if (!recordTimer.isActive()) {
        if (record_file.isEmpty()) {
            QString fileName = getVideoFileName();
            if (fileName.isEmpty())
                return;
            if (!fileName.contains(QRegExp(".[^\\/]*$")))
                fileName += ".avi";
            setRecordFile(fileName);
        }
    }
    toggleRecording();
}

void QDeclarativeViewer::toggleRecording()
{
#ifndef NO_PRIVATE_HEADERS
    if (record_file.isEmpty()) {
        toggleRecordingWithSelection();
        return;
    }
    bool recording = !recordTimer.isActive();
    recordAction->setText(recording ? tr("&Stop Recording Video\tF9") : tr("&Start Recording Video\tF9"));
    setRecording(recording);
#endif
}

void QDeclarativeViewer::setAnimationsPaused(bool enable)
{
    if (enable) {
        setAnimationSpeed(0.0);
    } else {
        setAnimationSpeed(animationSpeed);
    }
}

void QDeclarativeViewer::pauseAnimations() {
    pauseAnimationsAction->setChecked(true);
    setAnimationsPaused(true);
}

void QDeclarativeViewer::stepAnimations()
{
    setAnimationSpeed(1.0);
    QTimer::singleShot(m_stepSize, this, SLOT(pauseAnimations()));
}

void QDeclarativeViewer::setAnimationStep()
{
    bool ok;
    int stepSize = QInputDialog::getInt(this, tr("Set animation step duration"),
                                              tr("Step duration (ms):"), m_stepSize, 20, 10000, 1, &ok);
    if (ok) m_stepSize = stepSize;
}

void QDeclarativeViewer::changeAnimationSpeed()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (action) {
        float f = action->data().toFloat();
        animationSpeed = f;
        if (!pauseAnimationsAction->isChecked())
            setAnimationSpeed(animationSpeed);
    }
}

void QDeclarativeViewer::addLibraryPath(const QString& lib)
{
    canvas->engine()->addImportPath(lib);
}

void QDeclarativeViewer::addPluginPath(const QString& plugin)
{
    canvas->engine()->addPluginPath(plugin);
}

void QDeclarativeViewer::reload()
{
    observer->setDesignModeBehavior(false);
    open(currentFileOrUrl);
}

void QDeclarativeViewer::openFile()
{
    QString cur = canvas->source().toLocalFile();
    if (useQmlFileBrowser) {
        open("qrc:/content/Browser.qml");
    } else {
        QString fileName = QFileDialog::getOpenFileName(this, tr("Open QML file"), cur, tr("QML Files (*.qml)"));
        if (!fileName.isEmpty()) {
            QFileInfo fi(fileName);
            open(fi.absoluteFilePath());
        }
    }
}

void QDeclarativeViewer::statusChanged()
{
    if (canvas->status() == QDeclarativeView::Error && tester)
        tester->executefailure();

    if (canvas->status() == QDeclarativeView::Ready) {
        initialSize = canvas->initialSize();
        updateSizeHints(true);
    }
}

void QDeclarativeViewer::launch(const QString& file_or_url)
{
    QMetaObject::invokeMethod(this, "open", Qt::QueuedConnection, Q_ARG(QString, file_or_url));
}

void QDeclarativeViewer::loadTranslationFile(const QString& directory)
{
    if (!translator) {
        translator = new QTranslator(this);
        QApplication::installTranslator(translator);
    }

    translator->load(QLatin1String("qml_" )+QLocale::system().name(), directory + QLatin1String("/i18n"));
}

void QDeclarativeViewer::loadDummyDataFiles(const QString& directory)
{
    QDir dir(directory+"/dummydata", "*.qml");
    QStringList list = dir.entryList();
    for (int i = 0; i < list.size(); ++i) {
        QString qml = list.at(i);
        QFile f(dir.filePath(qml));
        f.open(QIODevice::ReadOnly);
        QByteArray data = f.readAll();
        QDeclarativeComponent comp(canvas->engine());
        comp.setData(data, QUrl());
        QObject *dummyData = comp.create();

        if(comp.isError()) {
            QList<QDeclarativeError> errors = comp.errors();
            foreach (const QDeclarativeError &error, errors) {
                qWarning() << error;
            }
            if (tester) tester->executefailure();
        }

        if (dummyData) {
            qWarning() << "Loaded dummy data:" << dir.filePath(qml);
            qml.truncate(qml.length()-4);
            canvas->rootContext()->setContextProperty(qml, dummyData);
            dummyData->setParent(this);
        }
    }
}

bool QDeclarativeViewer::open(const QString& file_or_url)
{
    currentFileOrUrl = file_or_url;

    QUrl url;
    QFileInfo fi(file_or_url);
    if (fi.exists())
        url = QUrl::fromLocalFile(fi.absoluteFilePath());
    else
        url = QUrl(file_or_url);
    setWindowTitle(tr("%1 - Qt QML Viewer").arg(file_or_url));

#ifndef NO_PRIVATE_HEADERS
    if (!m_script.isEmpty())
        tester = new QDeclarativeTester(m_script, m_scriptOptions, canvas);
#endif

    delete canvas->rootObject();
    canvas->engine()->clearComponentCache();
    QDeclarativeContext *ctxt = canvas->rootContext();
    ctxt->setContextProperty("qmlViewer", this);
#ifdef Q_OS_SYMBIAN
    ctxt->setContextProperty("qmlViewerFolder", "E:\\"); // Documents on your S60 phone
#else
    ctxt->setContextProperty("qmlViewerFolder", QDir::currentPath());
#endif

    ctxt->setContextProperty("runtime", Runtime::instance());

    QString fileName = url.toLocalFile();
    if (!fileName.isEmpty()) {
        fi.setFile(fileName);
        if (fi.exists()) {
            if (fi.suffix().toLower() != QLatin1String("qml")) {
                qWarning() << "qml cannot open non-QML file" << fileName;
                return false;
            }

            QFileInfo fi(fileName);
            loadTranslationFile(fi.path());
            loadDummyDataFiles(fi.path());
        } else {
            qWarning() << "qml cannot find file:" << fileName;
            return false;
        }
    }

    QTime t;
    t.start();

    canvas->setSource(url);

    return true;
}

void QDeclarativeViewer::setAutoRecord(int from, int to)
{
    if (from==0) from=1; // ensure resized
    record_autotime = to-from;
    autoStartTimer.setInterval(from);
    autoStartTimer.start();
}

void QDeclarativeViewer::setRecordArgs(const QStringList& a)
{
    record_args = a;
}

void QDeclarativeViewer::setRecordFile(const QString& f)
{
    record_file = f;
}

void QDeclarativeViewer::setRecordRate(int fps)
{
    record_rate = fps;
}

void QDeclarativeViewer::sceneResized(QSize)
{
    updateSizeHints();
}

void QDeclarativeViewer::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_0 && devicemode)
        exit(0);
    else if (event->key() == Qt::Key_F1 || (event->key() == Qt::Key_1 && devicemode)) {
        qDebug() << "F1 - help\n"
                 << "F2 - save test script\n"
                 << "F3 - take PNG snapshot\n"
                 << "F4 - show items and state\n"
                 << "F5 - reload QML\n"
                 << "F6 - show object tree\n"
                 << "F7 - show timing\n"
                 << "F9 - toggle video recording\n"
                 << "F10 - toggle orientation\n"
                 << "device keys: 0=quit, 1..8=F1..F8"
                ;
    } else if (event->key() == Qt::Key_F2 || (event->key() == Qt::Key_2 && devicemode)) {
        if (tester && m_scriptOptions & Record)
            tester->save();
    } else if (event->key() == Qt::Key_F3 || (event->key() == Qt::Key_3 && devicemode)) {
        takeSnapShot();
    } else if (event->key() == Qt::Key_F5 || (event->key() == Qt::Key_5 && devicemode)) {
        reload();
    } else if (event->key() == Qt::Key_F9 || (event->key() == Qt::Key_9 && devicemode)) {
        toggleRecording();
    } else if (event->key() == Qt::Key_F10) {
        rotateOrientation();
    }

    QWidget::keyPressEvent(event);
}

bool QDeclarativeViewer::event(QEvent *event)
{
    if (event->type() == QEvent::WindowActivate) {
        Runtime::instance()->setActiveWindow(true);
    } else if (event->type() == QEvent::WindowDeactivate) {
        Runtime::instance()->setActiveWindow(false);
    }
    return QWidget::event(event);
}

void QDeclarativeViewer::senseImageMagick()
{
    QProcess proc;
    proc.start("convert", QStringList() << "-h");
    proc.waitForFinished(2000);
    QString help = proc.readAllStandardOutput();
    convertAvailable = help.contains("ImageMagick");
}

void QDeclarativeViewer::senseFfmpeg()
{
    QProcess proc;
    proc.start("ffmpeg", QStringList() << "-h");
    proc.waitForFinished(2000);
    QString ffmpegHelp = proc.readAllStandardOutput();
    ffmpegAvailable = ffmpegHelp.contains("-s ");
    ffmpegHelp = tr("Video recording uses ffmpeg:")+"\n\n"+ffmpegHelp;

    QDialog *d = new QDialog(recdlg);
    QVBoxLayout *l = new QVBoxLayout(d);
    QTextBrowser *b = new QTextBrowser(d);
    QFont f = b->font();
    f.setFamily("courier");
    b->setFont(f);
    b->setText(ffmpegHelp);
    l->addWidget(b);
    d->setLayout(l);
    ffmpegHelpWindow = d;
    connect(recdlg->ffmpegHelp,SIGNAL(clicked()), ffmpegHelpWindow, SLOT(show()));
}

void QDeclarativeViewer::setRecording(bool on)
{
    if (on == recordTimer.isActive())
        return;

    int period = int(1000/record_rate+0.5);
#ifndef NO_PRIVATE_HEADERS
    QUnifiedTimer::instance()->setTimingInterval(on ? period:16);
    QUnifiedTimer::instance()->setConsistentTiming(on);
#endif
    if (on) {
        canvas->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
        recordTimer.setInterval(period);
        recordTimer.start();
        frame_fmt = record_file.right(4).toLower();
        frame = QImage(canvas->width(),canvas->height(),QImage::Format_RGB32);
        if (frame_fmt != ".png" && (!convertAvailable || frame_fmt != ".gif")) {
            // Stream video to ffmpeg

            QProcess *proc = new QProcess(this);
            connect(proc, SIGNAL(finished(int)), this, SLOT(ffmpegFinished(int)));
            frame_stream = proc;

            QStringList args;
            args << "-y";
            args << "-r" << QString::number(record_rate);
            args << "-f" << "rawvideo";
            args << "-pix_fmt" << (frame_fmt == ".gif" ? "rgb24" : "rgb32");
            args << "-s" << QString("%1x%2").arg(canvas->width()).arg(canvas->height());
            args << "-i" << "-";
            if (record_outsize.isValid()) {
                args << "-s" << QString("%1x%2").arg(record_outsize.width()).arg(record_outsize.height());
                args << "-aspect" << QString::number(double(canvas->width())/canvas->height());
            }
            args += record_args;
            args << record_file;
            proc->start("ffmpeg",args);

        } else {
            // Store frames, save to GIF/PNG
            frame_stream = 0;
        }
    } else {
        canvas->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
        recordTimer.stop();
        if (frame_stream) {
            qDebug() << "Saving video...";
            frame_stream->close();
            qDebug() << "Wrote" << record_file;
        } else {
            QProgressDialog progress(tr("Saving frames..."), tr("Cancel"), 0, frames.count()+10, this);
            progress.setWindowModality(Qt::WindowModal);

            int frame=0;
            QStringList inputs;
            qDebug() << "Saving frames...";

            QString framename;
            bool png_output = false;
            if (record_file.right(4).toLower()==".png") {
                if (record_file.contains('%'))
                    framename = record_file;
                else
                    framename = record_file.left(record_file.length()-4)+"%04d"+record_file.right(4);
                png_output = true;
            } else {
                framename = "tmp-frame%04d.png";
                png_output = false;
            }
            foreach (QImage* img, frames) {
                progress.setValue(progress.value()+1);
                if (progress.wasCanceled())
                    break;
                QString name;
                name.sprintf(framename.toLocal8Bit(),frame++);
                if (record_outsize.isValid())
                    *img = img->scaled(record_outsize,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
                if (record_dither=="ordered")
                    img->convertToFormat(QImage::Format_Indexed8,Qt::PreferDither|Qt::OrderedDither).save(name);
                else if (record_dither=="threshold")
                    img->convertToFormat(QImage::Format_Indexed8,Qt::PreferDither|Qt::ThresholdDither).save(name);
                else if (record_dither=="floyd")
                    img->convertToFormat(QImage::Format_Indexed8,Qt::PreferDither).save(name);
                else
                    img->save(name);
                inputs << name;
                delete img;
            }

            if (!progress.wasCanceled()) {
                if (png_output) {
                    framename.replace(QRegExp("%\\d*."),"*");
                    qDebug() << "Wrote frames" << framename;
                    inputs.clear(); // don't remove them
                } else {
                    // ImageMagick and gifsicle for GIF encoding
                    progress.setLabelText(tr("Converting frames to GIF file..."));
                    QStringList args;
                    args << "-delay" << QString::number(period/10);
                    args << inputs;
                    args << record_file;
                    qDebug() << "Converting..." << record_file << "(this may take a while)";
                    if (0!=QProcess::execute("convert", args)) {
                        qWarning() << "Cannot run ImageMagick 'convert' - recorded frames not converted";
                        inputs.clear(); // don't remove them
                        qDebug() << "Wrote frames tmp-frame*.png";
                    } else {
                        if (record_file.right(4).toLower() == ".gif") {
                            qDebug() << "Compressing..." << record_file;
                            if (0!=QProcess::execute("gifsicle", QStringList() << "-O2" << "-o" << record_file << record_file))
                                qWarning() << "Cannot run 'gifsicle' - not compressed";
                        }
                        qDebug() << "Wrote" << record_file;
                    }
                }
            }

            progress.setValue(progress.maximum()-1);
            foreach (QString name, inputs)
                QFile::remove(name);

            frames.clear();
        }
    }
    qDebug() << "Recording: " << (recordTimer.isActive()?"ON":"OFF");
}

void QDeclarativeViewer::ffmpegFinished(int code)
{
    qDebug() << "ffmpeg returned" << code << frame_stream->readAllStandardError();
}

void QDeclarativeViewer::appAboutToQuit()
{
    // avoid QGLContext errors about invalid contexts on exit
    canvas->setViewport(0);

    // avoid crashes if messages are received after app has closed
    delete loggerWindow;
    loggerWindow = 0;
}

void QDeclarativeViewer::autoStartRecording()
{
    setRecording(true);
    autoStopTimer.setInterval(record_autotime);
    autoStopTimer.start();
}

void QDeclarativeViewer::autoStopRecording()
{
    setRecording(false);
}

void QDeclarativeViewer::recordFrame()
{
    canvas->QWidget::render(&frame);
    if (frame_stream) {
        if (frame_fmt == ".gif") {
            // ffmpeg can't do 32bpp with gif
            QImage rgb24 = frame.convertToFormat(QImage::Format_RGB888);
            frame_stream->write((char*)rgb24.bits(),rgb24.numBytes());
        } else {
            frame_stream->write((char*)frame.bits(),frame.numBytes());
        }
    } else {
        frames.append(new QImage(frame));
    }
}

void QDeclarativeViewer::changeOrientation(QAction *action)
{
    if (!action)
        return;
    action->setChecked(true);

    QString o = action->text();
    if (o == QLatin1String("Portrait"))
        DeviceOrientation::instance()->setOrientation(DeviceOrientation::Portrait);
    else if (o == QLatin1String("Landscape"))
        DeviceOrientation::instance()->setOrientation(DeviceOrientation::Landscape);
    else if (o == QLatin1String("Portrait (inverted)"))
        DeviceOrientation::instance()->setOrientation(DeviceOrientation::PortraitInverted);
    else if (o == QLatin1String("Landscape (inverted)"))
        DeviceOrientation::instance()->setOrientation(DeviceOrientation::LandscapeInverted);
}

void QDeclarativeViewer::orientationChanged()
{
    updateSizeHints();
}

void QDeclarativeViewer::setDeviceKeys(bool on)
{
    devicemode = on;
}

void QDeclarativeViewer::setNetworkCacheSize(int size)
{
    namFactory->setCacheSize(size);
}

void QDeclarativeViewer::setUseGL(bool useGL)
{
#ifdef GL_SUPPORTED
    if (useGL) {
        QGLFormat format = QGLFormat::defaultFormat();
#ifdef Q_WS_MAC
        format.setSampleBuffers(true);
#else
        format.setSampleBuffers(false);
#endif

        QGLWidget *glWidget = new QGLWidget(format);
        //### potentially faster, but causes junk to appear if top-level is Item, not Rectangle
        //glWidget->setAutoFillBackground(false);

        canvas->setViewport(glWidget);
    }
#else
    Q_UNUSED(useGL)
#endif
}

void QDeclarativeViewer::setUseNativeFileBrowser(bool use)
{
    useQmlFileBrowser = !use;
}

void QDeclarativeViewer::setSizeToView(bool sizeToView)
{
    QDeclarativeView::ResizeMode resizeMode = sizeToView ? QDeclarativeView::SizeRootObjectToView : QDeclarativeView::SizeViewToRootObject;
    if (resizeMode != canvas->resizeMode()) {
        canvas->setResizeMode(resizeMode);
        updateSizeHints();
    }
}

void QDeclarativeViewer::setAnimationSpeed(float f)
{
    QDeclarativeDebugHelper::setAnimationSlowDownFactor(f);
}

void QDeclarativeViewer::updateSizeHints(bool initial)
{
    static bool isRecursive = false;

    if (isRecursive)
        return;
    isRecursive = true;

    if (initial || (canvas->resizeMode() == QDeclarativeView::SizeViewToRootObject)) {
        QSize newWindowSize = initial ? initialSize : canvas->sizeHint();
        //qWarning() << "USH:" << (initial ? "INIT:" : "V2R:") << "setting fixed size " << newWindowSize;
        if (!isFullScreen() && !isMaximized()) {
            m_centralWidget->setFixedSize(newWindowSize.width(), newWindowSize.height() + 32);
            canvas->setFixedSize(newWindowSize);
            resize(1, 1);
            layout()->setSizeConstraint(QLayout::SetFixedSize);
            layout()->activate();
        }
    }
    //qWarning() << "USH: R2V: setting free size ";
    layout()->setSizeConstraint(QLayout::SetNoConstraint);
    layout()->activate();

    setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
    canvas->setMinimumSize(QSize(0,0));
    canvas->setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));

    m_centralWidget->setMinimumSize(QSize(0,0));
    m_centralWidget->setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));

    isRecursive = false;
}


void QDeclarativeViewer::registerTypes()
{
    static bool registered = false;

    if (!registered) {
        // registering only for exposing the DeviceOrientation::Orientation enum
        qmlRegisterUncreatableType<DeviceOrientation>("Qt",4,7,"Orientation","");
        registered = true;
    }
}

QT_END_NAMESPACE

#include "qmlruntime.moc"