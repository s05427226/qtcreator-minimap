/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
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
**************************************************************************/

#include "qmlengine.h"
#include "qmladapter.h"

#include "debuggerstartparameters.h"
#include "debuggeractions.h"
#include "debuggerconstants.h"
#include "debuggercore.h"
#include "debuggerdialogs.h"
#include "debuggermainwindow.h"
#include "debuggerrunner.h"
#include "debuggerstringutils.h"
#include "debuggertooltipmanager.h"

#include "breakhandler.h"
#include "moduleshandler.h"
#include "registerhandler.h"
#include "stackhandler.h"
#include "watchhandler.h"
#include "watchutils.h"

#include <extensionsystem/pluginmanager.h>
#include <projectexplorer/applicationlauncher.h>

#include <utils/environment.h>
#include <utils/abstractprocess.h>
#include <utils/qtcassert.h>
#include <utils/fileinprojectfinder.h>

#include <coreplugin/icore.h>
#include <coreplugin/helpmanager.h>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QMainWindow>
#include <QtGui/QMessageBox>
#include <QtGui/QToolTip>
#include <QtGui/QTextDocument>

#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>

#define DEBUG_QML 1
#if DEBUG_QML
#   define SDEBUG(s) qDebug() << s
#else
#   define SDEBUG(s)
#endif
# define XSDEBUG(s) qDebug() << s

using namespace ProjectExplorer;

namespace Debugger {
namespace Internal {

struct JSAgentBreakpointData
{
    QByteArray functionName;
    QByteArray fileUrl;
    qint32 lineNumber;
};

struct JSAgentStackData
{
    QByteArray functionName;
    QByteArray fileUrl;
    qint32 lineNumber;
};

uint qHash(const JSAgentBreakpointData &b)
{
    return b.lineNumber ^ qHash(b.fileUrl);
}

QDataStream &operator<<(QDataStream &s, const JSAgentBreakpointData &data)
{
    return s << data.functionName << data.fileUrl << data.lineNumber;
}

QDataStream &operator<<(QDataStream &s, const JSAgentStackData &data)
{
    return s << data.functionName << data.fileUrl << data.lineNumber;
}

QDataStream &operator>>(QDataStream &s, JSAgentBreakpointData &data)
{
    return s >> data.functionName >> data.fileUrl >> data.lineNumber;
}

QDataStream &operator>>(QDataStream &s, JSAgentStackData &data)
{
    return s >> data.functionName >> data.fileUrl >> data.lineNumber;
}

bool operator==(const JSAgentBreakpointData &b1, const JSAgentBreakpointData &b2)
{
    return b1.lineNumber == b2.lineNumber && b1.fileUrl == b2.fileUrl;
}

typedef QSet<JSAgentBreakpointData> JSAgentBreakpoints;
typedef QList<JSAgentStackData> JSAgentStackFrames;


static QDataStream &operator>>(QDataStream &s, WatchData &data)
{
    data = WatchData();
    QByteArray name;
    QByteArray value;
    QByteArray type;
    bool hasChildren = false;
    s >> data.exp >> name >> value >> type >> hasChildren >> data.id;
    data.name = QString::fromUtf8(name);
    data.setType(type, false);
    data.setValue(QString::fromUtf8(value));
    data.setHasChildren(hasChildren);
    data.setAllUnneeded();
    return s;
}

class QmlEnginePrivate
{
public:
    explicit QmlEnginePrivate(QmlEngine *q);

private:
    friend class QmlEngine;
    int m_ping;
    QmlAdapter m_adapter;
    ApplicationLauncher m_applicationLauncher;
    Utils::FileInProjectFinder fileFinder;
};

QmlEnginePrivate::QmlEnginePrivate(QmlEngine *q)
    : m_ping(0), m_adapter(q)
{}


///////////////////////////////////////////////////////////////////////
//
// QmlEngine
//
///////////////////////////////////////////////////////////////////////

QmlEngine::QmlEngine(const DebuggerStartParameters &startParameters,
        DebuggerEngine *masterEngine)
  : DebuggerEngine(startParameters, masterEngine),
    d(new QmlEnginePrivate(this))
{
    setObjectName(QLatin1String("QmlEngine"));
}

QmlEngine::~QmlEngine()
{}

void QmlEngine::setupInferior()
{
    QTC_ASSERT(state() == InferiorSetupRequested, qDebug() << state());

    if (startParameters().startMode == AttachToRemote) {
        emit requestRemoteSetup();
    } else {
        connect(&d->m_applicationLauncher,
            SIGNAL(processExited(int)),
            SLOT(disconnected()));
        connect(&d->m_applicationLauncher,
            SIGNAL(appendMessage(QString,ProjectExplorer::OutputFormat)),
            SLOT(appendMessage(QString,ProjectExplorer::OutputFormat)));
        connect(&d->m_applicationLauncher,
            SIGNAL(bringToForegroundRequested(qint64)),
            runControl(),
            SLOT(bringApplicationToForeground(qint64)));

        d->m_applicationLauncher.setEnvironment(startParameters().environment);
        d->m_applicationLauncher.setWorkingDirectory(startParameters().workingDirectory);

        notifyInferiorSetupOk();
    }
}

void QmlEngine::appendMessage(const QString &msg, OutputFormat /* format */)
{
    showMessage(msg, AppOutput); // FIXME: Redirect to RunControl
}

void QmlEngine::connectionEstablished()
{
    attemptBreakpointSynchronization();

    ExtensionSystem::PluginManager *pluginManager =
        ExtensionSystem::PluginManager::instance();
    pluginManager->addObject(&d->m_adapter);
    pluginManager->addObject(this);

    showMessage(tr("QML Debugger connected."), StatusBar);

    notifyEngineRunAndInferiorRunOk();

}

void QmlEngine::connectionStartupFailed()
{
    Core::ICore * const core = Core::ICore::instance();
    QMessageBox *infoBox = new QMessageBox(core->mainWindow());
    infoBox->setIcon(QMessageBox::Critical);
    infoBox->setWindowTitle(tr("Qt Creator"));
    infoBox->setText(tr("Could not connect to the in-process QML debugger.\n"
                        "Do you want to retry?"));
    infoBox->setStandardButtons(QMessageBox::Retry | QMessageBox::Cancel | QMessageBox::Help);
    infoBox->setDefaultButton(QMessageBox::Retry);
    infoBox->setModal(true);

    connect(infoBox, SIGNAL(finished(int)),
            this, SLOT(retryMessageBoxFinished(int)));

    infoBox->show();
}

void QmlEngine::retryMessageBoxFinished(int result)
{
    switch (result) {
    case QMessageBox::Retry: {
        d->m_adapter.beginConnection();
        break;
    }
    case QMessageBox::Help: {
        Core::HelpManager *helpManager = Core::HelpManager::instance();
        helpManager->handleHelpRequest("qthelp://com.nokia.qtcreator/doc/creator-debugging-qml.html");
        // fall through
    }
    default:
        notifyEngineRunFailed();
        break;
    }
}

void QmlEngine::connectionError(QAbstractSocket::SocketError socketError)
{
    if (socketError == QAbstractSocket::RemoteHostClosedError)
        showMessage(tr("QML Debugger: Remote host closed connection."), StatusBar);
}

void QmlEngine::serviceConnectionError(const QString &serviceName)
{
    showMessage(tr("QML Debugger: Could not connect to service '%1'.")
        .arg(serviceName), StatusBar);
}

bool QmlEngine::canDisplayTooltip() const
{
    return state() == InferiorRunOk || state() == InferiorStopOk;
}

void QmlEngine::filterApplicationMessage(const QString &msg, int /*channel*/)
{
    static const QString qddserver = QLatin1String("QDeclarativeDebugServer: ");
    static const QString cannotRetrieveDebuggingOutput = Utils::AbstractProcess::msgWinCannotRetrieveDebuggingOutput();

    const int index = msg.indexOf(qddserver);
    if (index != -1) {
        QString status = msg;
        status.remove(0, index + qddserver.length()); // chop of 'QDeclarativeDebugServer: '

        static QString waitingForConnection = QLatin1String("Waiting for connection on port");
        static QString unableToListen = QLatin1String("Unable to listen on port");
        static QString debuggingNotEnabled = QLatin1String("Ignoring \"-qmljsdebugger=port:");
        static QString connectionEstablished = QLatin1String("Connection established");

        QString errorMessage;
        if (status.startsWith(waitingForConnection)) {
            d->m_adapter.beginConnection();
        } else if (status.startsWith(unableToListen)) {
            //: Error message shown after 'Could not connect ... debugger:"
            errorMessage = tr("The port seems to be in use.");
        } else if (status.startsWith(debuggingNotEnabled)) {
            //: Error message shown after 'Could not connect ... debugger:"
            errorMessage = tr("The application is not set up for QML/JS debugging.");
        } else if (status.startsWith(connectionEstablished)) {
            // nothing to do
        } else {
            qWarning() << "Unknown QDeclarativeDebugServer status message: " << status;
        }

        if (!errorMessage.isEmpty()) {
            notifyEngineRunFailed();

            Core::ICore * const core = Core::ICore::instance();
            QMessageBox *infoBox = new QMessageBox(core->mainWindow());
            infoBox->setIcon(QMessageBox::Critical);
            infoBox->setWindowTitle(tr("Qt Creator"));
            //: %1 is detailed error message
            infoBox->setText(tr("Could not connect to the in-process QML debugger:\n%1")
                             .arg(errorMessage));
            infoBox->setStandardButtons(QMessageBox::Ok | QMessageBox::Help);
            infoBox->setDefaultButton(QMessageBox::Ok);
            infoBox->setModal(true);

            connect(infoBox, SIGNAL(finished(int)),
                    this, SLOT(wrongSetupMessageBoxFinished(int)));

            infoBox->show();
        }
    } else if (msg.contains(cannotRetrieveDebuggingOutput)) {
        // we won't get debugging output, so just try to connect ...
        d->m_adapter.beginConnection();
    }
}

void QmlEngine::showMessage(const QString &msg, int channel, int timeout) const
{
    if (channel == AppOutput || channel == AppError) {
        const_cast<QmlEngine*>(this)->filterApplicationMessage(msg, channel);
    }
    DebuggerEngine::showMessage(msg, channel, timeout);
}

void QmlEngine::closeConnection()
{
    disconnect(&d->m_adapter, SIGNAL(connectionStartupFailed()),
        this, SLOT(connectionStartupFailed()));
    d->m_adapter.closeConnection();

    ExtensionSystem::PluginManager *pluginManager =
        ExtensionSystem::PluginManager::instance();

    if (pluginManager->allObjects().contains(this)) {
        pluginManager->removeObject(&d->m_adapter);
        pluginManager->removeObject(this);
    }
}

void QmlEngine::runEngine()
{
    QTC_ASSERT(state() == EngineRunRequested, qDebug() << state());

    if (!isSlaveEngine())
        startApplicationLauncher();
}

void QmlEngine::startApplicationLauncher()
{
    if (!d->m_applicationLauncher.isRunning()) {
        appendMessage(tr("Starting %1 %2").arg(QDir::toNativeSeparators(startParameters().executable), startParameters().processArgs), NormalMessageFormat);
        d->m_applicationLauncher.start(ApplicationLauncher::Gui,
                                    startParameters().executable,
                                    startParameters().processArgs);
    }
}

void QmlEngine::stopApplicationLauncher()
{
    if (d->m_applicationLauncher.isRunning()) {
        disconnect(&d->m_applicationLauncher, SIGNAL(processExited(int)), this, SLOT(disconnected()));
        d->m_applicationLauncher.stop();
    }
}

void QmlEngine::handleRemoteSetupDone(int gdbServerPort, int qmlPort)
{
    Q_UNUSED(gdbServerPort);
    if (qmlPort != -1)
        startParameters().qmlServerPort = qmlPort;
    notifyInferiorSetupOk();
}

void QmlEngine::handleRemoteSetupFailed(const QString &message)
{
    QMessageBox::critical(0,tr("Failed to start application"),
        tr("Application startup failed: %1").arg(message));
    notifyInferiorSetupFailed();
}

void QmlEngine::shutdownInferior()
{
    if (isSlaveEngine()) {
        resetLocation();
    }
    stopApplicationLauncher();
    notifyInferiorShutdownOk();
}

void QmlEngine::shutdownEngine()
{
    closeConnection();

    // double check (ill engine?):
    stopApplicationLauncher();

    notifyEngineShutdownOk();
    if (!isSlaveEngine())
        showMessage(QString(), StatusBar);
}

void QmlEngine::setupEngine()
{
    d->m_ping = 0;
    connect(&d->m_adapter, SIGNAL(connectionError(QAbstractSocket::SocketError)),
        SLOT(connectionError(QAbstractSocket::SocketError)));
    connect(&d->m_adapter, SIGNAL(serviceConnectionError(QString)),
        SLOT(serviceConnectionError(QString)));
    connect(&d->m_adapter, SIGNAL(connected()),
        SLOT(connectionEstablished()));
    connect(&d->m_adapter, SIGNAL(connectionStartupFailed()),
        SLOT(connectionStartupFailed()));

    notifyEngineSetupOk();
}

void QmlEngine::continueInferior()
{
    QTC_ASSERT(state() == InferiorStopOk, qDebug() << state());
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "CONTINUE";
    rs << cmd;
    logMessage(LogSend, cmd);
    sendMessage(reply);
    resetLocation();
    notifyInferiorRunRequested();
    notifyInferiorRunOk();
}

void QmlEngine::interruptInferior()
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "INTERRUPT";
    rs << cmd;
    logMessage(LogSend, cmd);
    sendMessage(reply);
    notifyInferiorStopOk();
}

void QmlEngine::executeStep()
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "STEPINTO";
    rs << cmd;
    logMessage(LogSend, cmd);
    sendMessage(reply);
    notifyInferiorRunRequested();
    notifyInferiorRunOk();
}

void QmlEngine::executeStepI()
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "STEPINTO";
    rs << cmd;
    logMessage(LogSend, cmd);
    sendMessage(reply);
    notifyInferiorRunRequested();
    notifyInferiorRunOk();
}

void QmlEngine::executeStepOut()
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "STEPOUT";
    rs << cmd;
    logMessage(LogSend, cmd);
    sendMessage(reply);
    notifyInferiorRunRequested();
    notifyInferiorRunOk();
}

void QmlEngine::executeNext()
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "STEPOVER";
    rs << cmd;
    logMessage(LogSend, cmd);
    sendMessage(reply);
    notifyInferiorRunRequested();
    notifyInferiorRunOk();
}

void QmlEngine::executeNextI()
{
    SDEBUG("QmlEngine::executeNextI()");
}

void QmlEngine::executeRunToLine(const ContextData &data)
{
    Q_UNUSED(data)
    SDEBUG("FIXME:  QmlEngine::executeRunToLine()");
}

void QmlEngine::executeRunToFunction(const QString &functionName)
{
    Q_UNUSED(functionName)
    XSDEBUG("FIXME:  QmlEngine::executeRunToFunction()");
}

void QmlEngine::executeJumpToLine(const ContextData &data)
{
    Q_UNUSED(data)
    XSDEBUG("FIXME:  QmlEngine::executeJumpToLine()");
}

void QmlEngine::activateFrame(int index)
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "ACTIVATE_FRAME";
    rs << cmd
       << index;
    logMessage(LogSend, QString("%1 %2").arg(QString(cmd), QString::number(index)));
    sendMessage(reply);
    gotoLocation(stackHandler()->frames().value(index));
}

void QmlEngine::selectThread(int index)
{
    Q_UNUSED(index)
}

void QmlEngine::attemptBreakpointSynchronization()
{
    BreakHandler *handler = breakHandler();

    foreach (BreakpointId id, handler->unclaimedBreakpointIds()) {
        // Take ownership of the breakpoint. Requests insertion.
        if (acceptsBreakpoint(id))
            handler->setEngine(id, this);
    }

    JSAgentBreakpoints breakpoints;
    foreach (BreakpointId id, handler->engineBreakpointIds(this)) {
        if (handler->state(id) == BreakpointRemoveRequested) {
            handler->notifyBreakpointRemoveProceeding(id);
            handler->notifyBreakpointRemoveOk(id);
        } else {
            if (handler->state(id) == BreakpointInsertRequested) {
                handler->notifyBreakpointInsertProceeding(id);
            }
            JSAgentBreakpointData bp;
            bp.fileUrl = QUrl::fromLocalFile(handler->fileName(id)).toString().toUtf8();
            bp.lineNumber = handler->lineNumber(id);
            bp.functionName = handler->functionName(id).toUtf8();
            breakpoints.insert(bp);
            if (handler->state(id) == BreakpointInsertProceeding) {
                handler->notifyBreakpointInsertOk(id);
            }
        }
    }

    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "BREAKPOINTS";
    rs << cmd
       << breakpoints;

    QStringList breakPointsStr;
    foreach (const JSAgentBreakpointData &bp, breakpoints) {
        breakPointsStr << QString("('%1' '%2' %3)").arg(QString(bp.functionName),
                                  QString(bp.fileUrl), QString::number(bp.lineNumber));
    }
    logMessage(LogSend, QString("%1 [%2]").arg(QString(cmd), breakPointsStr.join(", ")));

    sendMessage(reply);
}

bool QmlEngine::acceptsBreakpoint(BreakpointId id) const
{
    return !DebuggerEngine::isCppBreakpoint(breakHandler()->breakpointData(id));
}

void QmlEngine::loadSymbols(const QString &moduleName)
{
    Q_UNUSED(moduleName)
}

void QmlEngine::loadAllSymbols()
{
}

void QmlEngine::reloadModules()
{
}

void QmlEngine::requestModuleSymbols(const QString &moduleName)
{
    Q_UNUSED(moduleName)
}

//////////////////////////////////////////////////////////////////////
//
// Tooltip specific stuff
//
//////////////////////////////////////////////////////////////////////

bool QmlEngine::setToolTipExpression(const QPoint &mousePos,
    TextEditor::ITextEditor *editor, const DebuggerToolTipContext &ctx)
{
    // This is processed by QML inspector, which has dependencies to 
    // the qml js editor. Makes life easier.
    emit tooltipRequested(mousePos, editor, ctx.position);
    return true;
}

//////////////////////////////////////////////////////////////////////
//
// Watch specific stuff
//
//////////////////////////////////////////////////////////////////////

void QmlEngine::assignValueInDebugger(const WatchData *,
    const QString &expression, const QVariant &valueV)
{
    QRegExp inObject("@([0-9a-fA-F]+)->(.+)");
    if (inObject.exactMatch(expression)) {
        bool ok = false;
        quint64 objectId = inObject.cap(1).toULongLong(&ok, 16);
        QString property = inObject.cap(2);
        if (ok && objectId > 0 && !property.isEmpty()) {
            QByteArray reply;
            QDataStream rs(&reply, QIODevice::WriteOnly);
            QByteArray cmd = "SET_PROPERTY";
            rs << cmd;
            rs << expression.toUtf8() << objectId << property << valueV.toString();
            logMessage(LogSend, QString("%1 %2 %3 %4 %5").arg(
                                 QString(cmd), QString::number(objectId), QString(property),
                                 valueV.toString()));
            sendMessage(reply);
        }
    }
}

void QmlEngine::updateWatchData(const WatchData &data,
    const WatchUpdateFlags &)
{
//    qDebug() << "UPDATE WATCH DATA" << data.toString();
    //watchHandler()->rebuildModel();
    showStatusMessage(tr("Stopped."), 5000);

    if (!data.name.isEmpty() && data.isValueNeeded()) {
        QByteArray reply;
        QDataStream rs(&reply, QIODevice::WriteOnly);
        QByteArray cmd = "EXEC";
        rs << cmd;
        rs << data.iname << data.name;
        logMessage(LogSend, QString("%1 %2 %3").arg(QString(cmd), QString(data.iname),
                                                          QString(data.name)));
        sendMessage(reply);
    }

    if (!data.name.isEmpty() && data.isChildrenNeeded()
            && watchHandler()->isExpandedIName(data.iname))
        expandObject(data.iname, data.id);

    {
        QByteArray reply;
        QDataStream rs(&reply, QIODevice::WriteOnly);
        QByteArray cmd = "WATCH_EXPRESSIONS";
        rs << cmd;
        rs << watchHandler()->watchedExpressions();
        logMessage(LogSend, QString("%1 %2").arg(
                             QString(cmd), watchHandler()->watchedExpressions().join(", ")));
        sendMessage(reply);
    }

    if (!data.isSomethingNeeded())
        watchHandler()->insertData(data);
}

void QmlEngine::expandObject(const QByteArray &iname, quint64 objectId)
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "EXPAND";
    rs << cmd;
    rs << iname << objectId;
    logMessage(LogSend, QString("%1 %2 %3").arg(QString(cmd), QString(iname),
                                                      QString::number(objectId)));
    sendMessage(reply);
}

void QmlEngine::sendPing()
{
    d->m_ping++;
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "PING";
    rs << cmd;
    rs << d->m_ping;
    logMessage(LogSend, QString("%1 %2").arg(QString(cmd), QString::number(d->m_ping)));
    sendMessage(reply);
}

unsigned QmlEngine::debuggerCapabilities() const
{
    return AddWatcherCapability;
    /*ReverseSteppingCapability | SnapshotCapability
        | AutoDerefPointersCapability | DisassemblerCapability
        | RegisterCapability | ShowMemoryCapability
        | JumpToLineCapability | ReloadModuleCapability
        | ReloadModuleSymbolsCapability | BreakOnThrowAndCatchCapability
        | ReturnFromFunctionCapability
        | CreateFullBacktraceCapability
        | WatchpointCapability
        | AddWatcherCapability;*/
}

QString QmlEngine::toFileInProject(const QString &fileUrl)
{
    if (fileUrl.isEmpty())
        return fileUrl;

    const QString path = QUrl(fileUrl).path();

    // Try to find shadow-build file in source dir first
    if (!QUrl(fileUrl).toLocalFile().isEmpty()
            && isShadowBuildProject()) {
        const QString sourcePath = fromShadowBuildFilename(path);
        if (QFileInfo(sourcePath).exists())
            return sourcePath;
    }

    // Try whether file is absolute & exists
    if (QFileInfo(path).isAbsolute()
            && QFileInfo(path).exists()) {
        return path;
    }

    if (d->fileFinder.projectDirectory().isEmpty())
        d->fileFinder.setProjectDirectory(startParameters().projectDir);

    // Try to find file with biggest common path in source directory
    bool fileFound = false;
    QString fileInProject = d->fileFinder.findFile(path, &fileFound);
    if (fileFound)
        return fileInProject;
    return fileUrl;
}

void QmlEngine::messageReceived(const QByteArray &message)
{
    QByteArray rwData = message;
    QDataStream stream(&rwData, QIODevice::ReadOnly);

    QByteArray command;
    stream >> command;

    if (command == "STOPPED") {
        //qDebug() << command << this << state();
        if (state() == InferiorRunOk)
            notifyInferiorSpontaneousStop();

        QString logString = QString::fromLatin1(command);

        JSAgentStackFrames stackFrames;
        QList<WatchData> watches;
        QList<WatchData> locals;
        stream >> stackFrames >> watches >> locals;

        logString += QString::fromLatin1(" (%1 stack frames) (%2 watches)  (%3 locals)").
                     arg(stackFrames.size()).arg(watches.size()).arg(locals.size());

        StackFrames ideStackFrames;
        for (int i = 0; i != stackFrames.size(); ++i) {
            StackFrame frame;
            frame.line = stackFrames.at(i).lineNumber;
            frame.function = stackFrames.at(i).functionName;
            frame.file = toFileInProject(stackFrames.at(i).fileUrl);
            frame.usable = QFileInfo(frame.file).isReadable();
            frame.level = i + 1;
            ideStackFrames << frame;
        }

        if (ideStackFrames.size() && ideStackFrames.back().function == "<global>")
            ideStackFrames.takeLast();
        stackHandler()->setFrames(ideStackFrames);

        watchHandler()->beginCycle();
        bool needPing = false;

        foreach (WatchData data, watches) {
            data.iname = watchHandler()->watcherName(data.exp);
            watchHandler()->insertData(data);

            if (watchHandler()->expandedINames().contains(data.iname)) {
                needPing = true;
                expandObject(data.iname, data.id);
            }
        }

        foreach (WatchData data, locals) {
            data.iname = "local." + data.exp;
            watchHandler()->insertData(data);

            if (watchHandler()->expandedINames().contains(data.iname)) {
                needPing = true;
                expandObject(data.iname, data.id);
            }
        }

        if (needPing) {
            sendPing();
        } else {
            watchHandler()->endCycle();
        }

        bool becauseOfException;
        stream >> becauseOfException;

        logString += becauseOfException ? " exception" : " no_exception";

        if (becauseOfException) {
            QString error;
            stream >> error;

            logString += QLatin1Char(' ');
            logString += error;
            logMessage(LogReceive, logString);

            QString msg = stackFrames.isEmpty()
                ? tr("<p>An uncaught exception occurred:</p><p>%1</p>")
                    .arg(Qt::escape(error))
                : tr("<p>An uncaught exception occurred in <i>%1</i>:</p><p>%2</p>")
                    .arg(stackFrames.value(0).fileUrl, Qt::escape(error));
            showMessageBox(QMessageBox::Information, tr("Uncaught Exception"), msg);
        } else {
            //
            // Make breakpoint non-pending
            //
            QString file;
            QString function;
            int line = -1;

            if (!ideStackFrames.isEmpty()) {
                file = ideStackFrames.at(0).file;
                line = ideStackFrames.at(0).line;
                function = ideStackFrames.at(0).function;
            }

            BreakHandler *handler = breakHandler();
            foreach (BreakpointId id, handler->engineBreakpointIds(this)) {
                QString processedFilename = handler->fileName(id);
                if (processedFilename == file && handler->lineNumber(id) == line) {
                    QTC_ASSERT(handler->state(id) == BreakpointInserted,/**/);
                    BreakpointResponse br = handler->response(id);
                    br.fileName = file;
                    br.lineNumber = line;
                    br.functionName = function;
                    handler->setResponse(id, br);
                }
            }

            logMessage(LogReceive, logString);
        }

        if (!ideStackFrames.isEmpty())
            gotoLocation(ideStackFrames.value(0));

    } else if (command == "RESULT") {
        WatchData data;
        QByteArray iname;
        stream >> iname >> data;

        logMessage(LogReceive, QString("%1 %2 %3").arg(QString(command),
                                                             QString(iname), QString(data.value)));
        data.iname = iname;
        if (iname.startsWith("watch.")) {
            watchHandler()->insertData(data);
        } else if(iname == "console") {
            showMessage(data.value, ScriptConsoleOutput);
        } else {
            qWarning() << "QmlEngine: Unexcpected result: " << iname << data.value;
        }
    } else if (command == "EXPANDED") {
        QList<WatchData> result;
        QByteArray iname;
        stream >> iname >> result;

        logMessage(LogReceive, QString("%1 %2 (%3 x watchdata)").arg(
                             QString(command), QString(iname), QString::number(result.size())));
        bool needPing = false;
        foreach (WatchData data, result) {
            data.iname = iname + '.' + data.exp;
            watchHandler()->insertData(data);

            if (watchHandler()->expandedINames().contains(data.iname)) {
                needPing = true;
                expandObject(data.iname, data.id);
            }
        }
        if (needPing)
            sendPing();
    } else if (command == "LOCALS") {
        QList<WatchData> locals;
        int frameId;
        stream >> frameId >> locals;

        logMessage(LogReceive, QString("%1 %2 (%3 x locals)").arg(
                             QString(command), QString::number(frameId),
                             QString::number(locals.size())));
        watchHandler()->beginCycle();
        bool needPing = false;
        foreach (WatchData data, locals) {
            data.iname = "local." + data.exp;
            watchHandler()->insertData(data);
            if (watchHandler()->expandedINames().contains(data.iname)) {
                needPing = true;
                expandObject(data.iname, data.id);
            }
        }
        if (needPing)
            sendPing();
        else
            watchHandler()->endCycle();

    } else if (command == "PONG") {
        int ping;
        stream >> ping;

        logMessage(LogReceive, QString("%1 %2").arg(QString(command), QString::number(ping)));

        if (ping == d->m_ping)
            watchHandler()->endCycle();
    } else {
        qDebug() << Q_FUNC_INFO << "Unknown command: " << command;
        logMessage(LogReceive, QString("%1 UNKNOWN COMMAND!!").arg(QString(command)));
    }
}

void QmlEngine::disconnected()
{
    showMessage(tr("QML Debugger disconnected."), StatusBar);
    notifyInferiorExited();
}

void QmlEngine::wrongSetupMessageBoxFinished(int result)
{
    if (result == QMessageBox::Help) {
        Core::HelpManager *helpManager = Core::HelpManager::instance();
        helpManager->handleHelpRequest(
                    QLatin1String("qthelp://com.nokia.qtcreator/doc/creator-debugging-qml.html"));
    }
}

void QmlEngine::executeDebuggerCommand(const QString& command)
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "EXEC";
    QByteArray console = "console";
    rs << cmd << console << command;
    logMessage(LogSend, QString("%1 %2 %3").arg(QString(cmd), QString(console),
                                                      QString(command)));
    sendMessage(reply);
}

bool QmlEngine::isShadowBuildProject() const
{
    return !startParameters().projectBuildDir.isEmpty()
        && startParameters().projectDir != startParameters().projectBuildDir;
}

QString QmlEngine::qmlImportPath() const
{
    return startParameters().environment.value("QML_IMPORT_PATH");
}

QString QmlEngine::mangleFilenamePaths(const QString &filename,
    const QString &oldBasePath, const QString &newBasePath) const
{
    QDir oldBaseDir(oldBasePath);
    QDir newBaseDir(newBasePath);
    QFileInfo fileInfo(filename);

    if (oldBaseDir.exists() && newBaseDir.exists() && fileInfo.exists()) {
        if (fileInfo.absoluteFilePath().startsWith(oldBaseDir.canonicalPath())) {
            QString fileRelativePath = fileInfo.canonicalFilePath().mid(oldBaseDir.canonicalPath().length());
            QFileInfo projectFile(newBaseDir.canonicalPath() + QLatin1Char('/') + fileRelativePath);

            if (projectFile.exists())
                return projectFile.canonicalFilePath();
        }
    }
    return filename;
}

QString QmlEngine::fromShadowBuildFilename(const QString &filename) const
{
    QString newFilename = filename;
    QString importPath = qmlImportPath();

#ifdef Q_OS_MACX
    // Qt Quick Applications by default copy the qml directory
    // to buildDir()/X.app/Contents/Resources.
    const QString applicationBundleDir
                = QFileInfo(startParameters().executable).absolutePath() + "/../..";
    newFilename = mangleFilenamePaths(newFilename, applicationBundleDir + "/Contents/Resources", startParameters().projectDir);
#endif
    newFilename = mangleFilenamePaths(newFilename, startParameters().projectBuildDir, startParameters().projectDir);

    if (newFilename == filename && !importPath.isEmpty()) {
        newFilename = mangleFilenamePaths(filename, startParameters().projectBuildDir, importPath);
    }

    return newFilename;
}

void QmlEngine::logMessage(LogDirection direction, const QString &message)
{
    QString msg = "JSDebugger";
    if (direction == LogSend) {
        msg += " sending ";
    } else {
        msg += " receiving ";
    }
    msg += message;
    showMessage(msg, LogDebug);
}

QmlEngine *createQmlEngine(const DebuggerStartParameters &sp,
    DebuggerEngine *masterEngine)
{
    return new QmlEngine(sp, masterEngine);
}

} // namespace Internal
} // namespace Debugger

