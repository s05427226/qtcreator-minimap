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

#ifndef ANCHORLINECONTROLLER_H
#define ANCHORLINECONTROLLER_H

#include <QWeakPointer>
#include <QSharedPointer>
#include <qmlanchors.h>

namespace QmlDesigner {

class FormEditorItem;
class LayerItem;
class AnchorLineHandleItem;

class AnchorLineControllerData
{
public:
    AnchorLineControllerData(LayerItem *layerItem,
                         FormEditorItem *formEditorItem);
    AnchorLineControllerData(const AnchorLineControllerData &other);
    ~AnchorLineControllerData();

    QWeakPointer<LayerItem> layerItem;
    FormEditorItem *formEditorItem;

    AnchorLineHandleItem *topItem;
    AnchorLineHandleItem *leftItem;
    AnchorLineHandleItem *rightItem;
    AnchorLineHandleItem *bottomItem;
};


class AnchorLineController
{
 public:
    AnchorLineController();
    AnchorLineController(LayerItem *layerItem, FormEditorItem *formEditorItem);
    AnchorLineController(const QSharedPointer<AnchorLineControllerData> &data);

    void show(AnchorLine::Type anchorLineMask);
    void hide();

    void updatePosition();

    bool isValid() const;

    QWeakPointer<AnchorLineControllerData> weakPointer() const;


    FormEditorItem *formEditorItem() const;

    bool isTopHandle(const AnchorLineHandleItem *handle) const;
    bool isLeftHandle(const AnchorLineHandleItem *handle) const;
    bool isRightHandle(const AnchorLineHandleItem *handle) const;
    bool isBottomHandle(const AnchorLineHandleItem *handle) const;

    void clearHighlight();

private:
    QSharedPointer<AnchorLineControllerData> m_data;
};

} // namespace QmlDesigner

#endif // ANCHORLINECONTROLLER_H
