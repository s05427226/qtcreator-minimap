/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "debuggerrunner.h"
#include "debuggerruncontrolfactory.h"

#include "debuggeractions.h"
#include "debuggercore.h"
#include "debuggerengine.h"
#include "debuggermainwindow.h"
#include "debuggerplugin.h"
#include "debuggerstringutils.h"
#include "debuggerstartparameters.h"
#include "lldb/lldbenginehost.h"
#include "debuggertooltipmanager.h"
#include "qml/qmlengine.h"

#ifdef Q_OS_WIN
#  include "peutils.h"
#endif

#include <projectexplorer/abi.h>
#include <projectexplorer/debugginghelper.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/target.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/outputformat.h>
#include <projectexplorer/toolchain.h>
#include <projectexplorer/applicationrunconfiguration.h> // For LocalApplication*

#include <utils/synchronousprocess.h>
#include <utils/qtcassert.h>
#include <utils/fancymainwindow.h>
#include <utils/qtcprocess.h>
#include <coreplugin/icore.h>

#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtGui/QMessageBox>

using namespace ProjectExplorer;
using namespace Debugger::Internal;

enum { debug = 0 };

namespace Debugger {
namespace Internal {

bool isCdbEngineEnabled(); // Check the configuration page
bool checkCdbConfiguration(const DebuggerStartParameters &sp, ConfigurationCheck *check);
DebuggerEngine *createCdbEngine(const DebuggerStartParameters &sp,
    DebuggerEngine *masterEngine, QString *error);

bool checkGdbConfiguration(const DebuggerStartParameters &sp, ConfigurationCheck *check);
DebuggerEngine *createGdbEngine(const DebuggerStartParameters &sp,
    DebuggerEngine *masterEngine);

DebuggerEngine *createScriptEngine(const DebuggerStartParameters &sp);
DebuggerEngine *createPdbEngine(const DebuggerStartParameters &sp);
DebuggerEngine *createTcfEngine(const DebuggerStartParameters &sp);
QmlEngine *createQmlEngine(const DebuggerStartParameters &sp,
    DebuggerEngine *masterEngine);
DebuggerEngine *createQmlCppEngine(const DebuggerStartParameters &sp,
                                   DebuggerEngineType slaveEngineType,
                                   QString *errorMessage);
DebuggerEngine *createLldbEngine(const DebuggerStartParameters &sp);

extern QString msgNoBinaryForToolChain(const Abi &abi);

static const char *engineTypeName(DebuggerEngineType et)
{
    switch (et) {
    case Debugger::NoEngineType:
        break;
    case Debugger::GdbEngineType:
        return "Gdb";
    case Debugger::ScriptEngineType:
        return "Script engine";
    case Debugger::CdbEngineType:
        return "Cdb engine";
    case Debugger::PdbEngineType:
        return "Pdb engine";    case Debugger::TcfEngineType:
        return "Tcf engine";
    case Debugger::QmlEngineType:
        return "QML engine";
    case Debugger::QmlCppEngineType:
        return "QML C++ engine";
    case Debugger::LldbEngineType:
        return "LLDB engine";
    case Debugger::AllEngineTypes:
        break;
    }
    return "No engine";
}

static inline QString engineTypeNames(const QList<DebuggerEngineType> &l)
{
    QString rc;
    foreach (DebuggerEngineType et, l) {
        if (!rc.isEmpty())
            rc.append(QLatin1Char(','));
        rc += QLatin1String(engineTypeName(et));
    }
    return rc;
}

static QString msgEngineNotAvailable(const char *engine)
{
    return DebuggerPlugin::tr("The application requires the debugger engine '%1', "
        "which is disabled.").arg(_(engine));
}

static inline QString msgEngineNotAvailable(DebuggerEngineType et)
{
    return msgEngineNotAvailable(engineTypeName(et));
}

////////////////////////////////////////////////////////////////////////
//
// DebuggerRunControlPrivate
//
////////////////////////////////////////////////////////////////////////

class DebuggerRunControlPrivate
{
public:
    explicit DebuggerRunControlPrivate(DebuggerRunControl *parent,
                                       RunConfiguration *runConfiguration);

    DebuggerEngineType engineForExecutable(unsigned enabledEngineTypes,
        const QString &executable);
    DebuggerEngineType engineForMode(unsigned enabledEngineTypes,
        DebuggerStartMode mode);

public:
    DebuggerRunControl *q;
    DebuggerEngine *m_engine;
    const QWeakPointer<RunConfiguration> m_myRunConfiguration;
    bool m_running;
};

DebuggerRunControlPrivate::DebuggerRunControlPrivate(DebuggerRunControl *parent,
                                                     RunConfiguration *runConfiguration)
    : q(parent)
    , m_engine(0)
    , m_myRunConfiguration(runConfiguration)
    , m_running(false)
{
}

} // namespace Internal

DebuggerRunControl::DebuggerRunControl(RunConfiguration *runConfiguration,
                                       const DebuggerStartParameters &sp,
                                       const QPair<DebuggerEngineType, DebuggerEngineType> &masterSlaveEngineTypes)
    : RunControl(runConfiguration, Constants::DEBUGMODE),
      d(new DebuggerRunControlPrivate(this, runConfiguration))
{
    connect(this, SIGNAL(finished()), SLOT(handleFinished()));
    // Create the engine. Could arguably be moved to the factory, but
    // we still have a derived S60DebugControl. Should rarely fail, though.
    QString errorMessage;
    d->m_engine = masterSlaveEngineTypes.first == QmlCppEngineType ?
            createQmlCppEngine(sp, masterSlaveEngineTypes.second, &errorMessage) :
            DebuggerRunControlFactory::createEngine(masterSlaveEngineTypes.first, sp,
                                                    0, &errorMessage);
    if (d->m_engine) {
        DebuggerToolTipManager::instance()->registerEngine(d->m_engine);
    } else {
        debuggingFinished();
        Core::ICore::instance()->showWarningWithOptions(DebuggerRunControl::tr("Debugger"), errorMessage);
    }
}

DebuggerRunControl::~DebuggerRunControl()
{
    disconnect();
    if (DebuggerEngine *engine = d->m_engine) {
        d->m_engine = 0;
        engine->disconnect();
        delete engine;
    }
}

const DebuggerStartParameters &DebuggerRunControl::startParameters() const
{
    QTC_ASSERT(d->m_engine, return *(new DebuggerStartParameters()));
    return d->m_engine->startParameters();
}

QString DebuggerRunControl::displayName() const
{
    QTC_ASSERT(d->m_engine, return QString());
    return d->m_engine->startParameters().displayName;
}

void DebuggerRunControl::setCustomEnvironment(Utils::Environment env)
{
    QTC_ASSERT(d->m_engine, return);
    d->m_engine->startParameters().environment = env;
}

void DebuggerRunControl::start()
{
    QTC_ASSERT(d->m_engine, return);
    debuggerCore()->runControlStarted(d->m_engine);

    // We might get a synchronous startFailed() notification on Windows,
    // when launching the process fails. Emit a proper finished() sequence.
    emit started();
    d->m_running = true;

    d->m_engine->startDebugger(this);

    if (d->m_running)
        appendMessage(tr("Debugging starts"), NormalMessageFormat);
}

void DebuggerRunControl::startFailed()
{
    appendMessage(tr("Debugging has failed"), NormalMessageFormat);
    d->m_running = false;
    emit finished();
    d->m_engine->handleStartFailed();
}

void DebuggerRunControl::handleFinished()
{
    appendMessage(tr("Debugging has finished"), NormalMessageFormat);
    if (d->m_engine)
        d->m_engine->handleFinished();
    debuggerCore()->runControlFinished(d->m_engine);
}

void DebuggerRunControl::showMessage(const QString &msg, int channel)
{
    switch (channel) {
        case AppOutput:
            appendMessage(msg, StdOutFormatSameLine);
            break;
        case AppError:
            appendMessage(msg, StdErrFormatSameLine);
            break;
        case AppStuff:
            appendMessage(msg, NormalMessageFormat);
            break;
    }
}

bool DebuggerRunControl::promptToStop(bool *optionalPrompt) const
{
    QTC_ASSERT(isRunning(), return true;)

    if (optionalPrompt && !*optionalPrompt)
        return true;

    const QString question = tr("A debugging session is still in progress. "
            "Terminating the session in the current"
            " state can leave the target in an inconsistent state."
            " Would you still like to terminate it?");
    return showPromptToStopDialog(tr("Close Debugging Session"), question,
                                  QString(), QString(), optionalPrompt);
}

RunControl::StopResult DebuggerRunControl::stop()
{
    QTC_ASSERT(d->m_engine, return StoppedSynchronously);
    d->m_engine->quitDebugger();
    return AsynchronousStop;
}

void DebuggerRunControl::debuggingFinished()
{
    d->m_running = false;
    emit finished();
}

bool DebuggerRunControl::isRunning() const
{
    return d->m_running;
}

DebuggerEngine *DebuggerRunControl::engine()
{
    QTC_ASSERT(d->m_engine, /**/);
    return d->m_engine;
}

RunConfiguration *DebuggerRunControl::runConfiguration() const
{
    return d->m_myRunConfiguration.data();
}

////////////////////////////////////////////////////////////////////////
//
// Engine detection logic: Detection functions depending on toolchain, binary,
// etc. Return a list of possible engines (order of prefererence) without
// consideration of configuration, etc.
//
////////////////////////////////////////////////////////////////////////

static QList<DebuggerEngineType> enginesForToolChain(const Abi &toolChain)
{
    QList<DebuggerEngineType> result;
    switch (toolChain.binaryFormat()) {
    case Abi::ElfFormat:
    case Abi::MachOFormat:
        result.push_back(LldbEngineType);
        result.push_back(GdbEngineType);
        break;
   case Abi::PEFormat:
        if (toolChain.osFlavor() == Abi::WindowsMSysFlavor) {
            result.push_back(GdbEngineType);
            result.push_back(CdbEngineType);
        } else {
            result.push_back(CdbEngineType);
            result.push_back(GdbEngineType);
        }
        break;
    case Abi::RuntimeQmlFormat:
        result.push_back(QmlEngineType);
        break;
    default:
        break;
    }
    return result;
}

static inline QList<DebuggerEngineType> enginesForScriptExecutables(const QString &executable)
{
    QList<DebuggerEngineType> result;
    if (executable.endsWith(_(".js"))) {
        result.push_back(ScriptEngineType);
    } else if (executable.endsWith(_(".py"))) {
        result.push_back(PdbEngineType);
    }
    return result;
}

static QList<DebuggerEngineType> enginesForExecutable(const QString &executable)
{
    QList<DebuggerEngineType> result = enginesForScriptExecutables(executable);
    if (!result.isEmpty())
        return result;
#ifdef Q_OS_WIN
    // A remote executable?
    if (!executable.endsWith(_(".exe"), Qt::CaseInsensitive)) {
        result.push_back(GdbEngineType);
        return result;
    }

    // If a file has PDB files, it has been compiled by VS.
    QStringList pdbFiles;
    QString errorMessage;
    if (getPDBFiles(executable, &pdbFiles, &errorMessage) && !pdbFiles.isEmpty()) {
        result.push_back(CdbEngineType);
        result.push_back(GdbEngineType);
        return result;
    }
    // Fixme: Gdb should only be preferred if MinGW can positively be detected.
    result.push_back(GdbEngineType);
    result.push_back(CdbEngineType);
#else
    result.push_back(LldbEngineType);
    result.push_back(GdbEngineType);
#endif
    return result;
}

// Debugger type for mode.
static QList<DebuggerEngineType> enginesForMode(DebuggerStartMode startMode,
                                                bool hardConstraintsOnly)
{
    QList<DebuggerEngineType> result;
    switch (startMode) {
    case Debugger::NoStartMode:
        break;
    case Debugger::StartInternal:
    case Debugger::StartExternal:
    case AttachExternal:
        if (!hardConstraintsOnly) {
#ifdef Q_OS_WIN
            result.push_back(CdbEngineType); // Preferably Windows debugger for attaching locally.
#endif
            result.push_back(GdbEngineType);
        }
        break;
    case Debugger::AttachCore:
    case Debugger::StartRemoteGdb:
        result.push_back(GdbEngineType);
        break;
    case Debugger::AttachToRemote:
        if (!hardConstraintsOnly) {
#ifdef Q_OS_WIN
            result.push_back(CdbEngineType);
#endif
            result.push_back(GdbEngineType);
        }
        break;
    case AttachTcf:
        result.push_back(TcfEngineType);
        break;
    case AttachCrashedExternal:
        result.push_back(CdbEngineType); // Only CDB can do this
        break;
    case StartRemoteEngine:
        // FIXME: Unclear IPC override. Someone please have a better idea.
        // For now thats the only supported IPC engine.
        result.push_back(LldbEngineType);
        break;
    }
    return result;
}

// Engine detection logic: Call all detection functions in order.

static QList<DebuggerEngineType> engineTypes(const DebuggerStartParameters &sp)
{
    // Script executables and certain start modes are 'hard constraints'.
    QList<DebuggerEngineType> result = enginesForScriptExecutables(sp.executable);
    if (!result.isEmpty())
        return result;

    result = enginesForMode(sp.startMode, true);
    if (!result.isEmpty())
        return result;

    //  'hard constraints' done (with the exception of QML ABI checked here),
    // further try to restrict available engines.
    if (sp.toolChainAbi.isValid()) {
        result = enginesForToolChain(sp.toolChainAbi);
        if (!result.isEmpty())
            return result;
    }

    // FIXME: 1 of 3 testing hacks.
    if (sp.processArgs.startsWith(__("@tcf@ "))) {
        result.push_back(GdbEngineType);
        return result;
    }

    if (sp.startMode != AttachToRemote && !sp.executable.isEmpty())
        result = enginesForExecutable(sp.executable);
    if (!result.isEmpty())
        return result;

    result = enginesForMode(sp.startMode, false);
    return result;
}

// Engine detection logic: ConfigurationCheck.
ConfigurationCheck::ConfigurationCheck() :
    masterSlaveEngineTypes(NoEngineType, NoEngineType)
{
}

ConfigurationCheck::operator bool() const
{
    return errorMessage.isEmpty() &&  errorDetails.isEmpty() && masterSlaveEngineTypes.first != NoEngineType;
}

QString ConfigurationCheck::errorDetailsString() const
{
    return errorDetails.join(QLatin1String("\n\n"));
}

/*!
    \fn ConfigurationCheck checkDebugConfiguration(unsigned cmdLineEnabledEngines,
                                                   const DebuggerStartParameters &sp)

    This is the master engine detection function that returns the
    engine types for a given set of start parameters and checks their
    configuration.
*/

DEBUGGER_EXPORT ConfigurationCheck checkDebugConfiguration(const DebuggerStartParameters &sp)
{
    ConfigurationCheck result;
    const unsigned activeLangs = debuggerCore()->activeLanguages();
    const bool qmlLanguage = activeLangs & QmlLanguage;
    const bool cppLanguage = activeLangs & CppLanguage;
    if (debug)
        qDebug().nospace() << "checkDebugConfiguration " << sp.toolChainAbi.toString()
                           << " Start mode=" << sp.startMode << " Executable=" << sp.executable
                           << " Debugger command=" << sp.debuggerCommand;
    // Get all applicable types.
    QList<DebuggerEngineType> requiredTypes;
    if (qmlLanguage && !cppLanguage) {
        requiredTypes.push_back(QmlEngineType);
    } else {
        requiredTypes = engineTypes(sp);
    }
    if (requiredTypes.isEmpty()) {
        result.errorMessage = QLatin1String("Internal error: Unable to determine debugger engine type for this configuration");
        return result;
    }
    if (debug)
        qDebug() << " Required: " << engineTypeNames(requiredTypes);
    // Filter out disables types, command line + current settings.
    unsigned cmdLineEnabledEngines = debuggerCore()->enabledEngines();
#ifdef CDB_ENABLED
     if (!isCdbEngineEnabled() && !Cdb::isCdbEngineEnabled())
         cmdLineEnabledEngines &= ~CdbEngineType;
#endif
#ifdef WITH_LLDB
    if (!Core::ICore::instance()->settings()->value(QLatin1String("LLDB/enabled")).toBool())
        cmdLineEnabledEngines &= ~LldbEngineType;
#else
     cmdLineEnabledEngines &= ~LldbEngineType;
#endif
    QList<DebuggerEngineType> usableTypes;
    foreach (DebuggerEngineType et, requiredTypes)
        if (et & cmdLineEnabledEngines) {
            usableTypes.push_back(et);
        } else {
            const QString msg = DebuggerPlugin::tr("The debugger engine '%1' preferred for "
                                                   "debugging binaries of type %2 is disabled.").
                    arg(engineTypeName(et), sp.toolChainAbi.toString());
            debuggerCore()->showMessage(msg, LogWarning);
        }
    if (usableTypes.isEmpty()) {
        result.errorMessage = DebuggerPlugin::tr("This configuration requires the debugger engine %1, which is disabled.").
                arg(QLatin1String(engineTypeName(usableTypes.front())));
        return result;
    }
    if (debug)
        qDebug() << " Usable engines: " << engineTypeNames(usableTypes);
    // Configuration check: Strip off non-configured engines, find first one to use.
    while (!usableTypes.isEmpty()) {
        bool configurationOk = true;
        switch (usableTypes.front()) {
        case Debugger::CdbEngineType:
            configurationOk = checkCdbConfiguration(sp, &result);
            break;
        case Debugger::GdbEngineType:
            configurationOk = checkGdbConfiguration(sp, &result);
            break;
        default:
            break;
        }
        if (configurationOk) {
            break;
        } else {
            const QString msg = DebuggerPlugin::tr("The debugger engine '%1' preferred "
                                                   "for debugging binaries of type %2 is not set up correctly: %3").
                                arg(engineTypeName(usableTypes.front()), sp.toolChainAbi.toString(),
                                    result.errorDetails.isEmpty() ? QString() : result.errorDetails.back());
            debuggerCore()->showMessage(msg, LogWarning);
            usableTypes.pop_front();
        }
    }
    if (debug)
        qDebug() << "Configured engines: " << engineTypeNames(usableTypes);
    if (usableTypes.isEmpty()) {
        result.errorMessage = DebuggerPlugin::tr("The debugger engine required for this configuration is not correctly configured.");
        return result;
    }
    // Anything left: Happy.
    result.errorMessage.clear();
    result.errorDetails.clear();
    if (qmlLanguage && cppLanguage) {
        result.masterSlaveEngineTypes.first = QmlCppEngineType;
        result.masterSlaveEngineTypes.second = usableTypes.front();
    } else {
        result.masterSlaveEngineTypes.first = usableTypes.front();
    }
    if (debug)
        qDebug() << engineTypeName(result.masterSlaveEngineTypes.first) << engineTypeName(result.masterSlaveEngineTypes.second);
    return result;
}

////////////////////////////////////////////////////////////////////////
//
// DebuggerRunControlFactory
//
////////////////////////////////////////////////////////////////////////

// A factory to create DebuggerRunControls
DebuggerRunControlFactory::DebuggerRunControlFactory(QObject *parent,
        unsigned enabledEngines)
    : IRunControlFactory(parent), m_enabledEngines(enabledEngines)
{}

bool DebuggerRunControlFactory::canRun(RunConfiguration *runConfiguration, const QString &mode) const
{
//    return mode == ProjectExplorer::Constants::DEBUGMODE;
    return mode == Constants::DEBUGMODE
            && qobject_cast<LocalApplicationRunConfiguration *>(runConfiguration);
}

QString DebuggerRunControlFactory::displayName() const
{
    return DebuggerRunControl::tr("Debug");
}

// Find Qt installation by running qmake
static inline QString findQtInstallPath(const QString &qmakePath)
{
    QProcess proc;
    QStringList args;
    args.append(_("-query"));
    args.append(_("QT_INSTALL_HEADERS"));
    proc.start(qmakePath, args);
    if (!proc.waitForStarted()) {
        qWarning("%s: Cannot start '%s': %s", Q_FUNC_INFO, qPrintable(qmakePath),
           qPrintable(proc.errorString()));
        return QString();
    }
    proc.closeWriteChannel();
    if (!proc.waitForFinished()) {
        Utils::SynchronousProcess::stopProcess(proc);
        qWarning("%s: Timeout running '%s'.", Q_FUNC_INFO, qPrintable(qmakePath));
        return QString();
    }
    if (proc.exitStatus() != QProcess::NormalExit) {
        qWarning("%s: '%s' crashed.", Q_FUNC_INFO, qPrintable(qmakePath));
        return QString();
    }
    const QByteArray ba = proc.readAllStandardOutput().trimmed();
    QDir dir(QString::fromLocal8Bit(ba));
    if (dir.exists() && dir.cdUp())
        return dir.absolutePath();
    return QString();
}

static DebuggerStartParameters localStartParameters(RunConfiguration *runConfiguration)
{
    DebuggerStartParameters sp;
    QTC_ASSERT(runConfiguration, return sp);
    LocalApplicationRunConfiguration *rc =
            qobject_cast<LocalApplicationRunConfiguration *>(runConfiguration);
    QTC_ASSERT(rc, return sp);

    sp.startMode = StartInternal;
    sp.environment = rc->environment();
    sp.workingDirectory = rc->workingDirectory();
    sp.executable = rc->executable();
    sp.processArgs = rc->commandLineArguments();
    sp.toolChainAbi = rc->abi();
    sp.useTerminal = rc->runMode() == LocalApplicationRunConfiguration::Console;
    sp.dumperLibrary = rc->dumperLibrary();
    sp.dumperLibraryLocations = rc->dumperLibraryLocations();

    if (const ProjectExplorer::Target *target = runConfiguration->target()) {
        if (const ProjectExplorer::Project *project = target->project()) {
              sp.projectDir = project->projectDirectory();
              if (const ProjectExplorer::BuildConfiguration *buildConfig = target->activeBuildConfiguration()) {
                  sp.projectBuildDir = buildConfig->buildDirectory();
                  if (const ProjectExplorer::ToolChain *tc = buildConfig->toolChain())
                    sp.debuggerCommand = tc->debuggerCommand();
              }
        }
    }

    if (debuggerCore()->isActiveDebugLanguage(QmlLanguage)) {
        sp.qmlServerAddress = _("127.0.0.1");
        sp.qmlServerPort = runConfiguration->qmlDebugServerPort();

        // Makes sure that all bindings go through the JavaScript engine, so that
        // breakpoints are actually hit!
        const QString optimizerKey = _("QML_DISABLE_OPTIMIZER");
        if (!sp.environment.hasKey(optimizerKey)) {
            sp.environment.set(optimizerKey, _("1"));
        }

        Utils::QtcProcess::addArg(&sp.processArgs, _("-qmljsdebugger=port:")
                                  + QString::number(sp.qmlServerPort));
    }

    // FIXME: If it's not yet build this will be empty and not filled
    // when rebuild as the runConfiguration is not stored and therefore
    // cannot be used to retrieve the dumper location.
    //qDebug() << "DUMPER: " << sp.dumperLibrary << sp.dumperLibraryLocations;
    sp.displayName = rc->displayName();

    return sp;
}

RunControl *DebuggerRunControlFactory::create
    (RunConfiguration *runConfiguration, const QString &mode)
{
    QTC_ASSERT(mode == Constants::DEBUGMODE, return 0);
    DebuggerStartParameters sp = localStartParameters(runConfiguration);
    return create(sp, runConfiguration);
}

RunConfigWidget *DebuggerRunControlFactory::createConfigurationWidget
    (RunConfiguration *runConfiguration)
{
    // NBS TODO: Add GDB-specific configuration widget
    Q_UNUSED(runConfiguration)
    return 0;
}

DebuggerRunControl *DebuggerRunControlFactory::create
    (const DebuggerStartParameters &sp, RunConfiguration *runConfiguration)
{
    const ConfigurationCheck check = checkDebugConfiguration(sp);

    if (!check) {
        //appendMessage(errorMessage, true);
        Core::ICore::instance()->showWarningWithOptions(DebuggerRunControl::tr("Debugger"),
            check.errorMessage, check.errorDetailsString(), check.settingsCategory, check.settingsPage);
        return 0;
    }

    return new DebuggerRunControl(runConfiguration, sp, check.masterSlaveEngineTypes);
}

DebuggerEngine *
    DebuggerRunControlFactory::createEngine(DebuggerEngineType et,
                                            const DebuggerStartParameters &sp,
                                            DebuggerEngine *masterEngine,
                                            QString *errorMessage)
{
    switch (et) {
    case GdbEngineType:
        return createGdbEngine(sp, masterEngine);
    case ScriptEngineType:
        return createScriptEngine(sp);
    case CdbEngineType:
        return createCdbEngine(sp, masterEngine, errorMessage);
        break;
    case PdbEngineType:
        return createPdbEngine(sp);
        break;
    case TcfEngineType:
        return createTcfEngine(sp);
        break;
    case QmlEngineType:
        return createQmlEngine(sp, masterEngine);
        break;
    case LldbEngineType:
        return createLldbEngine(sp);
    default:
        break;
    }
    *errorMessage = DebuggerRunControl::tr("Unable to create a debugger engine of the type '%1'").
                    arg(_(engineTypeName(et)));
    return 0;
}


} // namespace Debugger
