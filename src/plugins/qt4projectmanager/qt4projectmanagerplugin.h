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

#ifndef QT4PROJECTMANAGERPLUGIN_H
#define QT4PROJECTMANAGERPLUGIN_H

#include <extensionsystem/iplugin.h>
#include <coreplugin/icontext.h>

QT_BEGIN_NAMESPACE
class QAction;
QT_END_NAMESPACE

namespace ProjectExplorer {
    class Project;
    class Node;
    class ProjectExplorerPlugin;
}
namespace Qt4ProjectManager {

class Qt4Manager;
class QtVersionManager;

namespace Internal {

class ProFileEditorFactory;
class GettingStartedWelcomePage;

class Qt4ProjectManagerPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT

public:
    ~Qt4ProjectManagerPlugin();
    bool initialize(const QStringList &arguments, QString *error_message);
    void extensionsInitialized();

    Core::Context projectContext() const { return m_projectContext; }

private slots:
    void updateContextMenu(ProjectExplorer::Project *project,
                           ProjectExplorer::Node *node);
    void currentProjectChanged();
    void buildStateChanged(ProjectExplorer::Project *pro);
    void addLibrary();
    void jumpToFile();

#ifdef WITH_TESTS
    void testBasicProjectLoading(); // Test fails!

    void testAbldOutputParsers_data();
    void testAbldOutputParsers();
    void testQtOutputParser_data();
    void testQtOutputParser();
    void testSbsV2OutputParsers_data();
    void testSbsV2OutputParsers();
    void testRvctOutputParser_data();
    void testRvctOutputParser();
    void testQmakeOutputParsers_data();
    void testQmakeOutputParsers();
#endif

private:
    ProjectExplorer::ProjectExplorerPlugin *m_projectExplorer;
    ProFileEditorFactory *m_proFileEditorFactory;
    Qt4Manager *m_qt4ProjectManager;

    QAction *m_runQMakeAction;
    QAction *m_runQMakeActionContextMenu;
    QAction *m_buildSubProjectContextMenu;
    QAction *m_rebuildSubProjectContextMenu;
    QAction *m_cleanSubProjectContextMenu;
    GettingStartedWelcomePage *m_welcomePage;
    Core::Context m_projectContext;
};

} // namespace Internal
} // namespace Qt4ProjectManager

#endif // QT4PROJECTMANAGERPLUGIN_H
