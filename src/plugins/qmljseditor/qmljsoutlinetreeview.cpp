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

#include "qmljsoutlinetreeview.h"
#include "qmloutlinemodel.h"

#include <utils/annotateditemdelegate.h>
#include <QtGui/QMenu>

namespace QmlJSEditor {
namespace Internal {

QmlJSOutlineTreeView::QmlJSOutlineTreeView(QWidget *parent) :
    Utils::NavigationTreeView(parent)
{
    // see also CppOutlineTreeView
    setFocusPolicy(Qt::NoFocus);
    setExpandsOnDoubleClick(false);

    setDragEnabled(true);
    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(InternalMove);

    setRootIsDecorated(false);

    Utils::AnnotatedItemDelegate *itemDelegate = new Utils::AnnotatedItemDelegate(this);
    itemDelegate->setDelimiter(QLatin1String(" "));
    itemDelegate->setAnnotationRole(QmlOutlineModel::AnnotationRole);
    setItemDelegateForColumn(0, itemDelegate);
}

void QmlJSOutlineTreeView::contextMenuEvent(QContextMenuEvent *event)
{
    if (!event)
        return;

    QMenu contextMenu;

    contextMenu.addAction(tr("Expand All"), this, SLOT(expandAll()));
    contextMenu.addAction(tr("Collapse All"), this, SLOT(collapseAllExceptRoot()));

    contextMenu.exec(event->globalPos());

    event->accept();
}

void QmlJSOutlineTreeView::collapseAllExceptRoot()
{
    if (!model())
        return;
    const QModelIndex rootElementIndex = model()->index(0, 0, rootIndex());
    int rowCount = model()->rowCount(rootElementIndex);
    for (int i = 0; i < rowCount; ++i) {
        collapse(model()->index(i, 0, rootElementIndex));
    }
}

} // namespace Internal
} // namespace QmlJSEditor
