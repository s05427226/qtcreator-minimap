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

#include "graphicsobjectnodeinstance.h"

#include "invalidnodeinstanceexception.h"
#include <QGraphicsObject>
#include "private/qgraphicsitem_p.h"
#include <QStyleOptionGraphicsItem>
#include "nodemetainfo.h"
#include <QPixmap>
#include <QSizeF>

namespace QmlDesigner {
namespace Internal {

GraphicsObjectNodeInstance::GraphicsObjectNodeInstance(QGraphicsObject *graphicsObject)
   : ObjectNodeInstance(graphicsObject),
   m_isMovable(true)
{
}

QGraphicsObject *GraphicsObjectNodeInstance::graphicsObject() const
{
    if (object() == 0)
        return 0;

    Q_ASSERT(qobject_cast<QGraphicsObject*>(object()));
    return static_cast<QGraphicsObject*>(object());
}

bool GraphicsObjectNodeInstance::hasContent() const
{
    return m_hasContent;
}

QList<ServerNodeInstance> GraphicsObjectNodeInstance::childItems() const
{
    QList<ServerNodeInstance> instanceList;
    foreach(QGraphicsItem *item, graphicsObject()->childItems())
    {
        QGraphicsObject *childObject = item->toGraphicsObject();
        if (childObject && nodeInstanceServer()->hasInstanceForObject(childObject)) {
            instanceList.append(nodeInstanceServer()->instanceForObject(childObject));
        }
    }

    return instanceList;
}

void GraphicsObjectNodeInstance::setHasContent(bool hasContent)
{
    m_hasContent = hasContent;
}

QPointF GraphicsObjectNodeInstance::position() const
{
    return graphicsObject()->pos();
}

QSizeF GraphicsObjectNodeInstance::size() const
{
    return graphicsObject()->boundingRect().size();
}

QTransform GraphicsObjectNodeInstance::transform() const
{
    if (!nodeInstanceServer()->hasInstanceForObject(object()))
        return sceneTransform();

    ServerNodeInstance nodeInstanceParent = nodeInstanceServer()->instanceForObject(object()).parent();

    if (!nodeInstanceParent.isValid())
        return sceneTransform();

    QGraphicsObject *graphicsObjectParent = qobject_cast<QGraphicsObject*>(nodeInstanceParent.internalObject());
    if (graphicsObjectParent)
        return graphicsObject()->itemTransform(graphicsObjectParent);
    else
        return sceneTransform();
}

QTransform GraphicsObjectNodeInstance::customTransform() const
{
    return graphicsObject()->transform();
}

QTransform GraphicsObjectNodeInstance::sceneTransform() const
{
    return graphicsObject()->sceneTransform();
}

double GraphicsObjectNodeInstance::rotation() const
{
    return graphicsObject()->rotation();
}

double GraphicsObjectNodeInstance::scale() const
{
    return graphicsObject()->scale();
}

QList<QGraphicsTransform *> GraphicsObjectNodeInstance::transformations() const
{
    return graphicsObject()->transformations();
}

QPointF GraphicsObjectNodeInstance::transformOriginPoint() const
{
    return graphicsObject()->transformOriginPoint();
}

double GraphicsObjectNodeInstance::zValue() const
{
    return graphicsObject()->zValue();
}

double GraphicsObjectNodeInstance::opacity() const
{
    return graphicsObject()->opacity();
}

QObject *GraphicsObjectNodeInstance::parent() const
{
    if (!graphicsObject() || !graphicsObject()->parentItem())
        return 0;

    return graphicsObject()->parentItem()->toGraphicsObject();
}

QRectF GraphicsObjectNodeInstance::boundingRect() const
{
    return graphicsObject()->boundingRect();
}

bool GraphicsObjectNodeInstance::isGraphicsObject() const
{
    return true;
}

void GraphicsObjectNodeInstance::setPropertyVariant(const QString &name, const QVariant &value)
{
    ObjectNodeInstance::setPropertyVariant(name, value);
}

void GraphicsObjectNodeInstance::setPropertyBinding(const QString &name, const QString &expression)
{
    ObjectNodeInstance::setPropertyBinding(name, expression);
}

QVariant GraphicsObjectNodeInstance::property(const QString &name) const
{
    return ObjectNodeInstance::property(name);
}

bool GraphicsObjectNodeInstance::equalGraphicsItem(QGraphicsItem *item) const
{
    return item == graphicsObject();
}

void initOption(QGraphicsItem *item, QStyleOptionGraphicsItem *option, const QTransform &transform)
{
    QGraphicsItemPrivate *privateItem = QGraphicsItemPrivate::get(item);
    privateItem->initStyleOption(option, transform, QRegion());
}

QImage GraphicsObjectNodeInstance::renderImage() const
{
    QRectF boundingRect = graphicsObject()->boundingRect();
    QSize boundingSize = boundingRect.size().toSize();

    QImage image(boundingSize, QImage::Format_ARGB32);

    if (image.isNull())
        return image;

    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.translate(-boundingRect.topLeft());

    if (hasContent()) {
        QStyleOptionGraphicsItem option;
        initOption(graphicsObject(), &option, painter.transform());
        graphicsObject()->paint(&painter, &option);

    }

    foreach(QGraphicsItem *graphicsItem, graphicsObject()->childItems())
        paintRecursively(graphicsItem, &painter);

    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

void GraphicsObjectNodeInstance::paintRecursively(QGraphicsItem *graphicsItem, QPainter *painter) const
{
    QGraphicsObject *graphicsObject = graphicsItem->toGraphicsObject();
    if (graphicsObject) {
        if (nodeInstanceServer()->hasInstanceForObject(graphicsObject))
            return; //we already keep track of this object elsewhere
    }

    if (graphicsItem->isVisible()) {
        painter->save();
        painter->setTransform(graphicsItem->itemTransform(graphicsItem->parentItem()), true);
        painter->setOpacity(graphicsItem->opacity() * painter->opacity());
        QStyleOptionGraphicsItem option;
        initOption(graphicsItem, &option, painter->transform());
        graphicsItem->paint(painter, &option);
        foreach(QGraphicsItem *childItem, graphicsItem->childItems()) {
            paintRecursively(childItem, painter);
        }
        painter->restore();
    }
}

void GraphicsObjectNodeInstance::paintUpdate()
{
    graphicsObject()->update();
}

bool GraphicsObjectNodeInstance::isMovable() const
{
    return m_isMovable && graphicsObject() && graphicsObject()->parentItem();
}

void GraphicsObjectNodeInstance::setMovable(bool movable)
{
    m_isMovable = movable;
}

} // namespace Internal
} // namespace QmlDesigner
