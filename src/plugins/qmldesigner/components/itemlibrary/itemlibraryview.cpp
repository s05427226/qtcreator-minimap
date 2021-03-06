/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include "itemlibraryview.h"
#include "itemlibrarywidget.h"
#include <import.h>

namespace QmlDesigner {

ItemLibraryView::ItemLibraryView(QObject* parent) : AbstractView(parent), m_widget(new ItemLibraryWidget)
{

}

ItemLibraryView::~ItemLibraryView()
{

}

ItemLibraryWidget *ItemLibraryView::widget()
{
    return m_widget.data();
}

void ItemLibraryView::modelAttached(Model *model)
{
    AbstractView::modelAttached(model);
    m_widget->setModel(model);
    updateImports();
}

void ItemLibraryView::modelAboutToBeDetached(Model *model)
{
    AbstractView::modelAboutToBeDetached(model);
    m_widget->setModel(0);
}

void ItemLibraryView::importsChanged(const QList<Import> &/*addedImports*/, const QList<Import> &/*removedImports*/)
{
    updateImports();
}

void ItemLibraryView::nodeCreated(const ModelNode &)
{

}

void ItemLibraryView::nodeRemoved(const ModelNode &, const NodeAbstractProperty &, PropertyChangeFlags)
{

}

void ItemLibraryView::propertiesRemoved(const QList<AbstractProperty> &)
{

}

void ItemLibraryView::variantPropertiesChanged(const QList<VariantProperty> &, PropertyChangeFlags)
{

}

void ItemLibraryView::bindingPropertiesChanged(const QList<BindingProperty> &, PropertyChangeFlags)
{

}

void ItemLibraryView::nodeAboutToBeRemoved(const ModelNode &)
{

}

void ItemLibraryView::nodeOrderChanged(const NodeListProperty &, const ModelNode &, int)
{

}

void ItemLibraryView::nodeAboutToBeReparented(const ModelNode &, const NodeAbstractProperty &, const NodeAbstractProperty &, AbstractView::PropertyChangeFlags)
{

}

void ItemLibraryView::nodeReparented(const ModelNode &, const NodeAbstractProperty &, const NodeAbstractProperty &, AbstractView::PropertyChangeFlags)
{

}

void ItemLibraryView::rootNodeTypeChanged(const QString &, int , int )
{

}

void ItemLibraryView::nodeIdChanged(const ModelNode &, const QString &, const QString& )
{

}

void ItemLibraryView::propertiesAboutToBeRemoved(const QList<AbstractProperty>& )
{

}

void ItemLibraryView::selectedNodesChanged(const QList<ModelNode> &,
                                  const QList<ModelNode> &)
{

}

void ItemLibraryView::auxiliaryDataChanged(const ModelNode &, const QString &, const QVariant &)
{

}

void ItemLibraryView::scriptFunctionsChanged(const ModelNode &, const QStringList &)
{

}

void ItemLibraryView::instancePropertyChange(const QList<QPair<ModelNode, QString> > &)
{

}

void ItemLibraryView::instancesCompleted(const QVector<ModelNode> &)
{

}

void ItemLibraryView::instanceInformationsChange(const QVector<ModelNode> &/*nodeList*/)
{
}

void ItemLibraryView::instancesRenderImageChanged(const QVector<ModelNode> &/*nodeList*/)
{
}

void ItemLibraryView::instancesPreviewImageChanged(const QVector<ModelNode> &/*nodeList*/)
{
}

void ItemLibraryView::instancesChildrenChanged(const QVector<ModelNode> &/*nodeList*/)
{

}

void ItemLibraryView::rewriterBeginTransaction()
{
}

void ItemLibraryView::rewriterEndTransaction()
{
}

void ItemLibraryView::actualStateChanged(const ModelNode &/*node*/)
{
}

void ItemLibraryView::updateImports()
{
    m_widget->updateModel();
}

} //QmlDesigner
