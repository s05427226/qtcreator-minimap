/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
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
** If you are unsure which license is appropriate for your use, please
** contact the sales department at qt-sales@nokia.com.
**
**************************************************************************/
#include "maemoqemuruntimeparser.h"

#include "maemoglobal.h"

#include <qt4projectmanager/qtversionmanager.h>

#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>

namespace Qt4ProjectManager {
namespace Internal {

class MaemoQemuRuntimeParserV1 : public MaemoQemuRuntimeParser
{
public:
    MaemoQemuRuntimeParserV1(const QString &madInfoOutput,
        const QString &targetName, const QString &maddeRoot);
    MaemoQemuRuntime parseRuntime();

private:
    void fillRuntimeInformation(MaemoQemuRuntime *runtime) const;
    void setEnvironment(MaemoQemuRuntime *runTime, const QString &envSpec) const;

    const QString m_maddeRoot;
};

class MaemoQemuRuntimeParserV2 : public MaemoQemuRuntimeParser
{
public:
    MaemoQemuRuntimeParserV2(const QString &madInfoOutput,
        const QString &targetName);
    MaemoQemuRuntime parseRuntime();

private:
    void handleTargetTag(QString &runtimeName);
    MaemoQemuRuntime handleRuntimeTag();
    QHash<QString, QString> handleEnvironmentTag();
    QPair<QString, QString> handleVariableTag();
    MaemoPortList handleTcpPortListTag();
    int handlePortTag();
};

MaemoQemuRuntimeParser::MaemoQemuRuntimeParser(const QString &madInfoOutput,
    const QString &targetName)
    : m_madInfoReader(madInfoOutput), m_targetName(targetName)
{
}

MaemoQemuRuntime MaemoQemuRuntimeParser::parseRuntime(const QtVersion *qtVersion)
{
    MaemoQemuRuntime runtime;
    const QString maddeRootPath
        = MaemoGlobal::maddeRoot(qtVersion->qmakeCommand());
    const QString madCommand = maddeRootPath + QLatin1String("/bin/mad");
    if (!QFileInfo(madCommand).exists())
        return runtime;
    QProcess madProc;
    MaemoGlobal::callMaddeShellScript(madProc, maddeRootPath, madCommand,
        QStringList() << QLatin1String("info"));
    if (!madProc.waitForStarted() || !madProc.waitForFinished())
        return runtime;
    const QByteArray &madInfoOutput = madProc.readAllStandardOutput();
    const QString &targetName
        = MaemoGlobal::targetName(qtVersion->qmakeCommand());
    runtime = MaemoQemuRuntimeParserV2(madInfoOutput, targetName).parseRuntime();
    if (!runtime.m_name.isEmpty()) {
        runtime.m_root = maddeRootPath + QLatin1String("/runtimes/")
            + runtime.m_name;

        // TODO: Workaround for missing ssh tag. Fix once MADDE is ready.
        runtime.m_sshPort = QLatin1String("6666");
    } else {
        runtime = MaemoQemuRuntimeParserV1(madInfoOutput, targetName,
            maddeRootPath).parseRuntime();
    }
    runtime.m_watchPath = runtime.m_root
        .left(runtime.m_root.lastIndexOf(QLatin1Char('/')));

    return runtime;
}

MaemoQemuRuntimeParserV1::MaemoQemuRuntimeParserV1(const QString &madInfoOutput,
    const QString &targetName, const QString &maddeRoot)
    : MaemoQemuRuntimeParser(madInfoOutput, targetName), m_maddeRoot(maddeRoot)
{
}

MaemoQemuRuntime MaemoQemuRuntimeParserV1::parseRuntime()
{
    QStringList installedRuntimes;
    QString targetRuntime;
    while (!m_madInfoReader.atEnd() && !installedRuntimes.contains(targetRuntime)) {
        if (m_madInfoReader.readNext() == QXmlStreamReader::StartElement) {
            if (targetRuntime.isEmpty()
                && m_madInfoReader.name() == QLatin1String("target")) {
                const QXmlStreamAttributes &attrs = m_madInfoReader.attributes();
                if (attrs.value(QLatin1String("target_id")) == targetName())
                    targetRuntime = attrs.value("runtime_id").toString();
            } else if (m_madInfoReader.name() == QLatin1String("runtime")) {
                const QXmlStreamAttributes attrs = m_madInfoReader.attributes();
                while (!m_madInfoReader.atEnd()) {
                    if (m_madInfoReader.readNext() == QXmlStreamReader::EndElement
                         && m_madInfoReader.name() == QLatin1String("runtime"))
                        break;
                    if (m_madInfoReader.tokenType() == QXmlStreamReader::StartElement
                        && m_madInfoReader.name() == QLatin1String("installed")) {
                        if (m_madInfoReader.readNext() == QXmlStreamReader::Characters
                            && m_madInfoReader.text() == QLatin1String("true")) {
                            if (attrs.hasAttribute(QLatin1String("runtime_id")))
                                installedRuntimes << attrs.value(QLatin1String("runtime_id")).toString();
                            else if (attrs.hasAttribute(QLatin1String("id"))) {
                                // older MADDE seems to use only id
                                installedRuntimes << attrs.value(QLatin1String("id")).toString();
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    MaemoQemuRuntime runtime;
    if (installedRuntimes.contains(targetRuntime)) {
        runtime.m_name = targetRuntime;
        runtime.m_root = m_maddeRoot + QLatin1String("/runtimes/")
            + targetRuntime;
        fillRuntimeInformation(&runtime);
    }
    return runtime;

}

void MaemoQemuRuntimeParserV1::fillRuntimeInformation(MaemoQemuRuntime *runtime) const
{
    const QStringList files = QDir(runtime->m_root).entryList(QDir::Files
        | QDir::NoSymLinks | QDir::NoDotAndDotDot);

    const QLatin1String infoFile("information");
    if (files.contains(infoFile)) {
        QFile file(runtime->m_root + QLatin1Char('/') + infoFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMap<QString, QString> map;
            QTextStream stream(&file);
            while (!stream.atEnd()) {
                const QString &line = stream.readLine().trimmed();
                const int index = line.indexOf(QLatin1Char('='));
                map.insert(line.mid(0, index).remove(QLatin1Char('\'')),
                    line.mid(index + 1).remove(QLatin1Char('\'')));
            }

            runtime->m_bin = map.value(QLatin1String("qemu"));
            runtime->m_args = map.value(QLatin1String("qemu_args"));
            setEnvironment(runtime, map.value(QLatin1String("libpath")));
            runtime->m_sshPort = map.value(QLatin1String("sshport"));
            runtime->m_freePorts = MaemoPortList();
            int i = 2;
            while (true) {
                const QString port = map.value(QLatin1String("redirport")
                    + QString::number(i++));
                if (port.isEmpty())
                    break;
                runtime->m_freePorts.addPort(port.toInt());
            }
            return;
        }
    }
}

void MaemoQemuRuntimeParserV1::setEnvironment(MaemoQemuRuntime *runTime,
    const QString &envSpec) const
{
    QString remainingEnvSpec = envSpec;
    QString currentKey;
    while (true) {
        const int nextEqualsSignPos
            = remainingEnvSpec.indexOf(QLatin1Char('='));
        if (nextEqualsSignPos == -1) {
            if (!currentKey.isEmpty())
                runTime->m_environment.insert(currentKey, remainingEnvSpec);
            break;
        }
        const int keyStartPos
            = remainingEnvSpec.lastIndexOf(QRegExp(QLatin1String("\\s")),
                nextEqualsSignPos) + 1;
        if (!currentKey.isEmpty()) {
            const int valueEndPos
                = remainingEnvSpec.lastIndexOf(QRegExp(QLatin1String("\\S")),
                    qMax(0, keyStartPos - 1)) + 1;
            runTime->m_environment.insert(currentKey,
                remainingEnvSpec.left(valueEndPos));
        }
        currentKey = remainingEnvSpec.mid(keyStartPos,
            nextEqualsSignPos - keyStartPos);
        remainingEnvSpec.remove(0, nextEqualsSignPos + 1);
    }
}


MaemoQemuRuntimeParserV2::MaemoQemuRuntimeParserV2(const QString &madInfoOutput,
    const QString &targetName)
    : MaemoQemuRuntimeParser(madInfoOutput, targetName)
{
}

MaemoQemuRuntime MaemoQemuRuntimeParserV2::parseRuntime()
{
    QString runtimeName;
    QList<MaemoQemuRuntime> runtimes;
    while (m_madInfoReader.readNextStartElement()) {
        if (m_madInfoReader.name() == QLatin1String("madde")) {
            while (m_madInfoReader.readNextStartElement()) {
                if (m_madInfoReader.name() == QLatin1String("targets")) {
                    while (m_madInfoReader.readNextStartElement())
                        handleTargetTag(runtimeName);
                } else if (m_madInfoReader.name() == QLatin1String("runtimes")) {
                    while (m_madInfoReader.readNextStartElement()) {
                        const MaemoQemuRuntime &rt = handleRuntimeTag();
                        if (!rt.m_name.isEmpty() && !rt.m_bin.isEmpty()
                                && !rt.m_args.isEmpty()) {
                            runtimes << rt;
                        }
                    }
                } else {
                    m_madInfoReader.skipCurrentElement();
                }
            }
        }
    }
    foreach (const MaemoQemuRuntime &rt, runtimes) {
        if (rt.m_name == runtimeName)
            return rt;
    }
    return MaemoQemuRuntime();
}

void MaemoQemuRuntimeParserV2::handleTargetTag(QString &runtimeName)
{
    const QXmlStreamAttributes &attrs = m_madInfoReader.attributes();
    if (m_madInfoReader.name() == QLatin1String("target") && runtimeName.isEmpty()
            && attrs.value(QLatin1String("name")) == targetName()
            && attrs.value(QLatin1String("installed")) == QLatin1String("true")) {
        while (m_madInfoReader.readNextStartElement()) {
            if (m_madInfoReader.name() == QLatin1String("runtime"))
                runtimeName = m_madInfoReader.readElementText();
            else
                m_madInfoReader.skipCurrentElement();
        }
    } else {
        m_madInfoReader.skipCurrentElement();
    }
}

MaemoQemuRuntime MaemoQemuRuntimeParserV2::handleRuntimeTag()
{
    MaemoQemuRuntime runtime;
    const QXmlStreamAttributes &attrs = m_madInfoReader.attributes();
    if (m_madInfoReader.name() != QLatin1String("runtime")
            || attrs.value(QLatin1String("installed")) != QLatin1String("true")) {
        m_madInfoReader.skipCurrentElement();
        return runtime;
    }
    runtime.m_name = attrs.value(QLatin1String("name")).toString();
    while (m_madInfoReader.readNextStartElement()) {
        if (m_madInfoReader.name() == QLatin1String("exec-path")) {
            runtime.m_bin = m_madInfoReader.readElementText();
        } else if (m_madInfoReader.name() == QLatin1String("args")) {
            runtime.m_args = m_madInfoReader.readElementText();
        } else if (m_madInfoReader.name() == QLatin1String("environment")) {
            runtime.m_environment = handleEnvironmentTag();
        } else if (m_madInfoReader.name() == QLatin1String("tcpportmap")) {
            runtime.m_freePorts = handleTcpPortListTag();
        } else {
            m_madInfoReader.skipCurrentElement();
        }
    }
    return runtime;
}

QHash<QString, QString> MaemoQemuRuntimeParserV2::handleEnvironmentTag()
{
    QHash<QString, QString> env;
    while (m_madInfoReader.readNextStartElement()) {
        const QPair<QString, QString> &var = handleVariableTag();
        if (!var.first.isEmpty())
            env.insert(var.first, var.second);
    }
    return env;
}

QPair<QString, QString> MaemoQemuRuntimeParserV2::handleVariableTag()
{
    QPair<QString, QString> var;
    if (m_madInfoReader.name() != QLatin1String("variable")) {
        m_madInfoReader.skipCurrentElement();
        return var;
    }

    // TODO: Check for "purpose" attribute and handle "glbackend" in a special way
    while (m_madInfoReader.readNextStartElement()) {
        if (m_madInfoReader.name() == QLatin1String("name"))
            var.first = m_madInfoReader.readElementText();
        else if (m_madInfoReader.name() == QLatin1String("value"))
            var.second = m_madInfoReader.readElementText();
        else
            m_madInfoReader.skipCurrentElement();
    }
    return var;
}

MaemoPortList MaemoQemuRuntimeParserV2::handleTcpPortListTag()
{
    MaemoPortList ports;
    while (m_madInfoReader.readNextStartElement()) {
        const int port = handlePortTag();
        if (port != -1 && port != 6666) // TODO: Remove second condition once MADDE has ssh tag
            ports.addPort(port);
    }
    return ports;
}

int MaemoQemuRuntimeParserV2::handlePortTag()
{
    int port = -1;
    if (m_madInfoReader.name() == QLatin1String("port")) {
        while (m_madInfoReader.readNextStartElement()) {
            if (m_madInfoReader.name() == QLatin1String("host"))
                port = m_madInfoReader.readElementText().toInt();
            else
                m_madInfoReader.skipCurrentElement();
        }
    }
    return port;
}

}   // namespace Internal
}   // namespace Qt4ProjectManager