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

#include "maemodeploystepfactory.h"

#include "maemodeploybymountstep.h"
#include "maemoglobal.h"
#include "maemoinstalltosysrootstep.h"
#include "maemouploadandinstalldeploystep.h"
#include "qt4maemodeployconfiguration.h"
#include "qt4maemotarget.h"

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/target.h>
#include <qt4projectmanager/qt4projectmanagerconstants.h>

#include <QtCore/QCoreApplication>

using namespace ProjectExplorer;

namespace Qt4ProjectManager {
namespace Internal {
namespace {
const QString OldMaemoDeployStepId(QLatin1String("Qt4ProjectManager.MaemoDeployStep"));
} // anonymous namespace

MaemoDeployStepFactory::MaemoDeployStepFactory(QObject *parent)
    : IBuildStepFactory(parent)
{
}

QStringList MaemoDeployStepFactory::availableCreationIds(BuildStepList *parent) const
{
    if (!qobject_cast<Qt4MaemoDeployConfiguration *>(parent->parent()))
        return QStringList();

    QStringList ids;
    if (qobject_cast<Qt4BaseTarget *>(parent->target()))
        ids << MaemoUploadAndInstallDeployStep::Id;
    if (qobject_cast<AbstractQt4MaemoTarget *>(parent->target()))
        ids << MaemoCopyToSysrootStep::Id;
    if (qobject_cast<AbstractDebBasedQt4MaemoTarget *>(parent->target()))
        ids << MaemoInstallDebianPackageToSysrootStep::Id;
    else if (qobject_cast<AbstractRpmBasedQt4MaemoTarget *>(parent->target()))
        ids << MaemoInstallRpmPackageToSysrootStep::Id;
    if (qobject_cast<Qt4Maemo5Target *>(parent->target())) {
        ids << MaemoMountAndInstallDeployStep::Id
            << MaemoMountAndCopyDeployStep::Id;
    }

    return ids;
}

QString MaemoDeployStepFactory::displayNameForId(const QString &id) const
{
    if (id == MaemoMountAndInstallDeployStep::Id)
        return MaemoMountAndInstallDeployStep::DisplayName;
    else if (id == MaemoMountAndCopyDeployStep::Id)
        return MaemoMountAndCopyDeployStep::DisplayName;
    else if (id == MaemoUploadAndInstallDeployStep::Id)
        return MaemoUploadAndInstallDeployStep::DisplayName;
    else if (id == MaemoInstallDebianPackageToSysrootStep::Id)
        return MaemoInstallDebianPackageToSysrootStep::DisplayName;
    else if (id == MaemoInstallRpmPackageToSysrootStep::Id)
        return MaemoInstallRpmPackageToSysrootStep::DisplayName;
    else if (id == MaemoCopyToSysrootStep::Id)
        return MaemoCopyToSysrootStep::DisplayName;
    return QString();
}

bool MaemoDeployStepFactory::canCreate(BuildStepList *parent, const QString &id) const
{
    return availableCreationIds(parent).contains(id) && !parent->contains(id);
}

BuildStep *MaemoDeployStepFactory::create(BuildStepList *parent, const QString &id)
{
    const Target * const t = parent->target();

    if (id == MaemoInstallDebianPackageToSysrootStep::Id) {
        return new MaemoInstallDebianPackageToSysrootStep(parent);
    } else if (id == MaemoInstallRpmPackageToSysrootStep::Id) {
        return new MaemoInstallRpmPackageToSysrootStep(parent);
    } else if (id == MaemoCopyToSysrootStep::Id) {
        return new MaemoCopyToSysrootStep(parent);
    } else if (id == MaemoMountAndInstallDeployStep::Id
        || (id == OldMaemoDeployStepId && qobject_cast< const Qt4Maemo5Target *>(t))) {
        return new MaemoMountAndInstallDeployStep(parent);
    } else if (id == MaemoMountAndCopyDeployStep::Id) {
        return new MaemoMountAndCopyDeployStep(parent);
    } else if (id == MaemoUploadAndInstallDeployStep::Id
        || (id == OldMaemoDeployStepId && (qobject_cast<const Qt4HarmattanTarget *>(t)
            || qobject_cast<const Qt4MeegoTarget *>(t)))) {
        return new MaemoUploadAndInstallDeployStep(parent);
    }
    return 0;
}

bool MaemoDeployStepFactory::canRestore(BuildStepList *parent, const QVariantMap &map) const
{
    return canCreate(parent, idFromMap(map))
        || idFromMap(map) == OldMaemoDeployStepId;
}

BuildStep *MaemoDeployStepFactory::restore(BuildStepList *parent, const QVariantMap &map)
{
    Q_ASSERT(canRestore(parent, map));
    BuildStep * const step = create(parent, idFromMap(map));
    if (!step->fromMap(map)) {
        delete step;
        return 0;
    }
    return step;
}

bool MaemoDeployStepFactory::canClone(BuildStepList *parent, BuildStep *product) const
{
    return canCreate(parent, product->id());
}

BuildStep *MaemoDeployStepFactory::clone(BuildStepList *parent, BuildStep *product)
{
    Q_ASSERT(canClone(parent, product));
    if (product->id() == MaemoMountAndInstallDeployStep::Id) {
        return new MaemoMountAndInstallDeployStep(parent,
            qobject_cast<MaemoMountAndInstallDeployStep *>(product));
    } else if (product->id() == MaemoMountAndCopyDeployStep::Id) {
        return new MaemoMountAndCopyDeployStep(parent,
            qobject_cast<MaemoMountAndCopyDeployStep *>(product));
    } else if (product->id() == MaemoUploadAndInstallDeployStep::Id) {
        return new MaemoUploadAndInstallDeployStep(parent,
            qobject_cast<MaemoUploadAndInstallDeployStep*>(product));
    } else if (product->id() == MaemoInstallDebianPackageToSysrootStep::Id) {
        return new MaemoInstallDebianPackageToSysrootStep(parent,
            qobject_cast<MaemoInstallDebianPackageToSysrootStep *>(product));
    } else if (product->id() == MaemoInstallRpmPackageToSysrootStep::Id) {
        return new MaemoInstallRpmPackageToSysrootStep(parent,
            qobject_cast<MaemoInstallRpmPackageToSysrootStep *>(product));
    } else if (product->id() == MaemoCopyToSysrootStep::Id) {
        return new MaemoCopyToSysrootStep(parent,
            qobject_cast<MaemoCopyToSysrootStep *>(product));
    }
    return 0;
}

} // namespace Internal
} // namespace Qt4ProjectManager
