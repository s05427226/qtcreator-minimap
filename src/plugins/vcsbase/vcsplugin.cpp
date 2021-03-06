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

#include "vcsplugin.h"
#include "diffhighlighter.h"
#include "commonsettingspage.h"
#include "nicknamedialog.h"
#include "vcsbaseoutputwindow.h"
#include "corelistener.h"

#include <coreplugin/icore.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/uniqueidmanager.h>
#include <coreplugin/mimedatabase.h>

#include <QtCore/QtPlugin>
#include <QtCore/QDebug>

namespace VCSBase {
namespace Internal {

VCSPlugin *VCSPlugin::m_instance = 0;

VCSPlugin::VCSPlugin() :
    m_settingsPage(0),
    m_nickNameModel(0),
    m_coreListener(0)
{
    m_instance = this;
}

VCSPlugin::~VCSPlugin()
{
    m_instance = 0;
}

bool VCSPlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorMessage)

    Core::ICore *core = Core::ICore::instance();
    if (!core->mimeDatabase()->addMimeTypes(QLatin1String(":/vcsbase/VCSBase.mimetypes.xml"), errorMessage))
        return false;

    m_coreListener = new CoreListener;
    addAutoReleasedObject(m_coreListener);

    m_settingsPage = new CommonOptionsPage;
    addAutoReleasedObject(m_settingsPage);
    addAutoReleasedObject(VCSBaseOutputWindow::instance());
    connect(m_settingsPage, SIGNAL(settingsChanged(VCSBase::Internal::CommonVcsSettings)),
            this, SIGNAL(settingsChanged(VCSBase::Internal::CommonVcsSettings)));
    connect(m_settingsPage, SIGNAL(settingsChanged(VCSBase::Internal::CommonVcsSettings)),
            this, SLOT(slotSettingsChanged()));
    slotSettingsChanged();
    return true;
}

void VCSPlugin::extensionsInitialized()
{
}

VCSPlugin *VCSPlugin::instance()
{
    return m_instance;
}

CoreListener *VCSPlugin::coreListener() const
{
    return m_coreListener;
}

CommonVcsSettings VCSPlugin::settings() const
{
    return m_settingsPage->settings();
}

/* Delayed creation/update of the nick name model. */
QStandardItemModel *VCSPlugin::nickNameModel()
{
    if (!m_nickNameModel) {
        m_nickNameModel = NickNameDialog::createModel(this);
        populateNickNameModel();
    }
    return m_nickNameModel;
}

void VCSPlugin::populateNickNameModel()
{
    QString errorMessage;
    if (!NickNameDialog::populateModelFromMailCapFile(settings().nickNameMailMap,
                                                      m_nickNameModel,
                                                      &errorMessage)) {
        qWarning("%s", qPrintable(errorMessage));
    }
}

void VCSPlugin::slotSettingsChanged()
{
    if (m_nickNameModel)
        populateNickNameModel();
}

} // namespace Internal
} // namespace VCSBase

Q_EXPORT_PLUGIN(VCSBase::Internal::VCSPlugin)
