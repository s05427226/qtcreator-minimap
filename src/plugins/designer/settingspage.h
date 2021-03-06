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

#ifndef DESIGNER_SETTINGSPAGE_H
#define DESIGNER_SETTINGSPAGE_H

#include <coreplugin/dialogs/ioptionspage.h>

QT_BEGIN_NAMESPACE
class QDesignerOptionsPageInterface;
QT_END_NAMESPACE

namespace Designer {
namespace Internal {

class SettingsPageWidget;

class SettingsPage : public Core::IOptionsPage
{
    Q_OBJECT

public:
    explicit SettingsPage(QDesignerOptionsPageInterface *designerPage);
    virtual ~SettingsPage();

    QString id() const;
    QString displayName() const;
    QString category() const;
    QString displayCategory() const;
    QIcon categoryIcon() const;

    virtual QWidget *createPage(QWidget *parent);
    virtual void apply();
    virtual void finish();

private:
    QDesignerOptionsPageInterface *m_designerPage;
    bool m_initialized;
};

class SettingsPageProvider : public Core::IOptionsPageProvider
{
    Q_OBJECT

public:
    SettingsPageProvider(QObject *parent = 0);
    ~SettingsPageProvider();

    QString category() const;
    QString displayCategory() const;
    QIcon categoryIcon() const;

    QList<Core::IOptionsPage *> pages() const;

private:
    mutable bool m_initialized;
};

} // namespace Internal
} // namespace Designer

#endif // DESIGNER_SETTINGSPAGE_H
