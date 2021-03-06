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

#include "qmlinspectortoolbar.h"
#include "qmljsinspectorconstants.h"
#include "qmljstoolbarcolorbox.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/uniqueidmanager.h>
#include <coreplugin/icore.h>
#include <debugger/debuggerconstants.h>
#include <debugger/debuggermainwindow.h>
#include <debugger/debuggerplugin.h>
#include <extensionsystem/pluginmanager.h>
#include <projectexplorer/projectexplorerconstants.h>

#include <utils/styledbar.h>

#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QHBoxLayout>
#include <QtGui/QMenu>
#include <QtGui/QToolButton>
#include <QtGui/QLineEdit>

namespace QmlJSInspector {
namespace Internal {

static QToolButton *toolButton(QAction *action)
{
    QToolButton *button = new QToolButton;
    button->setDefaultAction(action);
    return button;
}

QmlInspectorToolBar::QmlInspectorToolBar(QObject *parent) :
    QObject(parent),
    m_fromQmlAction(0),
    m_observerModeAction(0),
    m_playAction(0),
    m_selectAction(0),
    m_zoomAction(0),
    m_colorPickerAction(0),
    m_showAppOnTopAction(0),
    m_defaultAnimSpeedAction(0),
    m_halfAnimSpeedAction(0),
    m_fourthAnimSpeedAction(0),
    m_eighthAnimSpeedAction(0),
    m_tenthAnimSpeedAction(0),
    m_menuPauseAction(0),
    m_playIcon(QIcon(QLatin1String(":/qml/images/play-small.png"))),
    m_pauseIcon(QIcon(QLatin1String(":/qml/images/pause-small.png"))),
    m_colorBox(0),
    m_emitSignals(true),
    m_isRunning(false),
    m_animationSpeed(1.0f),
    m_previousAnimationSpeed(0.0f),
    m_activeTool(NoTool),
    m_barWidget(0)
{
}

void QmlInspectorToolBar::setEnabled(bool value)
{
    m_fromQmlAction->setEnabled(value);
    m_showAppOnTopAction->setEnabled(value);
    m_observerModeAction->setEnabled(value);
    m_playAction->setEnabled(value);
    m_selectAction->setEnabled(value);
    m_zoomAction->setEnabled(value);
    m_colorPickerAction->setEnabled(value);
    m_colorBox->setEnabled(value);
}

void QmlInspectorToolBar::enable()
{
    setEnabled(true);
    m_emitSignals = false;
    m_showAppOnTopAction->setChecked(false);
    m_observerModeAction->setChecked(false);
    setAnimationSpeed(1.0f);
    activateDesignModeOnClick();
    m_emitSignals = true;
}

void QmlInspectorToolBar::disable()
{
    setAnimationSpeed(1.0f);
    activateSelectTool();
    setEnabled(false);
}

void QmlInspectorToolBar::activateColorPicker()
{
    m_emitSignals = false;
    activateColorPickerOnClick();
    m_emitSignals = true;
}

void QmlInspectorToolBar::activateSelectTool()
{
    m_emitSignals = false;
    activateSelectToolOnClick();
    m_emitSignals = true;
}

void QmlInspectorToolBar::activateZoomTool()
{
    m_emitSignals = false;
    activateZoomOnClick();
    m_emitSignals = true;
}

void QmlInspectorToolBar::setAnimationSpeed(qreal slowdownFactor)
{
    m_emitSignals = false;
    if (slowdownFactor != 0) {
        m_animationSpeed = slowdownFactor;

        if (slowdownFactor == 1.0f) {
            m_defaultAnimSpeedAction->setChecked(true);
        } else if (slowdownFactor == 2.0f) {
            m_halfAnimSpeedAction->setChecked(true);
        } else if (slowdownFactor == 4.0f) {
            m_fourthAnimSpeedAction->setChecked(true);
        } else if (slowdownFactor == 8.0f) {
            m_eighthAnimSpeedAction->setChecked(true);
        } else if (slowdownFactor == 10.0f) {
            m_tenthAnimSpeedAction->setChecked(true);
        }
        updatePlayAction();
    } else {
        m_menuPauseAction->setChecked(true);
        updatePauseAction();
    }

    m_emitSignals = true;
}

void QmlInspectorToolBar::setDesignModeBehavior(bool inDesignMode)
{
    m_emitSignals = false;
    m_observerModeAction->setChecked(inDesignMode);
    activateDesignModeOnClick();
    m_emitSignals = true;
}

void QmlInspectorToolBar::setShowAppOnTop(bool showAppOnTop)
{
    m_emitSignals = false;
    m_showAppOnTopAction->setChecked(showAppOnTop);
    m_emitSignals = true;
}

void QmlInspectorToolBar::createActions(const Core::Context &context)
{
    Core::ICore *core = Core::ICore::instance();
    Core::ActionManager *am = core->actionManager();

    m_fromQmlAction =
            new QAction(QIcon(QLatin1String(":/qml/images/from-qml-small.png")),
                        tr("Apply Changes on Save"), this);
    m_showAppOnTopAction =
            new QAction(QIcon(QLatin1String(":/qml/images/app-on-top.png")),
                        tr("Show application on top"), this);
    m_observerModeAction =
            new QAction(QIcon(QLatin1String(":/qml/images/observermode.png")),
                        tr("Observer Mode"), this);
    m_playAction =
            new QAction(m_pauseIcon, tr("Play/Pause Animations"), this);
    m_selectAction =
            new QAction(QIcon(QLatin1String(":/qml/images/select-small.png")),
                        tr("Select"), this);
    m_zoomAction =
            new QAction(QIcon(QLatin1String(":/qml/images/zoom-small.png")),
                        tr("Zoom"), this);
    m_colorPickerAction =
            new QAction(QIcon(QLatin1String(":/qml/images/color-picker-small.png")),
                        tr("Color Picker"), this);

    m_fromQmlAction->setCheckable(true);
    m_fromQmlAction->setChecked(true);
    m_showAppOnTopAction->setCheckable(true);
    m_showAppOnTopAction->setChecked(false);
    m_observerModeAction->setCheckable(true);
    m_observerModeAction->setChecked(false);
    m_selectAction->setCheckable(true);
    m_zoomAction->setCheckable(true);
    m_colorPickerAction->setCheckable(true);

    am->registerAction(m_observerModeAction, Constants::DESIGNMODE_ACTION, context);
    am->registerAction(m_playAction, Constants::PLAY_ACTION, context);
    am->registerAction(m_selectAction, Constants::SELECT_ACTION, context);
    am->registerAction(m_zoomAction, Constants::ZOOM_ACTION, context);
    am->registerAction(m_colorPickerAction, Constants::COLOR_PICKER_ACTION, context);
    am->registerAction(m_fromQmlAction, Constants::FROM_QML_ACTION, context);
    am->registerAction(m_showAppOnTopAction, Constants::SHOW_APP_ON_TOP_ACTION, context);

    m_barWidget = new Utils::StyledBar;
    m_barWidget->setSingleRow(true);
    m_barWidget->setProperty("topBorder", true);

    QMenu *playSpeedMenu = new QMenu(m_barWidget);
    QActionGroup *playSpeedMenuActions = new QActionGroup(this);
    playSpeedMenuActions->setExclusive(true);
    playSpeedMenu->addAction(tr("Animation Speed"));
    playSpeedMenu->addSeparator();
    m_defaultAnimSpeedAction =
            playSpeedMenu->addAction(tr("1x"), this, SLOT(changeToDefaultAnimSpeed()));
    m_defaultAnimSpeedAction->setCheckable(true);
    m_defaultAnimSpeedAction->setChecked(true);
    playSpeedMenuActions->addAction(m_defaultAnimSpeedAction);

    m_halfAnimSpeedAction =
            playSpeedMenu->addAction(tr("0.5x"), this, SLOT(changeToHalfAnimSpeed()));
    m_halfAnimSpeedAction->setCheckable(true);
    playSpeedMenuActions->addAction(m_halfAnimSpeedAction);

    m_fourthAnimSpeedAction =
            playSpeedMenu->addAction(tr("0.25x"), this, SLOT(changeToFourthAnimSpeed()));
    m_fourthAnimSpeedAction->setCheckable(true);
    playSpeedMenuActions->addAction(m_fourthAnimSpeedAction);

    m_eighthAnimSpeedAction =
            playSpeedMenu->addAction(tr("0.125x"), this, SLOT(changeToEighthAnimSpeed()));
    m_eighthAnimSpeedAction->setCheckable(true);
    playSpeedMenuActions->addAction(m_eighthAnimSpeedAction);

    m_tenthAnimSpeedAction =
            playSpeedMenu->addAction(tr("0.1x"), this, SLOT(changeToTenthAnimSpeed()));
    m_tenthAnimSpeedAction->setCheckable(true);
    playSpeedMenuActions->addAction(m_tenthAnimSpeedAction);

    m_menuPauseAction = playSpeedMenu->addAction(tr("Pause"), this, SLOT(updatePauseAction()));
    m_menuPauseAction->setCheckable(true);
    m_menuPauseAction->setIcon(m_pauseIcon);
    playSpeedMenuActions->addAction(m_menuPauseAction);

    QHBoxLayout *toolBarLayout = new QHBoxLayout(m_barWidget);
    toolBarLayout->setMargin(0);
    toolBarLayout->setSpacing(5);

    m_operateByInstructionButton = toolButton(am->command(Debugger::Constants::OPERATE_BY_INSTRUCTION)->action());

    // Add generic debugging controls
    toolBarLayout->addWidget(toolButton(Debugger::DebuggerPlugin::visibleDebugAction()));
    toolBarLayout->addWidget(toolButton(am->command(Debugger::Constants::STOP)->action()));
    toolBarLayout->addWidget(toolButton(am->command(Debugger::Constants::NEXT)->action()));
    toolBarLayout->addWidget(toolButton(am->command(Debugger::Constants::STEP)->action()));
    toolBarLayout->addWidget(toolButton(am->command(Debugger::Constants::STEPOUT)->action()));
    toolBarLayout->addWidget(m_operateByInstructionButton);
    toolBarLayout->addWidget(new Utils::StyledSeparator);
    toolBarLayout->addStretch(1);

    toolBarLayout->addWidget(toolButton(am->command(Constants::FROM_QML_ACTION)->action()));
    toolBarLayout->addWidget(toolButton(am->command(Constants::SHOW_APP_ON_TOP_ACTION)->action()));
    toolBarLayout->addSpacing(10);

    toolBarLayout->addWidget(toolButton(am->command(Constants::DESIGNMODE_ACTION)->action()));
    m_playButton = toolButton(am->command(Constants::PLAY_ACTION)->action());
    m_playButton->setMenu(playSpeedMenu);
    toolBarLayout->addWidget(m_playButton);
    toolBarLayout->addWidget(toolButton(am->command(Constants::SELECT_ACTION)->action()));
    toolBarLayout->addWidget(toolButton(am->command(Constants::ZOOM_ACTION)->action()));
    toolBarLayout->addWidget(toolButton(am->command(Constants::COLOR_PICKER_ACTION)->action()));

    m_colorBox = new ToolBarColorBox(m_barWidget);
    m_colorBox->setMinimumSize(20, 20);
    m_colorBox->setMaximumSize(20, 20);
    m_colorBox->setInnerBorderColor(QColor(192, 192, 192));
    m_colorBox->setOuterBorderColor(QColor(58, 58, 58));
    toolBarLayout->addWidget(m_colorBox);

    setEnabled(false);

    connect(m_fromQmlAction, SIGNAL(triggered()), SLOT(activateFromQml()));
    connect(m_showAppOnTopAction, SIGNAL(triggered()), SLOT(showAppOnTopClick()));
    connect(m_observerModeAction, SIGNAL(triggered()), SLOT(activateDesignModeOnClick()));
    connect(m_playAction, SIGNAL(triggered()), SLOT(activatePlayOnClick()));
    connect(m_colorPickerAction, SIGNAL(triggered()), SLOT(activateColorPickerOnClick()));
    connect(m_selectAction, SIGNAL(triggered()), SLOT(activateSelectToolOnClick()));
    connect(m_zoomAction, SIGNAL(triggered()), SLOT(activateZoomOnClick()));
    connect(m_colorPickerAction, SIGNAL(triggered()), SLOT(activateColorPickerOnClick()));

    Debugger::DebuggerMainWindow *mw = Debugger::DebuggerPlugin::mainWindow();
    activeDebugLanguagesChanged(mw->activeDebugLanguages());
    connect(mw, SIGNAL(activeDebugLanguagesChanged(Debugger::DebuggerLanguages)),
            this, SLOT(activeDebugLanguagesChanged(Debugger::DebuggerLanguages)));
}

QWidget *QmlInspectorToolBar::widget() const
{
    return m_barWidget;
}

void QmlInspectorToolBar::changeToDefaultAnimSpeed()
{
    m_animationSpeed = 1.0f;
    updatePlayAction();
}

void QmlInspectorToolBar::changeToHalfAnimSpeed()
{
    m_animationSpeed = 2.0f;
    updatePlayAction();
}

void QmlInspectorToolBar::changeToFourthAnimSpeed()
{
    m_animationSpeed = 4.0f;
    updatePlayAction();
}

void QmlInspectorToolBar::changeToEighthAnimSpeed()
{
    m_animationSpeed = 8.0f;
    updatePlayAction();
}

void QmlInspectorToolBar::changeToTenthAnimSpeed()
{
    m_animationSpeed = 10.0f;
    updatePlayAction();
}

void QmlInspectorToolBar::activateDesignModeOnClick()
{
    bool checked = m_observerModeAction->isChecked();

    m_selectAction->setEnabled(checked);
    m_zoomAction->setEnabled(checked);
    m_colorPickerAction->setEnabled(checked);

    if (m_emitSignals)
        emit designModeSelected(checked);
}

void QmlInspectorToolBar::activatePlayOnClick()
{
    if (m_isRunning) {
        updatePauseAction();
    } else {
        updatePlayAction();
    }
}

void QmlInspectorToolBar::updatePlayAction()
{
    m_isRunning = true;
    m_playAction->setIcon(m_pauseIcon);
    if (m_animationSpeed != m_previousAnimationSpeed)
        m_previousAnimationSpeed = m_animationSpeed;

    if (m_emitSignals)
        emit animationSpeedChanged(m_animationSpeed);

    m_playButton->setDefaultAction(m_playAction);
}

void QmlInspectorToolBar::updatePauseAction()
{
    m_isRunning = false;
    m_playAction->setIcon(m_playIcon);
    if (m_emitSignals)
        emit animationSpeedChanged(0.0f);

    m_playButton->setDefaultAction(m_playAction);
}

void QmlInspectorToolBar::activateColorPickerOnClick()
{
    m_zoomAction->setChecked(false);
    m_selectAction->setChecked(false);

    m_colorPickerAction->setChecked(true);
    if (m_activeTool != ColorPickerMode) {
        m_activeTool = ColorPickerMode;
        if (m_emitSignals)
            emit colorPickerSelected();
    }
}

void QmlInspectorToolBar::activateSelectToolOnClick()
{
    m_zoomAction->setChecked(false);
    m_colorPickerAction->setChecked(false);

    m_selectAction->setChecked(true);
    if (m_activeTool != SelectionToolMode) {
        m_activeTool = SelectionToolMode;
        if (m_emitSignals)
            emit selectToolSelected();
    }
}

void QmlInspectorToolBar::activateZoomOnClick()
{
    m_selectAction->setChecked(false);
    m_colorPickerAction->setChecked(false);

    m_zoomAction->setChecked(true);
    if (m_activeTool != ZoomMode) {
        m_activeTool = ZoomMode;
        if (m_emitSignals)
            emit zoomToolSelected();
    }
}

void QmlInspectorToolBar::showAppOnTopClick()
{
    if (m_emitSignals)
        emit showAppOnTopSelected(m_showAppOnTopAction->isChecked());
}

void QmlInspectorToolBar::setSelectedColor(const QColor &color)
{
    m_colorBox->setColor(color);
}

void QmlInspectorToolBar::activateFromQml()
{
    if (m_emitSignals)
        emit applyChangesFromQmlFileTriggered(m_fromQmlAction->isChecked());
}

void QmlInspectorToolBar::activeDebugLanguagesChanged(Debugger::DebuggerLanguages languages)
{
    m_operateByInstructionButton->setVisible(languages & Debugger::CppLanguage);
}

} // namespace Internal
} // namespace QmlJSInspector
