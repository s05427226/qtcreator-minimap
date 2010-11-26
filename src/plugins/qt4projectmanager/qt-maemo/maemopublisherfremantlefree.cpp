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
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/
#include "maemopublisherfremantlefree.h"

#include "maemoglobal.h"
#include "maemopackagecreationstep.h"
#include "maemopublishingfileselectiondialog.h"
#include "maemotoolchain.h"

#include <coreplugin/ifile.h>
#include <projectexplorer/project.h>
#include <qt4projectmanager/qmakestep.h>
#include <qt4projectmanager/qt4buildconfiguration.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>

#define ASSERT_STATE(state) ASSERT_STATE_GENERIC(State, state, m_state)

using namespace Core;

namespace Qt4ProjectManager {
namespace Internal {

MaemoPublisherFremantleFree::MaemoPublisherFremantleFree(const ProjectExplorer::Project *project,
    QObject *parent) :
    QObject(parent),
    m_project(project),
    m_state(Inactive),
    m_sshParams(SshConnectionParameters::DefaultProxy)
{
    m_sshParams.authType = SshConnectionParameters::AuthByKey;
    m_sshParams.timeout = 30;
    m_sshParams.port = 22;
    m_process = new QProcess(this);
    connect(m_process, SIGNAL(finished(int,QProcess::ExitStatus)),
        SLOT(handleProcessFinished()));
    connect(m_process, SIGNAL(error(QProcess::ProcessError)),
        SLOT(handleProcessError(QProcess::ProcessError)));
    connect(m_process, SIGNAL(readyReadStandardOutput()),
        SLOT(handleProcessStdOut()));
    connect(m_process, SIGNAL(readyReadStandardError()),
        SLOT(handleProcessStdErr()));
}

MaemoPublisherFremantleFree::~MaemoPublisherFremantleFree()
{
    ASSERT_STATE(Inactive);
    m_process->kill();
}

void MaemoPublisherFremantleFree::publish()
{
    createPackage();
}

void MaemoPublisherFremantleFree::setSshParams(const QString &hostName,
    const QString &userName, const QString &keyFile, const QString &remoteDir)
{
    Q_ASSERT(m_doUpload);
    m_sshParams.host = hostName;
    m_sshParams.uname = userName;
    m_sshParams.privateKeyFile = keyFile;
    m_remoteDir = remoteDir;
}

void MaemoPublisherFremantleFree::cancel()
{
    finishWithFailure(tr("Canceled."), tr("Publishing canceled by user."));
}

void MaemoPublisherFremantleFree::createPackage()
{
    // Step 1: Copy project directory.
    setState(CopyingProjectDir);
    m_tmpProjectDir = tmpDirContainer() + QLatin1Char('/')
        + m_project->displayName();
    if (QFileInfo(tmpDirContainer()).exists()) {
        emit progressReport(tr("Removing left-over temporary directory ..."));
        QString error;
        if (!MaemoGlobal::removeRecursively(tmpDirContainer(), error)) {
            finishWithFailure(tr("Error removing temporary directory: %1").arg(error),
                tr("Publishing failed: Could not create source package."));
            return;
        }
    }

    emit progressReport(tr("Setting up temporary directory ..."));
    if (!QDir::temp().mkdir(QFileInfo(tmpDirContainer()).fileName())) {
        finishWithFailure(tr("Error: Could not create temporary directory."),
            tr("Publishing failed: Could not create source package."));
        return;
    }
    if (!copyRecursively(m_project->projectDirectory(), m_tmpProjectDir)) {
        finishWithFailure(tr("Error: Could not copy project directory"),
            tr("Publishing failed: Could not create source package."));
        return;
    }

    emit progressReport(tr("Cleaning up temporary directory ..."));
    QString error;
    if (!MaemoPackageCreationStep::preparePackagingProcess(m_process,
            m_buildConfig, m_tmpProjectDir, &error)) {
        finishWithFailure(tr("Error preparing packaging process: %1").arg(error),
            tr("Publishing failed: Could not create package."));
        return;
    }

    setState(RunningQmake);
    const ProjectExplorer::ProcessParameters * const pp
        = m_buildConfig->qmakeStep()->processParameters();
    m_process->start(pp->effectiveCommand() + QLatin1String(" ")
        + pp->effectiveArguments());
}

bool MaemoPublisherFremantleFree::copyRecursively(const QString &srcFilePath,
    const QString &tgtFilePath)
{
    if (m_state == Inactive)
        return true;

    QFileInfo srcFileInfo(srcFilePath);
    if (srcFileInfo.isDir()) {
        if (srcFileInfo == QFileInfo(m_project->projectDirectory()
               + QLatin1String("/debian")))
            return true;
        QString actualSourcePath = srcFilePath;
        QString actualTargetPath = tgtFilePath;

        if (srcFileInfo.fileName() == QLatin1String("qtc_packaging")) {
            actualSourcePath += QLatin1String("/debian_fremantle");
            actualTargetPath.replace(QRegExp(QLatin1String("qtc_packaging$")),
                QLatin1String("debian"));
        }

        QDir targetDir(actualTargetPath);
        targetDir.cdUp();
        if (!targetDir.mkdir(QFileInfo(actualTargetPath).fileName())) {
            emit progressReport(tr("Failed to create directory '%1'.")
                .arg(QDir::toNativeSeparators(actualTargetPath)), ErrorOutput);
            return false;
        }
        QDir sourceDir(actualSourcePath);
        QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::Hidden
            | QDir::System | QDir::Dirs | QDir::NoDotAndDotDot);
        foreach (const QString &fileName, fileNames) {
            if (!copyRecursively(actualSourcePath + QLatin1Char('/') + fileName,
                    actualTargetPath + QLatin1Char('/') + fileName))
                return false;
        }
    } else {
        if (!QFile::copy(srcFilePath, tgtFilePath)) {
            emit progressReport(tr("Could not copy file '%1' to '%2'.")
                .arg(QDir::toNativeSeparators(srcFilePath),
                     QDir::toNativeSeparators(tgtFilePath)));
            return false;
        }
        QCoreApplication::processEvents();

        if (tgtFilePath == m_tmpProjectDir + QLatin1String("/debian/rules")) {
            QFile rulesFile(tgtFilePath);
            if (!rulesFile.open(QIODevice::ReadWrite)) {
                emit progressReport(tr("Error: Cannot open file '%1'.")
                    .arg(QDir::toNativeSeparators(tgtFilePath)));
                return false;
            }
            QByteArray rulesContents = rulesFile.readAll();
            rulesContents.replace("$(MAKE) clean", "# $(MAKE) clean");
            rulesContents.replace("# Add here commands to configure the package.",
                "qmake " + QFileInfo(m_project->file()->fileName()).fileName().toLocal8Bit());
            rulesFile.resize(0);
            rulesFile.write(rulesContents);
        }
    }
    return true;
}

void MaemoPublisherFremantleFree::handleProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart)
        handleProcessFinished(true);
}

void MaemoPublisherFremantleFree::handleProcessFinished()
{
    handleProcessFinished(false);
}

void MaemoPublisherFremantleFree::handleProcessStdOut()
{
    if (m_state == RunningQmake || m_state == RunningMakeDistclean
            || m_state == BuildingPackage) {
        emit progressReport(QString::fromLocal8Bit(m_process->readAllStandardOutput()),
            ToolStatusOutput);
    }
}

void MaemoPublisherFremantleFree::handleProcessStdErr()
{
    if (m_state == RunningQmake || m_state == RunningMakeDistclean
            || m_state == BuildingPackage) {
        emit progressReport(QString::fromLocal8Bit(m_process->readAllStandardError()),
            ToolErrorOutput);
    }
}

void MaemoPublisherFremantleFree::handleProcessFinished(bool failedToStart)
{
    ASSERT_STATE(QList<State>() << RunningQmake << RunningMakeDistclean
        << BuildingPackage << Inactive);

    switch (m_state) {
    case RunningQmake:
        if (failedToStart || m_process->exitStatus() != QProcess::NormalExit
                ||m_process->exitCode() != 0) {
            runDpkgBuildPackage();
        } else {
            setState(RunningMakeDistclean);
            m_process->start(m_buildConfig->makeCommand(),
                QStringList() << QLatin1String("distclean"));
        }
        break;
    case RunningMakeDistclean:
        runDpkgBuildPackage();
        break;
    case BuildingPackage: {
        QString error;
        if (failedToStart) {
            error = tr("Error: Failed to start dpkg-buildpackage.");
        } else if (m_process->exitStatus() != QProcess::NormalExit
                   || m_process->exitCode() != 0) {
            error = tr("Error: dpkg-buildpackage did not succeed.");
        }

        if (!error.isEmpty()) {
            finishWithFailure(error, tr("Package creation failed."));
            return;
        }

        QDir dir(tmpDirContainer());
        const QStringList &fileNames = dir.entryList(QDir::Files);
        foreach (const QString &fileName, fileNames) {
            const QString filePath
                = tmpDirContainer() + QLatin1Char('/') + fileName;
            if (fileName.endsWith(QLatin1String(".dsc")))
                m_filesToUpload.append(filePath);
            else
                m_filesToUpload.prepend(filePath);
        }
        if (!m_doUpload) {
            emit progressReport(tr("Done."));
            QStringList nativeFilePaths;
            foreach (const QString &filePath, m_filesToUpload)
                nativeFilePaths << QDir::toNativeSeparators(filePath);
            m_resultString = tr("Packaging finished successfully. "
                "The following files were created:\n")
                + nativeFilePaths.join(QLatin1String("\n"));
            setState(Inactive);
        } else {
            uploadPackage();
        }
        break;
    }
    default:
        break;
    }
}

void MaemoPublisherFremantleFree::runDpkgBuildPackage()
{
    MaemoPublishingFileSelectionDialog d(m_tmpProjectDir);
    if (d.exec() == QDialog::Rejected) {
        cancel();
        return;
    }
    foreach (const QString &filePath, d.filesToExclude()) {
        QString error;
        if (!MaemoGlobal::removeRecursively(filePath, error)) {
            finishWithFailure(error,
                tr("Publishing failed: Could not create package."));
        }
    }

    if (m_state == Inactive)
        return;
    setState(BuildingPackage);
    emit progressReport(tr("Building source package..."));
    const MaemoToolChain * const tc
        = dynamic_cast<MaemoToolChain *>(m_buildConfig->toolChain());
    QStringList args = QStringList() << QLatin1String("-t")
        << tc->targetName() << QLatin1String("dpkg-buildpackage")
        << QLatin1String("-S") << QLatin1String("-us") << QLatin1String("-uc");
    QString madCommand = tc->maddeRoot() + QLatin1String("/bin/mad");

#ifdef Q_OS_WIN
    args.prepend(madCommand);
    madCommand = tc->maddeRoot() + QLatin1String("/bin/sh.exe");
#endif
    m_process->start(madCommand, args);
}

// We have to implement the SCP protocol, because the maemo.org
// webmaster refuses to enable SFTP "for security reasons" ...
void MaemoPublisherFremantleFree::uploadPackage()
{
    m_uploader = SshRemoteProcessRunner::create(m_sshParams);
    connect(m_uploader.data(), SIGNAL(processStarted()),
        SLOT(handleScpStarted()));
    connect(m_uploader.data(), SIGNAL(connectionError(Core::SshError)),
        SLOT(handleConnectionError()));
    connect(m_uploader.data(), SIGNAL(processClosed(int)),
        SLOT(handleUploadJobFinished(int)));
    connect(m_uploader.data(), SIGNAL(processOutputAvailable(QByteArray)),
        SLOT(handleScpStdOut(QByteArray)));
    emit progressReport(tr("Starting scp ..."));
    setState(StartingScp);
    m_uploader->run("scp -td " + m_remoteDir.toUtf8());
}

void MaemoPublisherFremantleFree::handleScpStarted()
{
    ASSERT_STATE(QList<State>() << StartingScp << Inactive);

    if (m_state == StartingScp)
        prepareToSendFile();
}

void MaemoPublisherFremantleFree::handleConnectionError()
{
    if (m_state != Inactive) {
        finishWithFailure(tr("SSH error: %1").arg(m_uploader->connection()->errorString()),
            tr("Upload failed."));
    }
}

void MaemoPublisherFremantleFree::handleUploadJobFinished(int exitStatus)
{
    ASSERT_STATE(QList<State>() << PreparingToUploadFile << UploadingFile
        << Inactive);

    if (m_state != Inactive && (exitStatus != SshRemoteProcess::ExitedNormally
            || m_uploader->process()->exitCode() != 0)) {
        QString error;
        if (exitStatus != SshRemoteProcess::ExitedNormally) {
            error = tr("Error uploading file: %1")
                .arg(m_uploader->process()->errorString());
        } else {
            error = tr("Error uploading file.");
        }
        finishWithFailure(error, tr("Upload failed."));
    }
}

void MaemoPublisherFremantleFree::prepareToSendFile()
{
    if (m_filesToUpload.isEmpty()) {
        emit progressReport(tr("All files uploaded."));
        m_resultString = tr("Upload succeeded. You should shortly "
            "receive an email informing you about the outcome "
            "of the build process.");
        setState(Inactive);
        return;
    }

    setState(PreparingToUploadFile);
    const QString &nextFilePath = m_filesToUpload.first();
    emit progressReport(tr("Uploading file %1 ...")
        .arg(QDir::toNativeSeparators(nextFilePath)));
    QFileInfo info(nextFilePath);
    m_uploader->process()->sendInput("C0644 " + QByteArray::number(info.size())
        + ' ' + info.fileName().toUtf8() + '\n');
}

void MaemoPublisherFremantleFree::sendFile()
{
    Q_ASSERT(!m_filesToUpload.isEmpty());
    Q_ASSERT(m_state == PreparingToUploadFile);

    setState(UploadingFile);
    const QString filePath = m_filesToUpload.takeFirst();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        finishWithFailure(tr("Cannot open file for reading: %1")
            .arg(file.errorString()), tr("Upload failed."));
        return;
    }
    qint64 bytesToSend = file.size();
    while (bytesToSend > 0) {
        const QByteArray &data
            = file.read(qMin(bytesToSend, Q_INT64_C(1024*1024)));
        if (data.count() == 0) {
            finishWithFailure(tr("Cannot read file: %1").arg(file.errorString()),
                tr("Upload failed."));
            return;
        }
        m_uploader->process()->sendInput(data);
        bytesToSend -= data.size();
        QCoreApplication::processEvents();
        if (m_state == Inactive)
            return;
    }
    m_uploader->process()->sendInput(QByteArray(1, '\0'));
}

void MaemoPublisherFremantleFree::handleScpStdOut(const QByteArray &output)
{
    ASSERT_STATE(QList<State>() << PreparingToUploadFile << UploadingFile
        << Inactive);

    if (m_state == Inactive)
        return;

    m_scpOutput += output;
    if (m_scpOutput == QByteArray(1, '\0')) {
        m_scpOutput.clear();
        switch (m_state) {
        case PreparingToUploadFile:
            sendFile();
            break;
        case UploadingFile:
            prepareToSendFile();
            break;
        default:
            break;
        }
    } else if (m_scpOutput.endsWith('\n')) {
        const QByteArray error = m_scpOutput.mid(1, m_scpOutput.count() - 2);
        QString progressError;
        if (!error.isEmpty()) {
            progressError = tr("Error uploading file: %1")
                .arg(QString::fromUtf8(error));
        } else {
            progressError = tr("Error uploading file.");
        }
        finishWithFailure(progressError, tr("Upload failed."));
    }
}

QString MaemoPublisherFremantleFree::tmpDirContainer() const
{
    return QDir::tempPath() + QLatin1String("/qtc_packaging_")
        + m_project->displayName();
}

void MaemoPublisherFremantleFree::finishWithFailure(const QString &progressMsg,
    const QString &resultMsg)
{
    if (!progressMsg.isEmpty())
        emit progressReport(progressMsg, ErrorOutput);
    m_resultString = resultMsg;
    setState(Inactive);
}

void MaemoPublisherFremantleFree::setState(State newState)
{
    if (m_state == newState)
        return;
    const State oldState = m_state;
    m_state = newState;
    if (m_state == Inactive) {
        switch (oldState) {
        case RunningQmake:
        case RunningMakeDistclean:
        case BuildingPackage:
            disconnect(m_process, 0, this, 0);
            m_process->terminate();
            break;
        case StartingScp:
        case PreparingToUploadFile:
        case UploadingFile:
            // TODO: Can we ensure the remote scp exits, e.g. by sending
            //       an illegal sequence of bytes? (Probably not, if
            //       we are currently uploading a file.)
            disconnect(m_uploader.data(), 0, this, 0);
            m_uploader = SshRemoteProcessRunner::Ptr();
            break;
        default:
            break;
        }
        emit finished();
    }
}

} // namespace Internal
} // namespace Qt4ProjectManager