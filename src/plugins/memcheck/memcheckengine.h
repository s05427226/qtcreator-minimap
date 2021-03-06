/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Author: Nicolas Arnaud-Cormos, KDAB (nicolas.arnaud-cormos@kdab.com)
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

#ifndef MEMCHECKENGINE_H
#define MEMCHECKENGINE_H

#include <valgrind/memcheck/memcheckrunner.h>
#include <valgrind/xmlprotocol/threadedparser.h>

#include <valgrindtoolbase/valgrindengine.h>

namespace Analyzer {
namespace Internal {

class MemcheckEngine : public ValgrindEngine
{
    Q_OBJECT
public:
    explicit MemcheckEngine(ProjectExplorer::RunConfiguration *runConfiguration);

    void start();
    void stop();

    QStringList suppressionFiles() const;

signals:
    void internalParserError(const QString &errorString);
    void parserError(const Valgrind::XmlProtocol::Error &error);
    void suppressionCount(const QString &name, qint64 count);

private slots:
    void receiveLogMessage(const QByteArray &);
    void status(const Valgrind::XmlProtocol::Status &status);

private:
    QString progressTitle() const;
    QStringList toolArguments() const;
    Valgrind::ValgrindRunner *runner();

    Valgrind::XmlProtocol::ThreadedParser m_parser;
    Valgrind::Memcheck::MemcheckRunner m_runner;
};

} // namespace Internal
} // namespace Analyzer

#endif // MEMCHECKENGINE_H
