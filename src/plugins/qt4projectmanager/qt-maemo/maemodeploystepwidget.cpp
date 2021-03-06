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

#include "maemodeploystepwidget.h"
#include "ui_maemodeploystepwidget.h"

#include "abstractmaemodeploystep.h"
#include "maemodeviceconfigurations.h"
#include "maemosettingspages.h"
#include "maemoglobal.h"
#include "maemomanager.h"
#include "maemopertargetdeviceconfigurationlistmodel.h"
#include "maemorunconfiguration.h"
#include "qt4maemodeployconfiguration.h"
#include "qt4maemotarget.h"

#include <coreplugin/icore.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/target.h>
#include <utils/qtcassert.h>

namespace Qt4ProjectManager {
namespace Internal {

MaemoDeployStepWidget::MaemoDeployStepWidget(AbstractMaemoDeployStep *step) :
    ProjectExplorer::BuildStepConfigWidget(),
    ui(new Ui::MaemoDeployStepWidget),
    m_step(step)
{
    ui->setupUi(this);
    ProjectExplorer::BuildStepList * const list
        = qobject_cast<ProjectExplorer::BuildStepList *>(step->parent());
    connect(list, SIGNAL(stepInserted(int)), SIGNAL(updateSummary()));
    connect(list, SIGNAL(stepMoved(int,int)), SIGNAL(updateSummary()));
    connect(list, SIGNAL(stepRemoved(int)), SIGNAL(updateSummary()));
}

MaemoDeployStepWidget::~MaemoDeployStepWidget()
{
    delete ui;
}

void MaemoDeployStepWidget::init()
{
    ui->deviceConfigComboBox->setModel(m_step->maemoDeployConfig()->deviceConfigModel());
    connect(m_step, SIGNAL(deviceConfigChanged()), SLOT(handleDeviceUpdate()));
    handleDeviceUpdate();
    connect(ui->deviceConfigComboBox, SIGNAL(activated(int)), this,
        SLOT(setCurrentDeviceConfig(int)));
    connect(ui->manageDevConfsLabel, SIGNAL(linkActivated(QString)),
        SLOT(showDeviceConfigurations()));
}

void MaemoDeployStepWidget::handleDeviceUpdate()
{
    const MaemoDeviceConfig::ConstPtr &devConf = m_step->deviceConfig();
    const MaemoDeviceConfig::Id internalId
        = MaemoDeviceConfigurations::instance()->internalId(devConf);
    const int newIndex = m_step->maemoDeployConfig()->deviceConfigModel()
        ->indexForInternalId(internalId);
    ui->deviceConfigComboBox->setCurrentIndex(newIndex);
    emit updateSummary();
}

QString MaemoDeployStepWidget::summaryText() const
{
    QString error;
    if (!m_step->isDeploymentPossible(error)) {
        return QLatin1String("<font color=\"red\">")
            + tr("Cannot deploy: %1").arg(error)
            + QLatin1String("</font>");
    }
    return tr("<b>Deploy to device</b>: %1")
        .arg(MaemoGlobal::deviceConfigurationName(m_step->deviceConfig()));
}

QString MaemoDeployStepWidget::displayName() const
{
    return QString();
}

void MaemoDeployStepWidget::setCurrentDeviceConfig(int index)
{
    disconnect(m_step, SIGNAL(deviceConfigChanged()), this,
        SLOT(handleDeviceUpdate()));
    m_step->setDeviceConfig(index);
    connect(m_step, SIGNAL(deviceConfigChanged()), SLOT(handleDeviceUpdate()));
    updateSummary();
}

void MaemoDeployStepWidget::showDeviceConfigurations()
{
    MaemoDeviceConfigurationsSettingsPage *page
        = MaemoManager::instance().deviceConfigurationsSettingsPage();
    Core::ICore::instance()->showOptionsDialog(page->category(), page->id());
}

} // namespace Internal
} // namespace Qt4ProjectManager
