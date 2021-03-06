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

#ifndef DEBUGGER_LLDBENGINE_GUEST_H
#define DEBUGGER_LLDBENGINE_GUEST_H

#include "ipcengineguest.h"

#include <QtCore/QQueue>
#include <QtCore/QVariant>
#include <QtCore/QThread>
#include <QtCore/QStringList>

#include <lldb/API/LLDB.h>

#if defined(HAVE_LLDB_PRIVATE)
#include "pygdbmiemu.h"
#endif

Q_DECLARE_METATYPE (lldb::SBListener *)
Q_DECLARE_METATYPE (lldb::SBEvent *)

namespace Debugger {
namespace Internal {

class LldbEventListener : public QObject
{
Q_OBJECT
public slots:
    void listen(lldb::SBListener *listener);
signals:
    // lldb API uses non thread safe implicit sharing with no explicit copy feature
    // additionally the scope is undefined, hence this signal needs to be connected BlockingQueued
    // whutever, works for now.
    void lldbEvent(lldb::SBEvent *ev);
};


class LldbEngineGuest : public IPCEngineGuest
{
    Q_OBJECT

public:
    explicit LldbEngineGuest();
    ~LldbEngineGuest();

    void nuke();
    void setupEngine();
    void setupInferior(const QString &executable, const QStringList &arguments,
            const QStringList &environment);
    void runEngine();
    void shutdownInferior();
    void shutdownEngine();
    void detachDebugger();
    void executeStep();
    void executeStepOut() ;
    void executeNext();
    void executeStepI();
    void executeNextI();
    void continueInferior();
    void interruptInferior();
    void executeRunToLine(const ContextData &data);
    void executeRunToFunction(const QString &functionName);
    void executeJumpToLine(const ContextData &data);
    void activateFrame(qint64);
    void selectThread(qint64);
    void disassemble(quint64 pc);
    void addBreakpoint(BreakpointId id, const BreakpointParameters &bp);
    void removeBreakpoint(BreakpointId id);
    void changeBreakpoint(BreakpointId id, const BreakpointParameters &bp);
    void requestUpdateWatchData(const WatchData &data,
            const WatchUpdateFlags &flags);
    void fetchFrameSource(qint64 frame);

private:
    bool m_running;

    QList<QByteArray> m_arguments;
    QList<QByteArray> m_environment;
    QThread m_wThread;
    LldbEventListener *m_worker;
    lldb::SBDebugger *m_lldb;
    lldb::SBTarget   *m_target;
    lldb::SBProcess  *m_process;
    lldb::SBListener *m_listener;

    lldb::SBFrame m_currentFrame;
    lldb::SBThread m_currentThread;
    bool m_relistFrames;
    QHash<QString, lldb::SBValue> m_localesCache;
    QHash<BreakpointId, lldb::SBBreakpoint> m_breakpoints;
    QHash<qint64, QString> m_frame_to_file;

    void updateThreads();
    void getWatchDataR(lldb::SBValue v, int level,
            const QByteArray &p_iname, QList<WatchData> &wd);

#if defined(HAVE_LLDB_PRIVATE)
    PythonLLDBToGdbMiHack * py;
#endif

private slots:
    void lldbEvent(lldb::SBEvent *ev);
};

} // namespace Internal
} // namespace Debugger

#endif // DEBUGGER_LLDBENGINE_H
#define SYNC_INFERIOR
