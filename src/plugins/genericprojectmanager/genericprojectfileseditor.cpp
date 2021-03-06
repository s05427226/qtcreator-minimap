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

#include "genericprojectfileseditor.h"
#include "genericprojectmanager.h"
#include "genericprojectconstants.h"

#include <coreplugin/editormanager/editormanager.h>
#include <texteditor/fontsettings.h>
#include <texteditor/texteditoractionhandler.h>
#include <texteditor/texteditorsettings.h>

using namespace GenericProjectManager;
using namespace GenericProjectManager::Internal;


////////////////////////////////////////////////////////////////////////////////////////
// ProjectFilesFactory
////////////////////////////////////////////////////////////////////////////////////////

ProjectFilesFactory::ProjectFilesFactory(Manager *manager,
                                         TextEditor::TextEditorActionHandler *handler)
    : Core::IEditorFactory(manager),
      m_manager(manager),
      m_actionHandler(handler)
{
    m_mimeTypes.append(QLatin1String(Constants::FILES_MIMETYPE));
    m_mimeTypes.append(QLatin1String(Constants::INCLUDES_MIMETYPE));
    m_mimeTypes.append(QLatin1String(Constants::CONFIG_MIMETYPE));
}

ProjectFilesFactory::~ProjectFilesFactory()
{
}

Manager *ProjectFilesFactory::manager() const
{
    return m_manager;
}

Core::IEditor *ProjectFilesFactory::createEditor(QWidget *parent)
{
    ProjectFilesEditorWidget *ed = new ProjectFilesEditorWidget(parent, this, m_actionHandler);
    TextEditor::TextEditorSettings::instance()->initializeEditor(ed);
    return ed->editor();
}

QStringList ProjectFilesFactory::mimeTypes() const
{
    return m_mimeTypes;
}

QString ProjectFilesFactory::id() const
{
    return QLatin1String(Constants::FILES_EDITOR_ID);
}

QString ProjectFilesFactory::displayName() const
{
    return tr(Constants::FILES_EDITOR_DISPLAY_NAME);
}

Core::IFile *ProjectFilesFactory::open(const QString &fileName)
{
    Core::EditorManager *editorManager = Core::EditorManager::instance();

    if (Core::IEditor *editor = editorManager->openEditor(fileName, id()))
        return editor->file();

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// ProjectFilesEditable
////////////////////////////////////////////////////////////////////////////////////////

ProjectFilesEditor::ProjectFilesEditor(ProjectFilesEditorWidget *editor)
  : TextEditor::BaseTextEditor(editor),
    m_context(Constants::C_FILESEDITOR)
{ }

ProjectFilesEditor::~ProjectFilesEditor()
{ }

Core::Context ProjectFilesEditor::context() const
{
    return m_context;
}

QString ProjectFilesEditor::id() const
{
    return QLatin1String(Constants::FILES_EDITOR_ID);
}

bool ProjectFilesEditor::duplicateSupported() const
{
    return true;
}

Core::IEditor *ProjectFilesEditor::duplicate(QWidget *parent)
{
    ProjectFilesEditorWidget *parentEditor = qobject_cast<ProjectFilesEditorWidget *>(editorWidget());
    ProjectFilesEditorWidget *editor = new ProjectFilesEditorWidget(parent,
                                                        parentEditor->factory(),
                                                        parentEditor->actionHandler());
    TextEditor::TextEditorSettings::instance()->initializeEditor(editor);
    return editor->editor();
}

////////////////////////////////////////////////////////////////////////////////////////
// ProjectFilesEditor
////////////////////////////////////////////////////////////////////////////////////////

ProjectFilesEditorWidget::ProjectFilesEditorWidget(QWidget *parent, ProjectFilesFactory *factory,
                                       TextEditor::TextEditorActionHandler *handler)
    : TextEditor::BaseTextEditorWidget(parent),
      m_factory(factory),
      m_actionHandler(handler)
{
    Manager *manager = factory->manager();
    ProjectFilesDocument *doc = new ProjectFilesDocument(manager);
    setBaseTextDocument(doc);

    handler->setupActions(this);
}

ProjectFilesEditorWidget::~ProjectFilesEditorWidget()
{ }

ProjectFilesFactory *ProjectFilesEditorWidget::factory() const
{
    return m_factory;
}

TextEditor::TextEditorActionHandler *ProjectFilesEditorWidget::actionHandler() const
{
    return m_actionHandler;
}

TextEditor::BaseTextEditor *ProjectFilesEditorWidget::createEditor()
{
    return new ProjectFilesEditor(this);
}

////////////////////////////////////////////////////////////////////////////////////////
// ProjectFilesDocument
////////////////////////////////////////////////////////////////////////////////////////

ProjectFilesDocument::ProjectFilesDocument(Manager *manager)
    : m_manager(manager)
{
    setMimeType(QLatin1String(Constants::FILES_MIMETYPE));
}

ProjectFilesDocument::~ProjectFilesDocument()
{ }

bool ProjectFilesDocument::save(const QString &name)
{
    if (! BaseTextDocument::save(name))
        return false;

    m_manager->notifyChanged(name);
    return true;
}
