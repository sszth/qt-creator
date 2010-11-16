/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
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
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "s60createpackageparser.h"

#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/taskwindow.h>

using namespace Qt4ProjectManager::Internal;

S60CreatePackageParser::S60CreatePackageParser(const QString &packageName) :
    m_packageName(packageName),
    m_needPassphrase(false)
{
    setObjectName(QLatin1String("S60CreatePackageParser"));
    m_signSis.setPattern("^(error:\\s?)+(.+)$");
    m_signSis.setMinimal(true);
}

bool S60CreatePackageParser::parseLine(const QString &line)
{
    if (line.startsWith("Patching: ")) {
        m_patchingLines.append(line.mid(10).trimmed());
        return true;
    }
    if (!m_patchingLines.isEmpty()) {
        emit packageWasPatched(m_packageName, m_patchingLines);

        QString lines = m_patchingLines.join("\n");
        m_patchingLines.clear();
        //: %1 package name, %2 will be replaced by a list of patching lines.
        QString message = tr("The binary package '%1' was patched to be installable after being self-signed!\n%2\n"
                             "Use a developer certificate or any other signing option to prevent "
                             "this patching from happening.").
                arg(m_packageName, lines);
        ProjectExplorer::Task task(ProjectExplorer::Task::Warning, message, QString(), -1,
                                   ProjectExplorer::Constants::TASK_CATEGORY_BUILDSYSTEM);

        QTextLayout::FormatRange fr;
        fr.start = message.indexOf(lines);
        fr.length = lines.length();
        fr.format.setFontItalic(true);
        task.formats.append(fr);

        emit addTask(task);
    }

    if (m_signSis.indexIn(line) > -1) {
        if (m_signSis.cap(2).contains(QLatin1String("bad password"))
            || m_signSis.cap(2).contains(QLatin1String("bad decrypt")))
            m_needPassphrase = true;
        else
            emit addTask(ProjectExplorer::Task(ProjectExplorer::Task::Error, m_signSis.cap(2), QString(), -1,
                                               ProjectExplorer::Constants::TASK_CATEGORY_BUILDSYSTEM));
        return true;
    }
    return false;
}

void S60CreatePackageParser::stdOutput(const QString &line)
{
    if (!parseLine(line))
        IOutputParser::stdOutput(line);
}

void S60CreatePackageParser::stdError(const QString &line)
{
    if (!parseLine(line))
        IOutputParser::stdError(line);
}

bool S60CreatePackageParser::needPassphrase() const
{
    return m_needPassphrase;
}

/* STDOUT:
make[1]: Entering directory `C:/temp/test/untitled131'
createpackage.bat -g  untitled131_template.pkg RELEASE-armv5
Auto-patching capabilities for self signed package.

Patching package file and relevant binaries...
Patching: Removed dependency to qt.sis (0x2001E61C) to avoid installation issues in case qt.sis is also patched.


NOTE: A patched package may not work as expected due to reduced capabilities and other modifications,
      so it should not be used for any kind of Symbian signing or distribution!
      Use a proper certificate to avoid the need to patch the package.

Processing untitled131_release-armv5.pkg...
*/
