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

#ifndef MAEMODEVICECONFIGURATIONS_H
#define MAEMODEVICECONFIGURATIONS_H

#include "maemoglobal.h"

#include <utils/ssh/sshconnection.h>

#include <QtCore/QAbstractListModel>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace Qt4ProjectManager {
namespace Internal {

class MaemoPortList
{
public:
    void addPort(int port);
    void addRange(int startPort, int endPort);
    bool hasMore() const;
    int count() const;
    int getNext();
    QString toString() const;

private:
    typedef QPair<int, int> Range;
    QList<Range> m_ranges;
};


class MaemoDeviceConfig
{
    friend class MaemoDeviceConfigurations;
public:
    typedef QSharedPointer<const MaemoDeviceConfig> ConstPtr;
    typedef quint64 Id;
    enum DeviceType { Physical, Emulator };

    MaemoPortList freePorts() const;
    Utils::SshConnectionParameters sshParameters() const { return m_sshParameters; }
    QString name() const { return m_name; }
    MaemoGlobal::OsVersion osVersion() const { return m_osVersion; }
    DeviceType type() const { return m_type; }
    QString portsSpec() const { return m_portsSpec; }
    Id internalId() const { return m_internalId; }
    bool isDefault() const { return m_isDefault; }
    static QString portsRegExpr();
    static QString defaultHost(DeviceType type, MaemoGlobal::OsVersion osVersion);
    static QString defaultPrivateKeyFilePath();
    static QString defaultPublicKeyFilePath();
    static QString defaultUser(MaemoGlobal::OsVersion osVersion);
    static int defaultSshPort(DeviceType type);
    static QString defaultQemuPassword(MaemoGlobal::OsVersion osVersion);

    static const Id InvalidId;

private:
    typedef QSharedPointer<MaemoDeviceConfig> Ptr;

    MaemoDeviceConfig(const QString &name, MaemoGlobal::OsVersion osVersion,
        DeviceType type, const Utils::SshConnectionParameters &sshParams,
        Id &nextId);
    MaemoDeviceConfig(const QSettings &settings, Id &nextId);
    MaemoDeviceConfig(const ConstPtr &other);

    MaemoDeviceConfig(const MaemoDeviceConfig &);
    MaemoDeviceConfig &operator=(const MaemoDeviceConfig &);

    static Ptr createHardwareConfig(const QString &name,
        MaemoGlobal::OsVersion osVersion, const QString &hostName,
        const QString &privateKeyFilePath, Id &nextId);
    static Ptr createGenericLinuxConfigUsingPassword(const QString &name,
        const QString &hostName, const QString &userName,
        const QString &password, Id &nextId);
    static Ptr createGenericLinuxConfigUsingKey(const QString &name,
        const QString &hostName, const QString &userName,
        const QString &privateKeyFilePath, Id &nextId);
    static Ptr createEmulatorConfig(const QString &name,
        MaemoGlobal::OsVersion osVersion, Id &nextId);
    static Ptr create(const QSettings &settings, Id &nextId);
    static Ptr create(const ConstPtr &other);

    void save(QSettings &settings) const;
    QString defaultPortsSpec(DeviceType type) const;

    Utils::SshConnectionParameters m_sshParameters;
    QString m_name;
    MaemoGlobal::OsVersion m_osVersion;
    DeviceType m_type;
    QString m_portsSpec;
    bool m_isDefault;
    Id m_internalId;
};


class MaemoDeviceConfigurations : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY(MaemoDeviceConfigurations)
public:
    static MaemoDeviceConfigurations *instance(QObject *parent = 0);

    static void replaceInstance(const MaemoDeviceConfigurations *other);
    static MaemoDeviceConfigurations *cloneInstance();

    MaemoDeviceConfig::ConstPtr deviceAt(int index) const;
    MaemoDeviceConfig::ConstPtr find(MaemoDeviceConfig::Id id) const;
    MaemoDeviceConfig::ConstPtr defaultDeviceConfig(const MaemoGlobal::OsVersion osVersion) const;
    bool hasConfig(const QString &name) const;
    int indexForInternalId(MaemoDeviceConfig::Id internalId) const;
    MaemoDeviceConfig::Id internalId(MaemoDeviceConfig::ConstPtr devConf) const;

    void setDefaultSshKeyFilePath(const QString &path) { m_defaultSshKeyFilePath = path; }
    QString defaultSshKeyFilePath() const { return m_defaultSshKeyFilePath; }

    void addHardwareDeviceConfiguration(const QString &name,
        MaemoGlobal::OsVersion osVersion, const QString &hostName,
        const QString &privateKeyFilePath);
    void addGenericLinuxConfigurationUsingPassword(const QString &name,
        const QString &hostName, const QString &userName,
        const QString &password);
    void addGenericLinuxConfigurationUsingKey(const QString &name,
        const QString &hostName, const QString &userName,
        const QString &privateKeyFilePath);
    void addEmulatorDeviceConfiguration(const QString &name,
        MaemoGlobal::OsVersion osVersion);
    void removeConfiguration(int index);
    void setConfigurationName(int i, const QString &name);
    void setSshParameters(int i, const Utils::SshConnectionParameters &params);
    void setPortsSpec(int i, const QString &portsSpec);
    void setDefaultDevice(int index);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index,
        int role = Qt::DisplayRole) const;

signals:
    void updated();

private:
    MaemoDeviceConfigurations(QObject *parent);
    void load();
    void save();
    static void copy(const MaemoDeviceConfigurations *source,
        MaemoDeviceConfigurations *target, bool deep);
    void addConfiguration(const MaemoDeviceConfig::Ptr &devConfig);
    void ensureDefaultExists(MaemoGlobal::OsVersion osVersion);

    static MaemoDeviceConfigurations *m_instance;
    MaemoDeviceConfig::Id m_nextId;
    QList<MaemoDeviceConfig::Ptr> m_devConfigs;
    QString m_defaultSshKeyFilePath;
};

} // namespace Internal
} // namespace Qt4ProjectManager

#endif // MAEMODEVICECONFIGURATIONS_H
