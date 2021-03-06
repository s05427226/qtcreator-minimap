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

#ifndef DEBUGGER_DEBUGGERSTARTPARAMETERS_H
#define DEBUGGER_DEBUGGERSTARTPARAMETERS_H

#include "debugger_global.h"
#include "debuggerconstants.h"

#include <utils/ssh/sshconnection.h>
#include <utils/environment.h>
#include <projectexplorer/abi.h>

#include <QtCore/QMetaType>

namespace Debugger {

// Note: This is part of the "soft interface" of the debugger plugin.
// Do not add anything that needs implementation in a .cpp file.

class DEBUGGER_EXPORT DebuggerStartParameters
{
public:
    enum CommunicationChannel {
        CommunicationChannelTcpIp,
        CommunicationChannelUsb
    };

    enum DebugClient {
        DebugClientTrk,
        DebugClientCoda
    };

    DebuggerStartParameters()
      : isSnapshot(false),
        attachPID(-1),
        useTerminal(false),
        qmlServerAddress(QLatin1String("127.0.0.1")),
        qmlServerPort(0),
        useServerStartScript(false),
        connParams(Utils::SshConnectionParameters::NoProxy),
        startMode(NoStartMode),
        executableUid(0),
        communicationChannel(CommunicationChannelUsb),
        serverPort(0),
        debugClient(DebugClientTrk)
    {}

    QString executable;
    QString displayName; // Used in the Snapshots view.
    QString startMessage; // First status message shown.
    QString coreFile;
    bool isSnapshot; // Set if created internally.
    QString processArgs;
    Utils::Environment environment;
    QString workingDirectory;
    qint64 attachPID;
    bool useTerminal;

    // Used by AttachCrashedExternal.
    QString crashParameter;

    // Used by Qml debugging.
    QString qmlServerAddress;
    quint16 qmlServerPort;
    QString projectBuildDir;
    QString projectDir;

    // Used by remote debugging.
    QString remoteChannel;
    QString remoteArchitecture;
    QString gnuTarget;
    QString symbolFileName;
    bool useServerStartScript;
    QString serverStartScript;
    QString sysRoot;
    QByteArray remoteDumperLib;
    QByteArray remoteSourcesDir;
    QString remoteMountPoint;
    QString localMountDir;
    Utils::SshConnectionParameters connParams;

    QString debuggerCommand;
    ProjectExplorer::Abi toolChainAbi;

    QString dumperLibrary;
    QStringList dumperLibraryLocations;
    DebuggerStartMode startMode;

    // For Symbian debugging.
    quint32 executableUid;
    CommunicationChannel communicationChannel;
    QString serverAddress;
    quint16 serverPort;
    DebugClient debugClient;
};

} // namespace Debugger

Q_DECLARE_METATYPE(Debugger::DebuggerStartParameters)

#endif // DEBUGGER_DEBUGGERSTARTPARAMETERS_H

