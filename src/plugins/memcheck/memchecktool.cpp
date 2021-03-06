/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Author: Nicolas Arnaud-Cormos, KDAB (nicolas.arnaud-cormos@kdab.com)
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

#include "memchecktool.h"
#include "memcheckengine.h"
#include "memcheckerrorview.h"
#include "memchecksettings.h"

#include <analyzerbase/analyzermanager.h>
#include <analyzerbase/analyzerconstants.h>

#include <valgrind/xmlprotocol/errorlistmodel.h>
#include <valgrind/xmlprotocol/stackmodel.h>
#include <valgrind/xmlprotocol/error.h>
#include <valgrind/xmlprotocol/frame.h>
#include <valgrind/xmlprotocol/stack.h>
#include <valgrind/xmlprotocol/suppression.h>

#include <valgrindtoolbase/valgrindsettings.h>

#include <extensionsystem/iplugin.h>
#include <extensionsystem/pluginmanager.h>

#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/project.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/target.h>
#include <projectexplorer/session.h>
#include <projectexplorer/buildconfiguration.h>

#include <coreplugin/coreconstants.h>
#include <coreplugin/icore.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/uniqueidmanager.h>

#include <texteditor/basetexteditor.h>

#include <utils/fancymainwindow.h>
#include <utils/styledbar.h>
#include <utils/qtcassert.h>

#include <QtCore/QString>
#include <QtCore/QLatin1String>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>
#include <QtCore/QDir>

#include <QtGui/QDockWidget>
#include <QtGui/QHBoxLayout>
#include <QtGui/QComboBox>
#include <QtGui/QLabel>
#include <QtGui/QSpinBox>
#include <QtGui/QAction>
#include <QtGui/QMenu>
#include <QtGui/QMessageBox>
#include <QtGui/QToolButton>
#include <QtGui/QCheckBox>
#include <utils/stylehelper.h>

using namespace Analyzer;
using namespace Analyzer::Internal;
using namespace Valgrind::XmlProtocol;

MemcheckErrorFilterProxyModel::MemcheckErrorFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent),
      m_filterExternalIssues(false)
{
}

void MemcheckErrorFilterProxyModel::setAcceptedKinds(const QList<int> &acceptedKinds)
{
    if (m_acceptedKinds != acceptedKinds) {
        m_acceptedKinds = acceptedKinds;
        invalidate();
    }
}

void MemcheckErrorFilterProxyModel::setFilterExternalIssues(bool filter)
{
    if (m_filterExternalIssues != filter) {
        m_filterExternalIssues = filter;
        invalidate();
    }
}

bool MemcheckErrorFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    // we only deal with toplevel items
    if (sourceParent.isValid())
        return true;

    // because toplevel items have no parent, we can't use sourceParent to find them. we just use
    // sourceParent as an invalid index, telling the model that the index we're looking for has no
    // parent.
    QAbstractItemModel *model = sourceModel();
    QModelIndex sourceIndex = model->index(sourceRow, filterKeyColumn(), sourceParent);
    if (!sourceIndex.isValid())
        return true;

    const Error error = sourceIndex.data(ErrorListModel::ErrorRole).value<Error>();

    // filter on kind
    if (!m_acceptedKinds.contains(error.kind()))
        return false;

    // filter non-project stuff
    if (m_filterExternalIssues && !error.stacks().isEmpty()) {
        // ALGORITHM: look at last five stack frames, if none of these is inside any open projects,
        // assume this error was created by an external library
        ProjectExplorer::SessionManager *session
            = ProjectExplorer::ProjectExplorerPlugin::instance()->session();
        QSet<QString> validFolders;
        foreach(ProjectExplorer::Project *project, session->projects()) {
            validFolders << project->projectDirectory();
            foreach(ProjectExplorer::Target *target, project->targets()) {
                foreach(ProjectExplorer::BuildConfiguration *config, target->buildConfigurations()) {
                    validFolders << config->buildDirectory();
                }
            }
        }

        const QVector< Frame > frames = error.stacks().first().frames();

        const int framesToLookAt = qMin(6, frames.size());

        bool inProject = false;
        for ( int i = 0; i < framesToLookAt; ++i ) {
            const Frame &frame = frames.at(i);
            foreach(const QString &folder, validFolders) {
                if (frame.object().startsWith(folder)) {
                    inProject = true;
                    break;
                }
            }
        }
        if (!inProject)
            return false;
    }

    return true;
}

MemcheckTool::MemcheckTool(QObject *parent) :
    Analyzer::IAnalyzerTool(parent),
    m_settings(0),
    m_errorModel(0),
    m_errorProxyModel(0),
    m_errorView(0),
    m_prevAction(0),
    m_nextAction(0),
    m_clearAction(0),
    m_filterProjectAction(0)
{
    connect(ProjectExplorer::ProjectExplorerPlugin::instance(),
            SIGNAL(updateRunActions()), SLOT(maybeActiveRunConfigurationChanged()));
}

void MemcheckTool::settingsDestroyed(QObject *settings)
{
    Q_ASSERT(m_settings == settings);
    m_settings = AnalyzerGlobalSettings::instance();
}

void MemcheckTool::maybeActiveRunConfigurationChanged()
{
    AnalyzerSettings *settings = 0;
    ProjectExplorer::ProjectExplorerPlugin *pe = ProjectExplorer::ProjectExplorerPlugin::instance();
    if (ProjectExplorer::Project *project = pe->startupProject()) {
        if (ProjectExplorer::Target *target = project->activeTarget()) {
            if (ProjectExplorer::RunConfiguration *rc = target->activeRunConfiguration()) {
                settings = rc->extraAspect<AnalyzerProjectSettings>();
            }
        }
    }

    if (!settings) // fallback to global settings
        settings = AnalyzerGlobalSettings::instance();

    if (m_settings == settings)
        return;

    // disconnect old settings class if any
    if (m_settings) {
        m_settings->disconnect(this);
        m_settings->disconnect(m_errorProxyModel);
    }

    // now make the new settings current, update and connect input widgets
    m_settings = settings;
    QTC_ASSERT(m_settings, return);

    connect(m_settings, SIGNAL(destroyed(QObject *)), SLOT(settingsDestroyed(QObject *)));

    AbstractMemcheckSettings *memcheckSettings = m_settings->subConfig<AbstractMemcheckSettings>();
    QTC_ASSERT(memcheckSettings, return);

    foreach (QAction *action, m_errorFilterActions) {
        bool contained = true;
        foreach (const QVariant &v, action->data().toList()) {
            bool ok;
            int kind = v.toInt(&ok);
            if (ok && !memcheckSettings->visibleErrorKinds().contains(kind))
                contained = false;
        }
        action->setChecked(contained);
    }

    m_filterProjectAction->setChecked(!memcheckSettings->filterExternalIssues());

    m_errorView->settingsChanged(m_settings);

    connect(memcheckSettings, SIGNAL(visibleErrorKindsChanged(QList<int>)),
            m_errorProxyModel, SLOT(setAcceptedKinds(QList<int>)));
    m_errorProxyModel->setAcceptedKinds(memcheckSettings->visibleErrorKinds());

    connect(memcheckSettings, SIGNAL(filterExternalIssuesChanged(bool)),
            m_errorProxyModel, SLOT(setFilterExternalIssues(bool)));
    m_errorProxyModel->setFilterExternalIssues(memcheckSettings->filterExternalIssues());
}

QString MemcheckTool::id() const
{
    return "Memcheck";
}

QString MemcheckTool::displayName() const
{
    return tr("Analyze Memory");
}

IAnalyzerTool::ToolMode MemcheckTool::mode() const
{
    return DebugMode;
}

namespace Analyzer {
namespace Internal {
class FrameFinder : public ErrorListModel::RelevantFrameFinder {
public:
    Frame findRelevant(const Error &error) const {
        const QVector<Stack> stacks = error.stacks();
        if (stacks.isEmpty())
            return Frame();
        const Stack &stack = stacks[0];
        const QVector<Frame> frames = stack.frames();
        if (frames.isEmpty())
            return Frame();

        //find the first frame belonging to the project
        foreach(const Frame &frame, frames) {
            if (frame.directory().isEmpty() || frame.file().isEmpty())
                continue;

            //filepaths can contain "..", clean them:
            const QString f = QFileInfo(frame.directory() + QLatin1Char('/') + frame.file()).absoluteFilePath();
            if (m_projectFiles.contains(f))
                return frame;
        }

        //if no frame belonging to the project was found, return the first one that is not malloc/new
        foreach(const Frame &frame, frames) {
            if (!frame.functionName().isEmpty() && frame.functionName() != QLatin1String("malloc")
                && !frame.functionName().startsWith("operator new(") )
            {
                return frame;
            }
        }

        //else fallback to the first frame
        return frames.first();
    }
    void setFiles(const QStringList &files)
    {
        m_projectFiles = files;
    }
private:
    QStringList m_projectFiles;
};
}
}

static void initKindFilterAction(QAction *action, const QList<int> &kinds)
{
    action->setCheckable(true);
    QVariantList data;
    foreach (int kind, kinds)
        data << kind;
    action->setData(data);
}

void MemcheckTool::initialize(ExtensionSystem::IPlugin */*plugin*/)
{
    AnalyzerManager *am = AnalyzerManager::instance();

    m_errorView = new MemcheckErrorView;
    m_errorView->setFrameStyle(QFrame::NoFrame);
    m_errorView->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_errorModel = new ErrorListModel(m_errorView);
    m_frameFinder = new FrameFinder;
    m_errorModel->setRelevantFrameFinder(QSharedPointer<FrameFinder>(m_frameFinder));
    m_errorProxyModel = new MemcheckErrorFilterProxyModel(m_errorView);
    m_errorProxyModel->setSourceModel(m_errorModel);
    m_errorProxyModel->setDynamicSortFilter(true);
    m_errorView->setModel(m_errorProxyModel);
    m_errorView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // make m_errorView->selectionModel()->selectedRows() return something
    m_errorView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_errorView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_errorView->setAutoScroll(false);
    m_errorView->setObjectName("Valgrind.MemcheckTool.ErrorView");

    am->createDockWidget(this, tr("Memory Errors"), m_errorView, Qt::BottomDockWidgetArea);

    QHBoxLayout *layout = new QHBoxLayout;
    layout->setMargin(0);
    layout->setSpacing(0);

    { // clear / next / prev
    QToolButton *button = 0;

    m_clearAction = new QAction(this);
    m_clearAction->setIcon(QIcon(Core::Constants::ICON_CLEAN_PANE));
    m_clearAction->setText(tr("Clear"));
    connect(m_clearAction, SIGNAL(triggered()), this, SLOT(slotClear()));
    button = new QToolButton;
    button->setDefaultAction(m_clearAction);
    layout->addWidget(button);

    m_prevAction = new QAction(this);
    m_prevAction->setIcon(QIcon(QLatin1String(Core::Constants::ICON_PREV)));
    m_prevAction->setText(tr("Previous Item"));
    connect(m_prevAction, SIGNAL(triggered()), this, SLOT(slotPrev()));
    button = new QToolButton;
    button->setDefaultAction(m_prevAction);
    layout->addWidget(button);

    m_nextAction = new QAction(this);
    m_nextAction->setIcon(QIcon(QLatin1String(Core::Constants::ICON_NEXT)));
    m_nextAction->setText(tr("Next Item"));
    connect(m_nextAction, SIGNAL(triggered()), this, SLOT(slotNext()));
    button = new QToolButton;
    button->setDefaultAction(m_nextAction);
    layout->addWidget(button);
    }
    {
    // filter
    QToolButton *filterButton = new QToolButton;
    filterButton->setIcon(QIcon(Core::Constants::ICON_FILTER));
    filterButton->setText(tr("Error Filter"));
    filterButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *filterMenu = new QMenu(filterButton);

    QAction *a = filterMenu->addAction(tr("Definite Memory Leaks"));
    initKindFilterAction(a, QList<int>() << Leak_DefinitelyLost << Leak_IndirectlyLost);
    m_errorFilterActions << a;

    a = filterMenu->addAction(tr("Possible Memory Leaks"));
    initKindFilterAction(a, QList<int>() << Leak_PossiblyLost << Leak_StillReachable);
    m_errorFilterActions << a;

    a = filterMenu->addAction(tr("Use of Uninitialized Memory"));
    initKindFilterAction(a, QList<int>() << InvalidRead << InvalidWrite << InvalidJump << Overlap
                                         << InvalidMemPool << UninitCondition << UninitValue
                                         << SyscallParam << ClientCheck);
    m_errorFilterActions << a;

    a = filterMenu->addAction(tr("Invalid Frees"));
    initKindFilterAction(a, QList<int>() << InvalidFree << MismatchedFree);
    m_errorFilterActions << a;

    filterMenu->addSeparator();

    m_filterProjectAction = filterMenu->addAction(tr("External Errors"));
    m_filterProjectAction->setToolTip(tr("Show issues originating outside currently opened projects."));
    m_filterProjectAction->setCheckable(true);

    m_suppressionSeparator = filterMenu->addSeparator();
    m_suppressionSeparator->setText(tr("Suppressions"));
    m_suppressionSeparator->setToolTip(tr("These suppression files where used in the last memory analyzer run."));

    connect(filterMenu, SIGNAL(triggered(QAction *)), SLOT(updateErrorFilter()));
    filterButton->setMenu(filterMenu);
    layout->addWidget(filterButton);
    }

    layout->addStretch();
    QWidget *toolbar = new QWidget;
    toolbar->setLayout(layout);
    am->setToolbar(this, toolbar);

    // register shortcuts
    Core::ActionManager *actionManager = Core::ICore::instance()->actionManager();
    const Core::Context analyzeContext(Constants::C_ANALYZEMODE);

    Core::Command *cmd;

    cmd = actionManager->registerAction(m_prevAction, "Analyzer.MemcheckTool.previtem", analyzeContext);
    cmd->setDefaultKeySequence(QKeySequence("Shift+F10"));

    cmd = actionManager->registerAction(m_nextAction, "Analyzer.MemcheckTool.nextitem", analyzeContext);
    cmd->setDefaultKeySequence(QKeySequence("F10"));

    maybeActiveRunConfigurationChanged();
}

IAnalyzerEngine *MemcheckTool::createEngine(ProjectExplorer::RunConfiguration *runConfiguration)
{
    m_frameFinder->setFiles(runConfiguration->target()->project()->files(ProjectExplorer::Project::AllFiles));

    MemcheckEngine *engine = new MemcheckEngine(runConfiguration);

    connect(engine, SIGNAL(starting(const IAnalyzerEngine*)),
            this, SLOT(engineStarting(const IAnalyzerEngine*)));
    connect(engine, SIGNAL(parserError(Valgrind::XmlProtocol::Error)),
            this, SLOT(parserError(Valgrind::XmlProtocol::Error)));
    connect(engine, SIGNAL(internalParserError(QString)),
            this, SLOT(internalParserError(QString)));
    return engine;
}

void MemcheckTool::engineStarting(const IAnalyzerEngine *engine)
{
    slotClear();

    const QString dir = engine->runConfiguration()->target()->project()->projectDirectory();
    const MemcheckEngine *mEngine = dynamic_cast<const MemcheckEngine*>(engine);
    QTC_ASSERT(mEngine, return);
    const QString name = QFileInfo(mEngine->executable()).fileName();

    m_errorView->setDefaultSuppressionFile(dir + QDir::separator() + name + QLatin1String(".supp"));

    QMenu *menu = qobject_cast<QMenu*>(m_suppressionSeparator->parentWidget());
    QTC_ASSERT(menu, return);
    foreach(const QString &file, mEngine->suppressionFiles()) {
        QAction *action = menu->addAction(QFileInfo(file).fileName());
        action->setToolTip(file);
        action->setData(file);
        connect(action, SIGNAL(triggered(bool)),
                this, SLOT(suppressionActionTriggered()));
        m_suppressionActions << action;
    }
}

void MemcheckTool::suppressionActionTriggered()
{
    QAction *action = qobject_cast<QAction*>(sender());
    QTC_ASSERT(action, return);
    const QString file = action->data().toString();
    QTC_ASSERT(!file.isEmpty(), return);

    TextEditor::BaseTextEditorWidget::openEditorAt(file, 0);
}

void MemcheckTool::parserError(const Valgrind::XmlProtocol::Error &error)
{
    m_errorModel->addError(error);
}

void MemcheckTool::internalParserError(const QString &errorString)
{
    QMessageBox::critical(m_errorView, tr("Internal Error"), tr("Error occurred parsing valgrind output: %1").arg(errorString));
}

void MemcheckTool::slotNext()
{
    QModelIndex current = m_errorView->selectionModel()->currentIndex();
    if (!current.isValid()) {
        if (!m_errorView->model()->rowCount())
            return;

        current = m_errorView->model()->index(0, 0);
    } else if (current.row() < m_errorView->model()->rowCount(current.parent()) - 1) {
        current = m_errorView->model()->index(current.row() + 1, 0);
    } else {
        return;
    }

    m_errorView->selectionModel()->setCurrentIndex(current, QItemSelectionModel::ClearAndSelect);
    m_errorView->scrollTo(current);
}

void MemcheckTool::slotPrev()
{
    QModelIndex current = m_errorView->selectionModel()->currentIndex();
    if (!current.isValid()) {
        if (!m_errorView->model()->rowCount())
            return;
        current = m_errorView->model()->index(m_errorView->model()->rowCount() - 1, 0);
    } else if (current.row() > 0) {
        current = m_errorView->model()->index(current.row() - 1, 0);
    } else {
        return;
    }

    m_errorView->selectionModel()->setCurrentIndex(current, QItemSelectionModel::ClearAndSelect);
    m_errorView->scrollTo(current);
}

void MemcheckTool::slotClear()
{
    m_errorModel->clear();

    qDeleteAll(m_suppressionActions);
    m_suppressionActions.clear();
    QTC_ASSERT(m_suppressionSeparator->parentWidget()->actions().last() == m_suppressionSeparator, qt_noop());
}

void MemcheckTool::updateErrorFilter()
{
    QTC_ASSERT(m_settings, return);

    AbstractMemcheckSettings *memcheckSettings = m_settings->subConfig<AbstractMemcheckSettings>();
    QTC_ASSERT(memcheckSettings, return);
    memcheckSettings->setFilterExternalIssues(!m_filterProjectAction->isChecked());

    QList<int> errorKinds;
    foreach (QAction *a, m_errorFilterActions) {
        if (!a->isChecked())
            continue;
        foreach (const QVariant &v, a->data().toList()) {
            bool ok;
            int kind = v.toInt(&ok);
            if (ok)
                errorKinds << kind;
        }
    }
    memcheckSettings->setVisibleErrorKinds(errorKinds);
}
