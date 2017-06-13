/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "runnables.h"

#include <QTcpServer>

#include <utils/temporaryfile.h>

namespace ProjectExplorer {

bool operator==(const StandardRunnable &r1, const StandardRunnable &r2)
{
    return r1.executable == r2.executable
        && r1.commandLineArguments == r2.commandLineArguments
        && r1.workingDirectory == r2.workingDirectory
        && r1.environment == r2.environment;
}

void *StandardRunnable::staticTypeId = &StandardRunnable::staticTypeId;

UrlConnection UrlConnection::fromHost(const QString &host)
{
    UrlConnection connection;
    connection.setHost(host);
    return connection;
}

UrlConnection UrlConnection::localHostWithoutPort()
{
    QUrl serverUrl;
    QTcpServer server;
    serverUrl.setHost(server.serverAddress().toString());
    return UrlConnection(serverUrl);
}

UrlConnection UrlConnection::localHostAndFreePort()
{
    QUrl serverUrl;
    QTcpServer server;
    if (server.listen(QHostAddress::LocalHost) || server.listen(QHostAddress::LocalHostIPv6)) {
        serverUrl.setHost(server.serverAddress().toString());
        serverUrl.setPort(server.serverPort());
    }
    return UrlConnection(serverUrl);
}

UrlConnection UrlConnection::localSocket()
{
    QUrl serverUrl;
    serverUrl.setScheme(socketScheme());
    Utils::TemporaryFile file("qmlprofiler-freesocket");
    if (file.open())
        serverUrl.setPath(file.fileName());
    return UrlConnection(serverUrl);
}

} // namespace ProjectExplorer
