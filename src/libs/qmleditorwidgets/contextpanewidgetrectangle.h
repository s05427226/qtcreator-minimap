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

#ifndef CONTEXTPANEWIDGETRECTANGLE_H
#define CONTEXTPANEWIDGETRECTANGLE_H

#include <qmleditorwidgets_global.h>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class ContextPaneWidgetRectangle;
}
QT_END_NAMESPACE

namespace QmlJS {
    class PropertyReader;
}

namespace QmlEditorWidgets {

class QMLEDITORWIDGETS_EXPORT ContextPaneWidgetRectangle : public QWidget
{
    Q_OBJECT

public:
    explicit ContextPaneWidgetRectangle(QWidget *parent = 0);
    ~ContextPaneWidgetRectangle();
    void setProperties(QmlJS::PropertyReader *propertyReader);

public slots:
    void onBorderColorButtonToggled(bool);
    void onColorButtonToggled(bool);
    void onColorDialogApplied(const QColor &color);
    void onColorDialogCancled();
    void onGradientClicked();
    void onColorNoneClicked();
    void onColorSolidClicked();
    void onBorderNoneClicked();
    void onBorderSolidClicked();
    void onGradientLineDoubleClicked(const QPoint &);
    void onUpdateGradient();

signals:
    void propertyChanged(const QString &, const QVariant &);
    void removeProperty(const QString &);
    void removeAndChangeProperty(const QString &, const QString &, const QVariant &, bool removeFirst);

protected:
    void changeEvent(QEvent *e);
    void timerEvent(QTimerEvent *event);

private:
    void setColor();
    Ui::ContextPaneWidgetRectangle *ui;
    bool m_hasBorder;
    bool m_hasGradient;
    bool m_none;
    bool m_gradientLineDoubleClicked;
    int m_gradientTimer;
};

} //QmlDesigner

#endif // CONTEXTPANEWIDGETRECTANGLE_H
