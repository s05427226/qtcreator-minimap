/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of Qt Creator.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
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
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "maemopackagecreationfactory.h"

#include "maemoglobal.h"
#include "maemopackagecreationstep.h"
#include "qt4maemodeployconfiguration.h"
#include "qt4maemotarget.h"

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/deployconfiguration.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/target.h>
#include <qt4projectmanagerconstants.h>

#include <QtCore/QCoreApplication>

using ProjectExplorer::BuildStepList;
using ProjectExplorer::BuildStep;

namespace Qt4ProjectManager {
namespace Internal {
namespace {
const QString OldCreatePackageId("Qt4ProjectManager.MaemoPackageCreationStep");
} // anonymous namespace

MaemoPackageCreationFactory::MaemoPackageCreationFactory(QObject *parent)
    : ProjectExplorer::IBuildStepFactory(parent)
{
}

QStringList MaemoPackageCreationFactory::availableCreationIds(ProjectExplorer::BuildStepList *parent) const
{
    if (!qobject_cast<Qt4MaemoDeployConfiguration *>(parent->parent()))
        return QStringList();

    if (qobject_cast<AbstractDebBasedQt4MaemoTarget *>(parent->target())
            && !parent->contains(MaemoDebianPackageCreationStep::CreatePackageId)) {
        return QStringList() << MaemoDebianPackageCreationStep::CreatePackageId;
    } else if (qobject_cast<AbstractRpmBasedQt4MaemoTarget *>(parent->target())
               && !parent->contains(MaemoRpmPackageCreationStep::CreatePackageId)) {
        return QStringList() << MaemoRpmPackageCreationStep::CreatePackageId;
    } else if (!parent->contains(MaemoTarPackageCreationStep::CreatePackageId)) {
        return QStringList() << MaemoTarPackageCreationStep::CreatePackageId;
    }
    return QStringList();
}

QString MaemoPackageCreationFactory::displayNameForId(const QString &id) const
{
    if (id == MaemoDebianPackageCreationStep::CreatePackageId) {
        return QCoreApplication::translate("Qt4ProjectManager::Internal::MaemoPackageCreationFactory",
            "Create Debian Package");
    } else if (id == MaemoRpmPackageCreationStep::CreatePackageId) {
        return QCoreApplication::translate("Qt4ProjectManager::Internal::MaemoPackageCreationFactory",
            "Create RPM Package");
    } else if (id == MaemoTarPackageCreationStep::CreatePackageId) {
        return QCoreApplication::translate("Qt4ProjectManager::Internal::MaemoPackageCreationFactory",
            "Create tarball");
    }
    return QString();
}

bool MaemoPackageCreationFactory::canCreate(ProjectExplorer::BuildStepList *parent, const QString &id) const
{
    return availableCreationIds(parent).contains(id);
}

BuildStep *MaemoPackageCreationFactory::create(ProjectExplorer::BuildStepList *parent, const QString &id)
{
    Q_ASSERT(canCreate(parent, id));
    if (id == MaemoDebianPackageCreationStep::CreatePackageId)
        return new MaemoDebianPackageCreationStep(parent);
    else if (id == MaemoRpmPackageCreationStep::CreatePackageId)
        return new MaemoRpmPackageCreationStep(parent);
    else if (id == MaemoTarPackageCreationStep::CreatePackageId)
        return new MaemoTarPackageCreationStep(parent);
    return 0;
}

bool MaemoPackageCreationFactory::canRestore(ProjectExplorer::BuildStepList *parent,
                                             const QVariantMap &map) const
{
    const QString id = ProjectExplorer::idFromMap(map);
    return canCreate(parent, id) || id == OldCreatePackageId;
}

BuildStep *MaemoPackageCreationFactory::restore(ProjectExplorer::BuildStepList *parent,
                                                const QVariantMap &map)
{
    Q_ASSERT(canRestore(parent, map));
    BuildStep * step = 0;
    const QString id = ProjectExplorer::idFromMap(map);
    if (id == MaemoDebianPackageCreationStep::CreatePackageId
            || (id == OldCreatePackageId
                && qobject_cast<AbstractDebBasedQt4MaemoTarget *>(parent->target()))) {
        step = new MaemoDebianPackageCreationStep(parent);
    } else if (id == MaemoRpmPackageCreationStep::CreatePackageId
               || (id == OldCreatePackageId
                   && qobject_cast<AbstractRpmBasedQt4MaemoTarget *>(parent->target()))) {
        step = new MaemoRpmPackageCreationStep(parent);
    } else if (id == MaemoTarPackageCreationStep::CreatePackageId) {
        step = new MaemoTarPackageCreationStep(parent);
   }
    Q_ASSERT(step);

    if (!step->fromMap(map)) {
        delete step;
        return 0;
    }
    return step;
}

bool MaemoPackageCreationFactory::canClone(ProjectExplorer::BuildStepList *parent,
                                           ProjectExplorer::BuildStep *product) const
{
    return canCreate(parent, product->id());
}

BuildStep *MaemoPackageCreationFactory::clone(ProjectExplorer::BuildStepList *parent,
                                              ProjectExplorer::BuildStep *product)
{
    Q_ASSERT(canClone(parent, product));
    MaemoDebianPackageCreationStep * const debianStep
        = qobject_cast<MaemoDebianPackageCreationStep *>(product);
    if (debianStep) {
        return new MaemoDebianPackageCreationStep(parent, debianStep);
    } else {
        MaemoRpmPackageCreationStep * const rpmStep
            = qobject_cast<MaemoRpmPackageCreationStep *>(product);
        if (rpmStep)  {
            return new MaemoRpmPackageCreationStep(parent, rpmStep);
        } else {
            return new MaemoTarPackageCreationStep(parent,
                qobject_cast<MaemoTarPackageCreationStep *>(product));
        }
    }
}

} // namespace Internal
} // namespace Qt4ProjectManager
