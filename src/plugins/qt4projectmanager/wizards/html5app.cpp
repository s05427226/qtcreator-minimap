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

#include "html5app.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>

#ifndef CREATORLESSTEST
#include <coreplugin/icore.h>
#endif // CREATORLESSTEST

namespace Qt4ProjectManager {
namespace Internal {

const QString appViewerBaseName(QLatin1String("html5applicationviewer"));
const QString appViewerPriFileName(appViewerBaseName + QLatin1String(".pri"));
const QString appViewerCppFileName(appViewerBaseName + QLatin1String(".cpp"));
const QString appViewerHFileName(appViewerBaseName + QLatin1String(".h"));
const QString appViewerOriginsSubDir(appViewerBaseName + QLatin1Char('/'));

Html5App::Html5App()
    : AbstractMobileApp()
    , m_mainHtmlMode(ModeGenerate)
{
}

Html5App::~Html5App()
{
}

void Html5App::setMainHtml(Mode mode, const QString &data)
{
    Q_ASSERT(mode != ModeGenerate || data.isEmpty());
    m_mainHtmlMode = mode;
    m_mainHtmlData = data;
}

Html5App::Mode Html5App::mainHtmlMode() const
{
    return m_mainHtmlMode;
}

QString Html5App::pathExtended(int fileType) const
{
    const QString appViewerTargetSubDir = appViewerOriginsSubDir;
    const QString indexHtml = QLatin1String("index.html");
    const QString pathBase = outputPathBase();
    const QDir appProFilePath(pathBase);
    const bool generateHtml = m_mainHtmlMode == ModeGenerate;
    const bool importHtml = m_mainHtmlMode == ModeImport;
    const QFileInfo importedHtmlFile(m_mainHtmlData);
    const QString htmlSubDir = importHtml ? importedHtmlFile.canonicalPath().split(QLatin1Char('/')).last() + QLatin1Char('/')
                                          : QString::fromLatin1("html/");

    switch (fileType) {
        case MainHtml:                      return generateHtml ? pathBase + htmlSubDir + indexHtml
                                                                : importedHtmlFile.canonicalFilePath();
        case MainHtmlDeployed:              return generateHtml ? QString(htmlSubDir + indexHtml)
                                                                : htmlSubDir + importedHtmlFile.fileName();
        case MainHtmlOrigin:                return originsRoot() + QLatin1String("html/") + indexHtml;
        case AppViewerPri:                  return pathBase + appViewerTargetSubDir + appViewerPriFileName;
        case AppViewerPriOrigin:            return originsRoot() + appViewerOriginsSubDir + appViewerPriFileName;
        case AppViewerCpp:                  return pathBase + appViewerTargetSubDir + appViewerCppFileName;
        case AppViewerCppOrigin:            return originsRoot() + appViewerOriginsSubDir + appViewerCppFileName;
        case AppViewerH:                    return pathBase + appViewerTargetSubDir + appViewerHFileName;
        case AppViewerHOrigin:              return originsRoot() + appViewerOriginsSubDir + appViewerHFileName;
        case HtmlDir:                       return pathBase + htmlSubDir;
        case HtmlDirProFileRelative:        return generateHtml ? QString(htmlSubDir).remove(htmlSubDir.length() - 1, 1)
                                                                : appProFilePath.relativeFilePath(importedHtmlFile.canonicalPath());
        default:                            qFatal("Html5App::pathExtended() needs more work");
    }
    return QString();
}

QString Html5App::originsRoot() const
{
    return templatesRoot() + QLatin1String("html5app/");
}

QString Html5App::mainWindowClassName() const
{
    return QLatin1String("Html5ApplicationViewer");
}

bool Html5App::adaptCurrentMainCppTemplateLine(QString &line) const
{
    const QLatin1Char quote('"');
    bool adaptLine = true;
    if (line.contains(QLatin1String("// MAINHTMLFILE"))) {
        if (m_mainHtmlMode != ModeUrl)
            insertParameter(line, quote + path(MainHtmlDeployed) + quote);
        else
            adaptLine = false;
    } else if (line.contains(QLatin1String("// MAINHTMLURL"))) {
        if (m_mainHtmlMode == ModeUrl)
            insertParameter(line, quote + m_mainHtmlData + quote);
        else
            adaptLine = false;
    }
    return adaptLine;
}

void Html5App::handleCurrentProFileTemplateLine(const QString &line,
    QTextStream &proFileTemplate, QTextStream &proFile,
    bool &commentOutNextLine) const
{
    Q_UNUSED(proFile)
    Q_UNUSED(commentOutNextLine)
    if (line.contains(QLatin1String("# INCLUDE_DEPLOYMENT_PRI"))) {
        proFileTemplate.readLine(); // eats 'include(deployment.pri)'
    }
}

#ifndef CREATORLESSTEST
Core::GeneratedFiles Html5App::generateFiles(QString *errorMessage) const
{
    Core::GeneratedFiles files = AbstractMobileApp::generateFiles(errorMessage);
    if (m_mainHtmlMode == ModeGenerate) {
        files.append(file(generateFile(Html5AppGeneratedFileInfo::MainHtmlFile, errorMessage), path(MainHtml)));
        files.last().setAttributes(Core::GeneratedFile::OpenEditorAttribute);
    }

    files.append(file(generateFile(Html5AppGeneratedFileInfo::AppViewerPriFile, errorMessage), path(AppViewerPri)));
    files.append(file(generateFile(Html5AppGeneratedFileInfo::AppViewerCppFile, errorMessage), path(AppViewerCpp)));
    files.append(file(generateFile(Html5AppGeneratedFileInfo::AppViewerHFile, errorMessage), path(AppViewerH)));

    return files;
}
#endif // CREATORLESSTEST

QByteArray Html5App::generateFileExtended(int fileType,
    bool *versionAndCheckSum, QString *comment, QString *errorMessage) const
{
    QByteArray data;
    switch (fileType) {
        case Html5AppGeneratedFileInfo::MainHtmlFile:
            data = readBlob(path(MainHtmlOrigin), errorMessage);
            break;
        case Html5AppGeneratedFileInfo::AppViewerPriFile:
            data = readBlob(path(AppViewerPriOrigin), errorMessage);
            data.append(readBlob(path(DeploymentPriOrigin), errorMessage));
            *comment = ProFileComment;
            *versionAndCheckSum = true;
            break;
        case Html5AppGeneratedFileInfo::AppViewerCppFile:
            data = readBlob(path(AppViewerCppOrigin), errorMessage);
            *versionAndCheckSum = true;
            break;
        case Html5AppGeneratedFileInfo::AppViewerHFile:
        default:
            data = readBlob(path(AppViewerHOrigin), errorMessage);
            *versionAndCheckSum = true;
            break;
    }
    return data;
}

int Html5App::stubVersionMinor() const
{
    return StubVersion;
}

QList<AbstractGeneratedFileInfo> Html5App::updateableFiles(const QString &mainProFile) const
{
    QList<AbstractGeneratedFileInfo> result;
    static const struct {
        int fileType;
        QString fileName;
    } files[] = {
        {Html5AppGeneratedFileInfo::AppViewerPriFile, appViewerPriFileName},
        {Html5AppGeneratedFileInfo::AppViewerHFile, appViewerHFileName},
        {Html5AppGeneratedFileInfo::AppViewerCppFile, appViewerCppFileName}
    };
    const QFileInfo mainProFileInfo(mainProFile);
    const int size = sizeof(files) / sizeof(files[0]);
    for (int i = 0; i < size; ++i) {
        const QString fileName = mainProFileInfo.dir().absolutePath()
                + QLatin1Char('/') + appViewerOriginsSubDir + files[i].fileName;
        if (!QFile::exists(fileName))
            continue;
        Html5AppGeneratedFileInfo file;
        file.fileType = files[i].fileType;
        file.fileInfo = QFileInfo(fileName);
        file.currentVersion = AbstractMobileApp::makeStubVersion(Html5App::StubVersion);
        result.append(file);
    }
    if (result.count() != size)
        result.clear(); // All files must be found. No wrong/partial updates, please.
    return result;
}

QList<DeploymentFolder> Html5App::deploymentFolders() const
{
    QList<DeploymentFolder> result;
    if (m_mainHtmlMode != ModeUrl)
        result.append(DeploymentFolder(path(HtmlDirProFileRelative), QLatin1String(".")));
    return result;
}

const int Html5App::StubVersion = 10;

} // namespace Internal
} // namespace Qt4ProjectManager
