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

#include "subversionplugin.h"

#include "settingspage.h"
#include "subversioneditor.h"

#include "subversionsubmiteditor.h"
#include "subversionconstants.h"
#include "subversioncontrol.h"
#include "checkoutwizard.h"

#include <vcsbase/basevcseditorfactory.h>
#include <vcsbase/vcsbaseeditor.h>
#include <vcsbase/basevcssubmiteditorfactory.h>
#include <vcsbase/vcsbaseoutputwindow.h>
#include <utils/synchronousprocess.h>
#include <utils/parameteraction.h>

#include <coreplugin/icore.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/filemanager.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/mimedatabase.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/uniqueidmanager.h>
#include <coreplugin/editormanager/editormanager.h>

#include <locator/commandlocator.h>

#include <utils/qtcassert.h>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryFile>
#include <QtCore/QTextCodec>
#include <QtCore/QtPlugin>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QUrl>
#include <QtGui/QAction>
#include <QtGui/QFileDialog>
#include <QtGui/QMainWindow>
#include <QtGui/QMenu>
#include <QtGui/QMessageBox>
#include <QtGui/QInputDialog>
#include <QtXml/QXmlStreamReader>
#include <limits.h>

using namespace Subversion::Internal;

static const char * const CMD_ID_SUBVERSION_MENU    = "Subversion.Menu";
static const char * const CMD_ID_ADD                = "Subversion.Add";
static const char * const CMD_ID_DELETE_FILE        = "Subversion.Delete";
static const char * const CMD_ID_REVERT             = "Subversion.Revert";
static const char * const CMD_ID_SEPARATOR0         = "Subversion.Separator0";
static const char * const CMD_ID_DIFF_PROJECT       = "Subversion.DiffAll";
static const char * const CMD_ID_DIFF_CURRENT       = "Subversion.DiffCurrent";
static const char * const CMD_ID_SEPARATOR1         = "Subversion.Separator1";
static const char * const CMD_ID_COMMIT_ALL         = "Subversion.CommitAll";
static const char * const CMD_ID_REVERT_ALL         = "Subversion.RevertAll";
static const char * const CMD_ID_COMMIT_CURRENT     = "Subversion.CommitCurrent";
static const char * const CMD_ID_SEPARATOR2         = "Subversion.Separator2";
static const char * const CMD_ID_FILELOG_CURRENT    = "Subversion.FilelogCurrent";
static const char * const CMD_ID_ANNOTATE_CURRENT   = "Subversion.AnnotateCurrent";
static const char * const CMD_ID_SEPARATOR3         = "Subversion.Separator3";
static const char * const CMD_ID_SEPARATOR4         = "Subversion.Separator4";
static const char * const CMD_ID_STATUS             = "Subversion.Status";
static const char * const CMD_ID_PROJECTLOG         = "Subversion.ProjectLog";
static const char * const CMD_ID_REPOSITORYLOG      = "Subversion.RepositoryLog";
static const char * const CMD_ID_REPOSITORYUPDATE   = "Subversion.RepositoryUpdate";
static const char * const CMD_ID_REPOSITORYDIFF     = "Subversion.RepositoryDiff";
static const char * const CMD_ID_REPOSITORYSTATUS   = "Subversion.RepositoryStatus";
static const char * const CMD_ID_UPDATE             = "Subversion.Update";
static const char * const CMD_ID_COMMIT_PROJECT     = "Subversion.CommitProject";
static const char * const CMD_ID_DESCRIBE           = "Subversion.Describe";

static const char *nonInteractiveOptionC = "--non-interactive";



static const VCSBase::VCSBaseEditorParameters editorParameters[] = {
{
    VCSBase::RegularCommandOutput,
    "Subversion Command Log Editor", // id
    QT_TRANSLATE_NOOP("VCS", "Subversion Command Log Editor"), // display name
    "Subversion Command Log Editor", // context
    "application/vnd.nokia.text.scs_svn_commandlog",
    "scslog"},
{   VCSBase::LogOutput,
    "Subversion File Log Editor",   // id
    QT_TRANSLATE_NOOP("VCS", "Subversion File Log Editor"),   // display_name
    "Subversion File Log Editor",   // context
    "application/vnd.nokia.text.scs_svn_filelog",
    "scsfilelog"},
{    VCSBase::AnnotateOutput,
    "Subversion Annotation Editor",  // id
    QT_TRANSLATE_NOOP("VCS", "Subversion Annotation Editor"),   // display_name
    "Subversion Annotation Editor",  // context
    "application/vnd.nokia.text.scs_svn_annotation",
    "scsannotate"},
{   VCSBase::DiffOutput,
    "Subversion Diff Editor",  // id
    QT_TRANSLATE_NOOP("VCS", "Subversion Diff Editor"),   // display_name
    "Subversion Diff Editor",  // context
    "text/x-patch","diff"}
};

// Utility to find a parameter set by type
static inline const VCSBase::VCSBaseEditorParameters *findType(int ie)
{
    const VCSBase::EditorContentType et = static_cast<VCSBase::EditorContentType>(ie);
    return  VCSBase::VCSBaseEditorWidget::findType(editorParameters, sizeof(editorParameters)/sizeof(VCSBase::VCSBaseEditorParameters), et);
}

static inline QString debugCodec(const QTextCodec *c)
{
    return c ? QString::fromAscii(c->name()) : QString::fromAscii("Null codec");
}

Core::IEditor* locateEditor(const char *property, const QString &entry)
{
    foreach (Core::IEditor *ed, Core::EditorManager::instance()->openedEditors())
        if (ed->property(property).toString() == entry)
            return ed;
    return 0;
}

// Parse "svn status" output for added/modified/deleted files
// "M<7blanks>file"
typedef QList<SubversionSubmitEditor::StatusFilePair> StatusList;

StatusList parseStatusOutput(const QString &output)
{
    StatusList changeSet;
    const QString newLine = QString(QLatin1Char('\n'));
    const QStringList list = output.split(newLine, QString::SkipEmptyParts);
    foreach (const QString &l, list) {
        const QString line =l.trimmed();
        if (line.size() > 8) {
            const QChar state = line.at(0);
            if (state == QLatin1Char('A') || state == QLatin1Char('D') || state == QLatin1Char('M')) {
                const QString fileName = line.mid(7); // Column 8 starting from svn 1.6
                changeSet.push_back(SubversionSubmitEditor::StatusFilePair(QString(state), fileName.trimmed()));
            }

        }
    }
    return changeSet;
}

// Return a list of names for the internal svn directories
static inline QStringList svnDirectories()
{
    QStringList rc(QLatin1String(".svn"));
#ifdef Q_OS_WIN
    // Option on Windows systems to avoid hassle with some IDEs
    rc.push_back(QLatin1String("_svn"));
#endif
    return rc;
}

// ------------- SubversionPlugin
SubversionPlugin *SubversionPlugin::m_subversionPluginInstance = 0;

SubversionPlugin::SubversionPlugin() :
    VCSBase::VCSBasePlugin(QLatin1String(Subversion::Constants::SUBVERSIONCOMMITEDITOR_ID)),
    m_svnDirectories(svnDirectories()),
    m_commandLocator(0),
    m_addAction(0),
    m_deleteAction(0),
    m_revertAction(0),
    m_diffProjectAction(0),
    m_diffCurrentAction(0),
    m_logProjectAction(0),
    m_logRepositoryAction(0),
    m_commitAllAction(0),
    m_revertRepositoryAction(0),
    m_diffRepositoryAction(0),
    m_statusRepositoryAction(0),
    m_updateRepositoryAction(0),
    m_commitCurrentAction(0),
    m_filelogCurrentAction(0),
    m_annotateCurrentAction(0),
    m_statusProjectAction(0),
    m_updateProjectAction(0),
    m_commitProjectAction(0),
    m_describeAction(0),
    m_submitCurrentLogAction(0),
    m_submitDiffAction(0),
    m_submitUndoAction(0),
    m_submitRedoAction(0),
    m_menuAction(0),
    m_submitActionTriggered(false)
{
}

SubversionPlugin::~SubversionPlugin()
{
    cleanCommitMessageFile();
}

void SubversionPlugin::cleanCommitMessageFile()
{
    if (!m_commitMessageFileName.isEmpty()) {
        QFile::remove(m_commitMessageFileName);
        m_commitMessageFileName.clear();
        m_commitRepository.clear();
    }
}

bool SubversionPlugin::isCommitEditorOpen() const
{
    return !m_commitMessageFileName.isEmpty();
}

static const VCSBase::VCSBaseSubmitEditorParameters submitParameters = {
    Subversion::Constants::SUBVERSION_SUBMIT_MIMETYPE,
    Subversion::Constants::SUBVERSIONCOMMITEDITOR_ID,
    Subversion::Constants::SUBVERSIONCOMMITEDITOR_DISPLAY_NAME,
    Subversion::Constants::SUBVERSIONCOMMITEDITOR
};

static inline Core::Command *createSeparator(QObject *parent,
                                             Core::ActionManager *ami,
                                             const char*id,
                                             const Core::Context &globalcontext)
{
    QAction *tmpaction = new QAction(parent);
    tmpaction->setSeparator(true);
    return ami->registerAction(tmpaction, id, globalcontext);
}

bool SubversionPlugin::initialize(const QStringList & /*arguments */, QString *errorMessage)
{
    typedef VCSBase::VCSSubmitEditorFactory<SubversionSubmitEditor> SubversionSubmitEditorFactory;
    typedef VCSBase::VCSEditorFactory<SubversionEditor> SubversionEditorFactory;
    using namespace Constants;

    using namespace Core::Constants;
    using namespace ExtensionSystem;

    VCSBase::VCSBasePlugin::initialize(new SubversionControl(this));

    m_subversionPluginInstance = this;
    Core::ICore *core = Core::ICore::instance();

    if (!core->mimeDatabase()->addMimeTypes(QLatin1String(":/trolltech.subversion/Subversion.mimetypes.xml"), errorMessage))
        return false;

    if (QSettings *settings = core->settings())
        m_settings.fromSettings(settings);

    addAutoReleasedObject(new SettingsPage);

    addAutoReleasedObject(new SubversionSubmitEditorFactory(&submitParameters));

    static const char *describeSlot = SLOT(describe(QString,QString));
    const int editorCount = sizeof(editorParameters)/sizeof(VCSBase::VCSBaseEditorParameters);
    for (int i = 0; i < editorCount; i++)
        addAutoReleasedObject(new SubversionEditorFactory(editorParameters + i, this, describeSlot));

    addAutoReleasedObject(new CheckoutWizard);

    const QString description = QLatin1String("Subversion");
    const QString prefix = QLatin1String("svn");
    m_commandLocator = new Locator::CommandLocator(description, prefix, prefix);
    addAutoReleasedObject(m_commandLocator);

    //register actions
    Core::ActionManager *ami = core->actionManager();
    Core::ActionContainer *toolsContainer = ami->actionContainer(M_TOOLS);

    Core::ActionContainer *subversionMenu =
        ami->createMenu(Core::Id(CMD_ID_SUBVERSION_MENU));
    subversionMenu->menu()->setTitle(tr("&Subversion"));
    toolsContainer->addMenu(subversionMenu);
    m_menuAction = subversionMenu->menu()->menuAction();
    Core::Context globalcontext(C_GLOBAL);
    Core::Command *command;

    m_diffCurrentAction = new Utils::ParameterAction(tr("Diff Current File"), tr("Diff \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = ami->registerAction(m_diffCurrentAction,
        CMD_ID_DIFF_CURRENT, globalcontext);
    command->setAttribute(Core::Command::CA_UpdateText);
    command->setDefaultKeySequence(QKeySequence(tr("Alt+S,Alt+D")));
    connect(m_diffCurrentAction, SIGNAL(triggered()), this, SLOT(diffCurrentFile()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_filelogCurrentAction = new Utils::ParameterAction(tr("Filelog Current File"), tr("Filelog \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = ami->registerAction(m_filelogCurrentAction,
        CMD_ID_FILELOG_CURRENT, globalcontext);
    command->setAttribute(Core::Command::CA_UpdateText);
    connect(m_filelogCurrentAction, SIGNAL(triggered()), this,
        SLOT(filelogCurrentFile()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_annotateCurrentAction = new Utils::ParameterAction(tr("Annotate Current File"), tr("Annotate \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = ami->registerAction(m_annotateCurrentAction,
        CMD_ID_ANNOTATE_CURRENT, globalcontext);
    command->setAttribute(Core::Command::CA_UpdateText);
    connect(m_annotateCurrentAction, SIGNAL(triggered()), this,
        SLOT(annotateCurrentFile()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    subversionMenu->addAction(createSeparator(this, ami, CMD_ID_SEPARATOR0, globalcontext));

    m_addAction = new Utils::ParameterAction(tr("Add"), tr("Add \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = ami->registerAction(m_addAction, CMD_ID_ADD,
        globalcontext);
    command->setAttribute(Core::Command::CA_UpdateText);
    command->setDefaultKeySequence(QKeySequence(tr("Alt+S,Alt+A")));
    connect(m_addAction, SIGNAL(triggered()), this, SLOT(addCurrentFile()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_commitCurrentAction = new Utils::ParameterAction(tr("Commit Current File"), tr("Commit \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = ami->registerAction(m_commitCurrentAction,
        CMD_ID_COMMIT_CURRENT, globalcontext);
    command->setAttribute(Core::Command::CA_UpdateText);
    command->setDefaultKeySequence(QKeySequence(tr("Alt+S,Alt+C")));
    connect(m_commitCurrentAction, SIGNAL(triggered()), this, SLOT(startCommitCurrentFile()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_deleteAction = new Utils::ParameterAction(tr("Delete..."), tr("Delete \"%1\"..."), Utils::ParameterAction::EnabledWithParameter, this);
    command = ami->registerAction(m_deleteAction, CMD_ID_DELETE_FILE,
        globalcontext);
    command->setAttribute(Core::Command::CA_UpdateText);
    connect(m_deleteAction, SIGNAL(triggered()), this, SLOT(promptToDeleteCurrentFile()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_revertAction = new Utils::ParameterAction(tr("Revert..."), tr("Revert \"%1\"..."), Utils::ParameterAction::EnabledWithParameter, this);
    command = ami->registerAction(m_revertAction, CMD_ID_REVERT,
        globalcontext);
    command->setAttribute(Core::Command::CA_UpdateText);
    connect(m_revertAction, SIGNAL(triggered()), this, SLOT(revertCurrentFile()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    subversionMenu->addAction(createSeparator(this, ami, CMD_ID_SEPARATOR1, globalcontext));

    m_diffProjectAction = new Utils::ParameterAction(tr("Diff Project"), tr("Diff Project \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = ami->registerAction(m_diffProjectAction, CMD_ID_DIFF_PROJECT,
        globalcontext);
    command->setAttribute(Core::Command::CA_UpdateText);
    connect(m_diffProjectAction, SIGNAL(triggered()), this, SLOT(diffProject()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_statusProjectAction = new Utils::ParameterAction(tr("Project Status"), tr("Status of Project \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = ami->registerAction(m_statusProjectAction, CMD_ID_STATUS,
        globalcontext);
    command->setAttribute(Core::Command::CA_UpdateText);
    connect(m_statusProjectAction, SIGNAL(triggered()), this, SLOT(projectStatus()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_logProjectAction = new Utils::ParameterAction(tr("Log Project"), tr("Log Project \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = ami->registerAction(m_logProjectAction, CMD_ID_PROJECTLOG, globalcontext);
    command->setAttribute(Core::Command::CA_UpdateText);
    connect(m_logProjectAction, SIGNAL(triggered()), this, SLOT(logProject()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_updateProjectAction = new Utils::ParameterAction(tr("Update Project"), tr("Update Project \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = ami->registerAction(m_updateProjectAction, CMD_ID_UPDATE, globalcontext);
    connect(m_updateProjectAction, SIGNAL(triggered()), this, SLOT(updateProject()));
    command->setAttribute(Core::Command::CA_UpdateText);
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_commitProjectAction = new Utils::ParameterAction(tr("Commit Project"), tr("Commit Project \"%1\""), Utils::ParameterAction::EnabledWithParameter, this);
    command = ami->registerAction(m_commitProjectAction, CMD_ID_COMMIT_PROJECT, globalcontext);
    connect(m_commitProjectAction, SIGNAL(triggered()), this, SLOT(startCommitProject()));
    command->setAttribute(Core::Command::CA_UpdateText);
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    subversionMenu->addAction(createSeparator(this, ami, CMD_ID_SEPARATOR2, globalcontext));

    m_diffRepositoryAction = new QAction(tr("Diff Repository"), this);
    command = ami->registerAction(m_diffRepositoryAction, CMD_ID_REPOSITORYDIFF, globalcontext);
    connect(m_diffRepositoryAction, SIGNAL(triggered()), this, SLOT(diffRepository()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_statusRepositoryAction = new QAction(tr("Repository Status"), this);
    command = ami->registerAction(m_statusRepositoryAction, CMD_ID_REPOSITORYSTATUS, globalcontext);
    connect(m_statusRepositoryAction, SIGNAL(triggered()), this, SLOT(statusRepository()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_logRepositoryAction = new QAction(tr("Log Repository"), this);
    command = ami->registerAction(m_logRepositoryAction, CMD_ID_REPOSITORYLOG, globalcontext);
    connect(m_logRepositoryAction, SIGNAL(triggered()), this, SLOT(logRepository()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_updateRepositoryAction = new QAction(tr("Update Repository"), this);
    command = ami->registerAction(m_updateRepositoryAction, CMD_ID_REPOSITORYUPDATE, globalcontext);
    connect(m_updateRepositoryAction, SIGNAL(triggered()), this, SLOT(updateRepository()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_commitAllAction = new QAction(tr("Commit All Files"), this);
    command = ami->registerAction(m_commitAllAction, CMD_ID_COMMIT_ALL,
        globalcontext);
    connect(m_commitAllAction, SIGNAL(triggered()), this, SLOT(startCommitAll()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    m_describeAction = new QAction(tr("Describe..."), this);
    command = ami->registerAction(m_describeAction, CMD_ID_DESCRIBE, globalcontext);
    connect(m_describeAction, SIGNAL(triggered()), this, SLOT(slotDescribe()));
    subversionMenu->addAction(command);

    m_revertRepositoryAction = new QAction(tr("Revert Repository..."), this);
    command = ami->registerAction(m_revertRepositoryAction, CMD_ID_REVERT_ALL,
        globalcontext);
    connect(m_revertRepositoryAction, SIGNAL(triggered()), this, SLOT(revertAll()));
    subversionMenu->addAction(command);
    m_commandLocator->appendCommand(command);

    // Actions of the submit editor
    Core::Context svncommitcontext(Constants::SUBVERSIONCOMMITEDITOR);

    m_submitCurrentLogAction = new QAction(VCSBase::VCSBaseSubmitEditor::submitIcon(), tr("Commit"), this);
    command = ami->registerAction(m_submitCurrentLogAction, Constants::SUBMIT_CURRENT, svncommitcontext);
    command->setAttribute(Core::Command::CA_UpdateText);
    connect(m_submitCurrentLogAction, SIGNAL(triggered()), this, SLOT(submitCurrentLog()));

    m_submitDiffAction = new QAction(VCSBase::VCSBaseSubmitEditor::diffIcon(), tr("Diff Selected Files"), this);
    command = ami->registerAction(m_submitDiffAction , Constants::DIFF_SELECTED, svncommitcontext);

    m_submitUndoAction = new QAction(tr("&Undo"), this);
    command = ami->registerAction(m_submitUndoAction, Core::Constants::UNDO, svncommitcontext);

    m_submitRedoAction = new QAction(tr("&Redo"), this);
    command = ami->registerAction(m_submitRedoAction, Core::Constants::REDO, svncommitcontext);

    return true;
}

bool SubversionPlugin::submitEditorAboutToClose(VCSBase::VCSBaseSubmitEditor *submitEditor)
{
    if (!isCommitEditorOpen())
        return true;

    Core::IFile *fileIFace = submitEditor->file();
    const SubversionSubmitEditor *editor = qobject_cast<SubversionSubmitEditor *>(submitEditor);
    if (!fileIFace || !editor)
        return true;

    // Submit editor closing. Make it write out the commit message
    // and retrieve files
    const QFileInfo editorFile(fileIFace->fileName());
    const QFileInfo changeFile(m_commitMessageFileName);
    if (editorFile.absoluteFilePath() != changeFile.absoluteFilePath())
        return true; // Oops?!

    // Prompt user. Force a prompt unless submit was actually invoked (that
    // is, the editor was closed or shutdown).
    SubversionSettings newSettings = m_settings;
    const VCSBase::VCSBaseSubmitEditor::PromptSubmitResult answer =
            editor->promptSubmit(tr("Closing Subversion Editor"),
                                 tr("Do you want to commit the change?"),
                                 tr("The commit message check failed. Do you want to commit the change?"),
                                 &newSettings.promptToSubmit, !m_submitActionTriggered);
    m_submitActionTriggered = false;
    switch (answer) {
    case VCSBase::VCSBaseSubmitEditor::SubmitCanceled:
        return false; // Keep editing and change file
    case VCSBase::VCSBaseSubmitEditor::SubmitDiscarded:
        cleanCommitMessageFile();
        return true; // Cancel all
    default:
        break;
    }
    setSettings(newSettings); // in case someone turned prompting off
    const QStringList fileList = editor->checkedFiles();
    bool closeEditor = true;
    if (!fileList.empty()) {
        // get message & commit
        Core::ICore::instance()->fileManager()->blockFileChange(fileIFace);
        fileIFace->save();
        Core::ICore::instance()->fileManager()->unblockFileChange(fileIFace);
        closeEditor= commit(m_commitMessageFileName, fileList);
    }
    if (closeEditor)
        cleanCommitMessageFile();
    return closeEditor;
}

void SubversionPlugin::diffCommitFiles(const QStringList &files)
{
    svnDiff(m_commitRepository, files);
}

static inline void setDiffBaseDirectory(Core::IEditor *editor, const QString &db)
{
    if (VCSBase::VCSBaseEditorWidget *ve = qobject_cast<VCSBase::VCSBaseEditorWidget*>(editor->widget()))
        ve->setDiffBaseDirectory(db);
}

void SubversionPlugin::svnDiff(const QString &workingDir, const QStringList &files, QString diffname)
{
    if (Subversion::Constants::debug)
        qDebug() << Q_FUNC_INFO << files << diffname;
    const QString source = VCSBase::VCSBaseEditorWidget::getSource(workingDir, files);
    QTextCodec *codec = source.isEmpty() ? static_cast<QTextCodec *>(0) : VCSBase::VCSBaseEditorWidget::getCodec(source);

    if (files.count() == 1 && diffname.isEmpty())
        diffname = QFileInfo(files.front()).fileName();

    QStringList args(QLatin1String("diff"));
    args << files;

    const SubversionResponse response =
            runSvn(workingDir, args, m_settings.timeOutMS(), 0, codec);
    if (response.error)
        return;

    // diff of a single file? re-use an existing view if possible to support
    // the common usage pattern of continuously changing and diffing a file
    if (files.count() == 1) {
        // Show in the same editor if diff has been executed before
        if (Core::IEditor *editor = locateEditor("originalFileName", files.front())) {
            editor->createNew(response.stdOut);
            Core::EditorManager::instance()->activateEditor(editor, Core::EditorManager::ModeSwitch);
            setDiffBaseDirectory(editor, workingDir);
            return;
        }
    }
    const QString title = QString::fromLatin1("svn diff %1").arg(diffname);
    Core::IEditor *editor = showOutputInEditor(title, response.stdOut, VCSBase::DiffOutput, source, codec);
    setDiffBaseDirectory(editor, workingDir);
    if (files.count() == 1)
        editor->setProperty("originalFileName", files.front());
}

SubversionSubmitEditor *SubversionPlugin::openSubversionSubmitEditor(const QString &fileName)
{
    Core::IEditor *editor = Core::EditorManager::instance()->openEditor(fileName,
                                                                        QLatin1String(Constants::SUBVERSIONCOMMITEDITOR_ID),
                                                                        Core::EditorManager::ModeSwitch);
    SubversionSubmitEditor *submitEditor = qobject_cast<SubversionSubmitEditor*>(editor);
    QTC_ASSERT(submitEditor, /**/);
    submitEditor->registerActions(m_submitUndoAction, m_submitRedoAction, m_submitCurrentLogAction, m_submitDiffAction);
    connect(submitEditor, SIGNAL(diffSelectedFiles(QStringList)), this, SLOT(diffCommitFiles(QStringList)));
    submitEditor->setCheckScriptWorkingDirectory(m_commitRepository);
    return submitEditor;
}

void SubversionPlugin::updateActions(VCSBase::VCSBasePlugin::ActionState as)
{
    if (!enableMenuAction(as, m_menuAction)) {
        m_commandLocator->setEnabled(false);
        return;
    }
    const bool hasTopLevel = currentState().hasTopLevel();
    m_commandLocator->setEnabled(hasTopLevel);
    m_logRepositoryAction->setEnabled(hasTopLevel);

    const QString projectName = currentState().currentProjectName();
    m_diffProjectAction->setParameter(projectName);
    m_statusProjectAction->setParameter(projectName);
    m_updateProjectAction->setParameter(projectName);
    m_logProjectAction->setParameter(projectName);
    m_commitProjectAction->setParameter(projectName);

    const bool repoEnabled = currentState().hasTopLevel();
    m_commitAllAction->setEnabled(repoEnabled);
    m_describeAction->setEnabled(repoEnabled);
    m_revertRepositoryAction->setEnabled(repoEnabled);
    m_diffRepositoryAction->setEnabled(repoEnabled);
    m_statusRepositoryAction->setEnabled(repoEnabled);
    m_updateRepositoryAction->setEnabled(repoEnabled);

    const QString fileName = currentState().currentFileName();

    m_addAction->setParameter(fileName);
    m_deleteAction->setParameter(fileName);
    m_revertAction->setParameter(fileName);
    m_diffCurrentAction->setParameter(fileName);
    m_commitCurrentAction->setParameter(fileName);
    m_filelogCurrentAction->setParameter(fileName);
    m_annotateCurrentAction->setParameter(fileName);
}

void SubversionPlugin::addCurrentFile()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasFile(), return)
    vcsAdd(state.currentFileTopLevel(), state.relativeCurrentFile());
}

void SubversionPlugin::revertAll()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return)
    const QString title = tr("Revert repository");
    if (QMessageBox::warning(0, title, tr("Revert all pending changes to the repository?"),
                             QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
        return;
    // NoteL: Svn "revert ." doesn not work.
    QStringList args;
    args << QLatin1String("revert") << QLatin1String("--recursive") << state.topLevel();
    const SubversionResponse revertResponse =
            runSvn(state.topLevel(), args, m_settings.timeOutMS(),
                   SshPasswordPrompt|ShowStdOutInLogWindow);
    if (revertResponse.error) {
        QMessageBox::warning(0, title, tr("Revert failed: %1").arg(revertResponse.message), QMessageBox::Ok);
    } else {
        subVersionControl()->emitRepositoryChanged(state.topLevel());
    }
}

void SubversionPlugin::revertCurrentFile()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasFile(), return)

    QStringList args(QLatin1String("diff"));
    args.push_back(state.relativeCurrentFile());

    const SubversionResponse diffResponse =
            runSvn(state.currentFileTopLevel(), args, m_settings.timeOutMS(), 0);
    if (diffResponse.error)
        return;

    if (diffResponse.stdOut.isEmpty())
        return;
    if (QMessageBox::warning(0, QLatin1String("svn revert"), tr("The file has been changed. Do you want to revert it?"),
                             QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
        return;


    Core::FileChangeBlocker fcb(state.currentFile());

    // revert
    args.clear();
    args << QLatin1String("revert") << state.relativeCurrentFile();

    const SubversionResponse revertResponse =
            runSvn(state.currentFileTopLevel(), args, m_settings.timeOutMS(),
                   SshPasswordPrompt|ShowStdOutInLogWindow);

    if (!revertResponse.error) {
        subVersionControl()->emitFilesChanged(QStringList(state.currentFile()));
    }
}

void SubversionPlugin::diffProject()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasProject(), return)
    svnDiff(state.currentProjectTopLevel(), state.relativeCurrentProject(), state.currentProjectName());
}

void SubversionPlugin::diffCurrentFile()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasFile(), return)
    svnDiff(state.currentFileTopLevel(), QStringList(state.relativeCurrentFile()));
}

void SubversionPlugin::startCommitCurrentFile()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasFile(), return)
    startCommit(state.currentFileTopLevel(), QStringList(state.relativeCurrentFile()));
}

void SubversionPlugin::startCommitAll()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return);
    startCommit(state.topLevel());
}

void SubversionPlugin::startCommitProject()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasProject(), return);
    startCommit(state.currentProjectPath());
}

/* Start commit of files of a single repository by displaying
 * template and files in a submit editor. On closing, the real
 * commit will start. */
void SubversionPlugin::startCommit(const QString &workingDir, const QStringList &files)
{
    if (VCSBase::VCSBaseSubmitEditor::raiseSubmitEditor())
        return;
    if (isCommitEditorOpen()) {
        VCSBase::VCSBaseOutputWindow::instance()->appendWarning(tr("Another commit is currently being executed."));
        return;
    }

    QStringList args(QLatin1String("status"));
    args += files;

    const SubversionResponse response =
            runSvn(workingDir, args, m_settings.timeOutMS(), 0);
    if (response.error)
        return;

    // Get list of added/modified/deleted files
    const StatusList statusOutput = parseStatusOutput(response.stdOut);
    if (statusOutput.empty()) {
        VCSBase::VCSBaseOutputWindow::instance()->appendWarning(tr("There are no modified files."));
        return;
    }
    m_commitRepository = workingDir;
    // Create a new submit change file containing the submit template
    QTemporaryFile changeTmpFile;
    changeTmpFile.setAutoRemove(false);
    if (!changeTmpFile.open()) {
        VCSBase::VCSBaseOutputWindow::instance()->appendError(tr("Cannot create temporary file: %1").arg(changeTmpFile.errorString()));
        return;
    }
    m_commitMessageFileName = changeTmpFile.fileName();
    // TODO: Regitctrieve submit template from
    const QString submitTemplate;
    // Create a submit
    changeTmpFile.write(submitTemplate.toUtf8());
    changeTmpFile.flush();
    changeTmpFile.close();
    // Create a submit editor and set file list
    SubversionSubmitEditor *editor = openSubversionSubmitEditor(m_commitMessageFileName);
    editor->setStatusList(statusOutput);
}

bool SubversionPlugin::commit(const QString &messageFile,
                              const QStringList &subVersionFileList)
{
    if (Subversion::Constants::debug)
        qDebug() << Q_FUNC_INFO << messageFile << subVersionFileList;
    // Transform the status list which is sth
    // "[ADM]<blanks>file" into an args list. The files of the status log
    // can be relative or absolute depending on where the command was run.
    QStringList args = QStringList(QLatin1String("commit"));
    args << QLatin1String(nonInteractiveOptionC) << QLatin1String("--file") << messageFile;
    args.append(subVersionFileList);
    const SubversionResponse response =
            runSvn(m_commitRepository, args, m_settings.longTimeOutMS(),
                   SshPasswordPrompt|ShowStdOutInLogWindow);
    return !response.error ;
}

void SubversionPlugin::filelogCurrentFile()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasFile(), return)
   filelog(state.currentFileTopLevel(), QStringList(state.relativeCurrentFile()), true);
}

void SubversionPlugin::logProject()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasProject(), return)
    filelog(state.currentProjectTopLevel(), state.relativeCurrentProject());
}

void SubversionPlugin::logRepository()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return)
    filelog(state.topLevel());
}

void SubversionPlugin::diffRepository()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return)
    svnDiff(state.topLevel(), QStringList());
}

void SubversionPlugin::statusRepository()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return)
    svnStatus(state.topLevel());
}

void SubversionPlugin::updateRepository()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return)
    svnUpdate(state.topLevel());
}

void SubversionPlugin::svnStatus(const QString &workingDir, const QStringList &relativePaths)
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return)
    QStringList args(QLatin1String("status"));
    if (!relativePaths.isEmpty())
        args.append(relativePaths);
    VCSBase::VCSBaseOutputWindow *outwin = VCSBase::VCSBaseOutputWindow::instance();
    outwin->setRepository(workingDir);
    runSvn(workingDir, args, m_settings.timeOutMS(),
           ShowStdOutInLogWindow|ShowSuccessMessage);
    outwin->clearRepository();
}

void SubversionPlugin::filelog(const QString &workingDir,
                               const QStringList &files,
                               bool enableAnnotationContextMenu)
{
    // no need for temp file
    QStringList args(QLatin1String("log"));
    if (m_settings.logCount > 0)
        args << QLatin1String("-l") << QString::number(m_settings.logCount);
    foreach(const QString &file, files)
        args.append(QDir::toNativeSeparators(file));

    // subversion stores log in UTF-8 and returns it back in user system locale.
    // So we do not need to encode it.
    const SubversionResponse response =
            runSvn(workingDir, args, m_settings.timeOutMS(),
                   SshPasswordPrompt, 0/*codec*/);
    if (response.error)
        return;

    // Re-use an existing view if possible to support
    // the common usage pattern of continuously changing and diffing a file

    const QString id = VCSBase::VCSBaseEditorWidget::getTitleId(workingDir, files);
    if (Core::IEditor *editor = locateEditor("logFileName", id)) {
        editor->createNew(response.stdOut);
        Core::EditorManager::instance()->activateEditor(editor, Core::EditorManager::ModeSwitch);
    } else {
        const QString title = QString::fromLatin1("svn log %1").arg(id);
        const QString source = VCSBase::VCSBaseEditorWidget::getSource(workingDir, files);
        Core::IEditor *newEditor = showOutputInEditor(title, response.stdOut, VCSBase::LogOutput, source, /*codec*/0);
        newEditor->setProperty("logFileName", id);
        if (enableAnnotationContextMenu)
            VCSBase::VCSBaseEditorWidget::getVcsBaseEditor(newEditor)->setFileLogAnnotateEnabled(true);
    }
}

void SubversionPlugin::updateProject()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasProject(), return);
    svnUpdate(state.currentProjectTopLevel(), state.relativeCurrentProject());
}

void SubversionPlugin::svnUpdate(const QString &workingDir, const QStringList &relativePaths)
{
    QStringList args(QLatin1String("update"));
    args.push_back(QLatin1String(nonInteractiveOptionC));
    if (!relativePaths.isEmpty())
        args.append(relativePaths);
        const SubversionResponse response =
                runSvn(workingDir, args, m_settings.longTimeOutMS(),
                       SshPasswordPrompt|ShowStdOutInLogWindow);
    if (!response.error)
        subVersionControl()->emitRepositoryChanged(workingDir);
}

void SubversionPlugin::annotateCurrentFile()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasFile(), return);
    vcsAnnotate(state.currentFileTopLevel(), state.relativeCurrentFile());
}

void SubversionPlugin::annotateVersion(const QString &file,
                                       const QString &revision,
                                       int lineNr)
{
    const QFileInfo fi(file);
    vcsAnnotate(fi.absolutePath(), fi.fileName(), revision, lineNr);
}

void SubversionPlugin::vcsAnnotate(const QString &workingDir, const QString &file,
                                const QString &revision /* = QString() */,
                                int lineNumber /* = -1 */)
{
    const QString source = VCSBase::VCSBaseEditorWidget::getSource(workingDir, file);
    QTextCodec *codec = VCSBase::VCSBaseEditorWidget::getCodec(source);

    QStringList args(QLatin1String("annotate"));
    if (m_settings.spaceIgnorantAnnotation)
        args << QLatin1String("-x") << QLatin1String("-uw");
    if (!revision.isEmpty())
        args << QLatin1String("-r") << revision;
    args.push_back(QLatin1String("-v"));
    args.append(QDir::toNativeSeparators(file));

    const SubversionResponse response =
            runSvn(workingDir, args, m_settings.timeOutMS(),
                   SshPasswordPrompt|ForceCLocale, codec);
    if (response.error)
        return;

    // Re-use an existing view if possible to support
    // the common usage pattern of continuously changing and diffing a file
    if (lineNumber <= 0)
        lineNumber = VCSBase::VCSBaseEditorWidget::lineNumberOfCurrentEditor(source);
    // Determine id
    const QString id = VCSBase::VCSBaseEditorWidget::getTitleId(workingDir, QStringList(file), revision);

    if (Core::IEditor *editor = locateEditor("annotateFileName", id)) {
        editor->createNew(response.stdOut);
        VCSBase::VCSBaseEditorWidget::gotoLineOfEditor(editor, lineNumber);
        Core::EditorManager::instance()->activateEditor(editor, Core::EditorManager::ModeSwitch);
    } else {
        const QString title = QString::fromLatin1("svn annotate %1").arg(id);
        Core::IEditor *newEditor = showOutputInEditor(title, response.stdOut, VCSBase::AnnotateOutput, source, codec);
        newEditor->setProperty("annotateFileName", id);
        VCSBase::VCSBaseEditorWidget::gotoLineOfEditor(newEditor, lineNumber);
    }
}

void SubversionPlugin::projectStatus()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasProject(), return);
    svnStatus(state.currentFileTopLevel(), state.relativeCurrentProject());
}

void SubversionPlugin::describe(const QString &source, const QString &changeNr)
{
    // To describe a complete change, find the top level and then do
    //svn diff -r 472958:472959 <top level>
    const QFileInfo fi(source);
    QString topLevel;
    const bool manages = managesDirectory(fi.isDir() ? source : fi.absolutePath(), &topLevel);
    if (!manages || topLevel.isEmpty())
        return;
    if (Subversion::Constants::debug)
        qDebug() << Q_FUNC_INFO << source << topLevel << changeNr;
    // Number must be > 1
    bool ok;
    const int number = changeNr.toInt(&ok);
    if (!ok || number < 2)
        return;
    // Run log to obtain message (local utf8)
    QString description;
    QStringList args(QLatin1String("log"));
    args.push_back(QLatin1String("-r"));
    args.push_back(changeNr);
    const SubversionResponse logResponse =
            runSvn(topLevel, args, m_settings.timeOutMS(), SshPasswordPrompt);
    if (logResponse.error)
        return;
    description = logResponse.stdOut;

    // Run diff (encoding via source codec)
    args.clear();
    args.push_back(QLatin1String("diff"));
    args.push_back(QLatin1String("-r"));
    QString diffArg;
    QTextStream(&diffArg) << (number - 1) << ':' << number;
    args.push_back(diffArg);

    QTextCodec *codec = VCSBase::VCSBaseEditorWidget::getCodec(source);
    const SubversionResponse response =
            runSvn(topLevel, args, m_settings.timeOutMS(),
                   SshPasswordPrompt, codec);
    if (response.error)
        return;
    description += response.stdOut;

    // Re-use an existing view if possible to support
    // the common usage pattern of continuously changing and diffing a file
    const QString id = diffArg + source;
    if (Core::IEditor *editor = locateEditor("describeChange", id)) {
        editor->createNew(description);
        Core::EditorManager::instance()->activateEditor(editor, Core::EditorManager::ModeSwitch);
    } else {
        const QString title = QString::fromLatin1("svn describe %1#%2").arg(fi.fileName(), changeNr);
        Core::IEditor *newEditor = showOutputInEditor(title, description, VCSBase::DiffOutput, source, codec);
        newEditor->setProperty("describeChange", id);
    }
}

void SubversionPlugin::slotDescribe()
{
    const VCSBase::VCSBasePluginState state = currentState();
    QTC_ASSERT(state.hasTopLevel(), return);

    QInputDialog inputDialog(Core::ICore::instance()->mainWindow());
    inputDialog.setWindowFlags(inputDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    inputDialog.setInputMode(QInputDialog::IntInput);
    inputDialog.setIntRange(2, INT_MAX);
    inputDialog.setWindowTitle(tr("Describe"));
    inputDialog.setLabelText(tr("Revision number:"));
    if (inputDialog.exec() != QDialog::Accepted)
        return;

    const int revision = inputDialog.intValue();
    describe(state.topLevel(), QString::number(revision));
}

void SubversionPlugin::submitCurrentLog()
{
    m_submitActionTriggered = true;
    Core::EditorManager::instance()->closeEditors(QList<Core::IEditor*>()
        << Core::EditorManager::instance()->currentEditor());
}

SubversionResponse
        SubversionPlugin::runSvn(const QString &workingDir,
                                 const QStringList &arguments,
                                 int timeOut,
                                 unsigned flags,
                                 QTextCodec *outputCodec)
{

    return m_settings.hasAuthentication() ?
            runSvn(workingDir, m_settings.user, m_settings.password,
                   arguments, timeOut, flags, outputCodec) :
            runSvn(workingDir, QString(), QString(),
                   arguments, timeOut, flags, outputCodec);
}

// Add authorization options to the command line arguments.
// SVN pre 1.5 does not accept "--userName" for "add", which is most likely
// an oversight. As no password is needed for the option, generally omit it.
QStringList SubversionPlugin::addAuthenticationOptions(const QStringList &args,
                                                      const QString &userName, const QString &password)
{
    if (userName.isEmpty())
        return args;
    if (!args.empty() && args.front() == QLatin1String("add"))
        return args;
    QStringList rc;
    rc.push_back(QLatin1String("--username"));
    rc.push_back(userName);
    if (!password.isEmpty()) {
        rc.push_back(QLatin1String("--password"));
        rc.push_back(password);
    }
    rc.append(args);
    return rc;
}

SubversionResponse SubversionPlugin::runSvn(const QString &workingDir,
                          const QString &userName, const QString &password,
                          const QStringList &arguments, int timeOut,
                          unsigned flags, QTextCodec *outputCodec)
{
    const QString executable = m_settings.svnCommand;
    SubversionResponse response;
    if (executable.isEmpty()) {
        response.error = true;
        response.message =tr("No subversion executable specified!");
        return response;
    }

    const QStringList completeArguments = SubversionPlugin::addAuthenticationOptions(arguments, userName, password);
    const Utils::SynchronousProcessResponse sp_resp =
            VCSBase::VCSBasePlugin::runVCS(workingDir, executable,
                                           completeArguments, timeOut, flags, outputCodec);

    response.error = sp_resp.result != Utils::SynchronousProcessResponse::Finished;
    if (response.error)
        response.message = sp_resp.exitMessage(executable, timeOut);
    response.stdErr = sp_resp.stdErr;
    response.stdOut = sp_resp.stdOut;
    return response;
}

Core::IEditor * SubversionPlugin::showOutputInEditor(const QString& title, const QString &output,
                                                     int editorType, const QString &source,
                                                     QTextCodec *codec)
{
    const VCSBase::VCSBaseEditorParameters *params = findType(editorType);
    QTC_ASSERT(params, return 0);
    const QString id = params->id;
    if (Subversion::Constants::debug)
        qDebug() << "SubversionPlugin::showOutputInEditor" << title << id <<  "Size= " << output.size() <<  " Type=" << editorType << debugCodec(codec);
    QString s = title;
    Core::IEditor *editor = Core::EditorManager::instance()->openEditorWithContents(id, &s, output);
    connect(editor, SIGNAL(annotateRevisionRequested(QString,QString,int)),
            this, SLOT(annotateVersion(QString,QString,int)));
    SubversionEditor *e = qobject_cast<SubversionEditor*>(editor->widget());
    if (!e)
        return 0;
    e->setForceReadOnly(true);
    s.replace(QLatin1Char(' '), QLatin1Char('_'));
    e->setSuggestedFileName(s);
    if (!source.isEmpty())
        e->setSource(source);
    if (codec)
        e->setCodec(codec);
    Core::IEditor *ie = e->editor();
    Core::EditorManager::instance()->activateEditor(ie, Core::EditorManager::ModeSwitch);
    return ie;
}

SubversionSettings SubversionPlugin::settings() const
{
    return m_settings;
}

void SubversionPlugin::setSettings(const SubversionSettings &s)
{
    if (s != m_settings) {
        m_settings = s;
        if (QSettings *settings = Core::ICore::instance()->settings())
            m_settings.toSettings(settings);
    }
}

SubversionPlugin *SubversionPlugin::subversionPluginInstance()
{
    QTC_ASSERT(m_subversionPluginInstance, return m_subversionPluginInstance);
    return m_subversionPluginInstance;
}

bool SubversionPlugin::vcsAdd(const QString &workingDir, const QString &rawFileName)
{
#ifdef Q_OS_MAC // See below.
    return vcsAdd14(workingDir, rawFileName);
#else
    return vcsAdd15(workingDir, rawFileName);
#endif
}

// Post 1.4 add: Use "--parents" to add directories
bool SubversionPlugin::vcsAdd15(const QString &workingDir, const QString &rawFileName)
{
    const QString file = QDir::toNativeSeparators(rawFileName);
    QStringList args;
    args << QLatin1String("add") << QLatin1String("--parents") << file;
    const SubversionResponse response =
            runSvn(workingDir, args, m_settings.timeOutMS(),
                   SshPasswordPrompt|ShowStdOutInLogWindow);
    return !response.error;
}

// Pre 1.5 add: Add directories in a loop. To be deprecated
// once Mac ships newer svn-versions
bool SubversionPlugin::vcsAdd14(const QString &workingDir, const QString &rawFileName)
{
    const QChar slash = QLatin1Char('/');
    const QStringList relativePath = rawFileName.split(slash);
    // Add directories (dir1/dir2/file.cpp) in a loop.
    if (relativePath.size() > 1) {
        QString path;
        const int lastDir = relativePath.size() - 1;
        for (int p = 0; p < lastDir; p++) {
            if (!path.isEmpty())
                path += slash;
            path += relativePath.at(p);
            if (!checkSVNSubDir(QDir(path))) {
                QStringList addDirArgs;
                addDirArgs << QLatin1String("add") << QLatin1String("--non-recursive") << QDir::toNativeSeparators(path);
                const SubversionResponse addDirResponse =
                        runSvn(workingDir, addDirArgs, m_settings.timeOutMS(),
                               SshPasswordPrompt|ShowStdOutInLogWindow);
                if (addDirResponse.error)
                    return false;
            }
        }
    }
    // Add file
    QStringList args;
    args << QLatin1String("add") << QDir::toNativeSeparators(rawFileName);
    const SubversionResponse response =
            runSvn(workingDir, args, m_settings.timeOutMS(),
                   SshPasswordPrompt|ShowStdOutInLogWindow);
    return !response.error;
}

bool SubversionPlugin::vcsDelete(const QString &workingDir, const QString &rawFileName)
{
    const QString file = QDir::toNativeSeparators(rawFileName);

    QStringList args(QLatin1String("delete"));
    args.push_back(file);

    const SubversionResponse response =
            runSvn(workingDir, args, m_settings.timeOutMS(),
                   SshPasswordPrompt|ShowStdOutInLogWindow);
    return !response.error;
}

bool SubversionPlugin::vcsMove(const QString &workingDir, const QString &from, const QString &to)
{
    QStringList args(QLatin1String("move"));
    args << QDir::toNativeSeparators(from) << QDir::toNativeSeparators(to);
    const SubversionResponse response =
            runSvn(workingDir, args, m_settings.timeOutMS(),
                   SshPasswordPrompt|ShowStdOutInLogWindow|FullySynchronously);
    return !response.error;
}

bool SubversionPlugin::vcsCheckout(const QString &directory, const QByteArray &url)
{
    QUrl tempUrl;
    tempUrl.setEncodedUrl(url);
    QString username = tempUrl.userName();
    QString password = tempUrl.password();
    QStringList args = QStringList(QLatin1String("checkout"));
    args << QLatin1String(nonInteractiveOptionC) ;

    if(!username.isEmpty() && !password.isEmpty())
    {
        // If url contains username and password we have to use separate username and password
        // arguments instead of passing those in the url. Otherwise the subversion 'non-interactive'
        // authentication will always fail (if the username and password data are not stored locally),
        // if for example we are logging into a new host for the first time using svn. There seems to
        // be a bug in subversion, so this might get fixed in the future.
        tempUrl.setUserInfo("");
        args << tempUrl.toEncoded() << directory;
        const SubversionResponse response = runSvn(directory, username, password, args,
                                                   m_settings.longTimeOutMS(),
                                                   VCSBase::VCSBasePlugin::SshPasswordPrompt);
        return !response.error;
    } else {
        args << url << directory;
        const SubversionResponse response = runSvn(directory, args, m_settings.longTimeOutMS(),
                                                   VCSBase::VCSBasePlugin::SshPasswordPrompt);
        return !response.error;
    }
}

QString SubversionPlugin::vcsGetRepositoryURL(const QString &directory)
{
    QXmlStreamReader xml;
    QStringList args = QStringList(QLatin1String("info"));
    args << QLatin1String("--xml");

    const SubversionResponse response = runSvn(directory, args, m_settings.longTimeOutMS(), SuppressCommandLogging);
    xml.addData(response.stdOut);

    bool repo = false;
    bool root = false;

    while(!xml.atEnd() && !xml.hasError()) {
        switch(xml.readNext()) {
        case QXmlStreamReader::StartDocument:
            break;
        case QXmlStreamReader::StartElement:
            if(xml.name() == QLatin1String("repository"))
                repo = true;
            else if(repo && xml.name() == QLatin1String("root"))
                root = true;
            break;
        case QXmlStreamReader::EndElement:
            if(xml.name() == QLatin1String("repository"))
                repo = false;
            else if(repo && xml.name() == QLatin1String("root"))
                root = false;
            break;
        case QXmlStreamReader::Characters:
            if (repo && root)
                return xml.text().toString();
            break;
        default:
            break;
        }
    }
    return QString();
}

/* Subversion has ".svn" directory in each directory
 * it manages. The top level is the first directory
 * under the directory that does not have a  ".svn". */
bool SubversionPlugin::managesDirectory(const QString &directory, QString *topLevel /* = 0 */) const
{
    const QDir dir(directory);
    if (topLevel)
        topLevel->clear();
    bool manages = false;
    do {
        if (!dir.exists() || !checkSVNSubDir(dir))
            break;
        manages = true;
        if (!topLevel)
            break;
        /* Recursing up, the top level is a child of the first directory that does
         * not have a  ".svn" directory. The starting directory must be a managed
         * one. Go up and try to find the first unmanaged parent dir. */
        QDir lastDirectory = dir;
        for (QDir parentDir = lastDirectory; parentDir.cdUp() ; lastDirectory = parentDir) {
            if (!checkSVNSubDir(parentDir)) {
                *topLevel = lastDirectory.absolutePath();
                break;
            }
        }
    } while (false);
    if (Subversion::Constants::debug) {
        QDebug nsp = qDebug().nospace();
        nsp << "SubversionPlugin::managesDirectory" << directory << manages;
        if (topLevel)
            nsp << *topLevel;
    }
    return manages;
}

// Check whether SVN management subdirs exist.
bool SubversionPlugin::checkSVNSubDir(const QDir &directory) const
{
    const int dirCount = m_svnDirectories.size();
    for (int i = 0; i < dirCount; i++) {
        const QString svnDir = directory.absoluteFilePath(m_svnDirectories.at(i));
        if (QFileInfo(svnDir).isDir())
            return true;
    }
    return false;
}

SubversionControl *SubversionPlugin::subVersionControl() const
{
    return static_cast<SubversionControl *>(versionControl());
}

Q_EXPORT_PLUGIN(SubversionPlugin)
