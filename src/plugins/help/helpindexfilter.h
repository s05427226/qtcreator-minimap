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

#ifndef HELPINDEXFILTER_H
#define HELPINDEXFILTER_H

#include <locator/ilocatorfilter.h>

#include <QtGui/QIcon>

namespace Help {
namespace Internal {

class HelpIndexFilter : public Locator::ILocatorFilter
{
    Q_OBJECT

public:
    HelpIndexFilter();
    ~HelpIndexFilter();

    // ILocatorFilter
    QString displayName() const;
    QString id() const;
    Priority priority() const;
    QList<Locator::FilterEntry> matchesFor(QFutureInterface<Locator::FilterEntry> &future, const QString &entry);
    void accept(Locator::FilterEntry selection) const;
    void refresh(QFutureInterface<void> &future);

signals:
    void linkActivated(const QUrl &link) const;

private:
    QIcon m_icon;
};

} // namespace Internal
} // namespace Help

#endif // HELPINDEXFILTER_H
