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

#ifndef TEXTEDITORQUICKFIX_H
#define TEXTEDITORQUICKFIX_H

#include "texteditor_global.h"
#include <utils/changeset.h>

#include <QtCore/QSharedPointer>
#include <QtGui/QTextCursor>

namespace TextEditor {

class BaseTextEditor;
class QuickFixOperation;

class TEXTEDITOR_EXPORT QuickFixState
{
    Q_DISABLE_COPY(QuickFixState)

public:
    QuickFixState() {}
    virtual ~QuickFixState() {}
};

class TEXTEDITOR_EXPORT QuickFixOperation
{
    Q_DISABLE_COPY(QuickFixOperation)

public:
    typedef QSharedPointer<QuickFixOperation> Ptr;

    struct Range {
        Range() {}
        Range(const QTextCursor &tc): begin(tc), end(tc) {}

        QTextCursor begin;
        QTextCursor end;
    };

public:
    QuickFixOperation(TextEditor::BaseTextEditor *editor);
    virtual ~QuickFixOperation();

    virtual QString description() const = 0;
    virtual void createChangeSet() = 0;
    virtual Range topLevelRange() const = 0;

    virtual int match(QuickFixState *state) = 0;

    void apply();

    TextEditor::BaseTextEditor *editor() const;

    QTextCursor textCursor() const;
    void setTextCursor(const QTextCursor &cursor);

    void reindent(const Range &range);

    int selectionStart() const;
    int selectionEnd() const;

    int position(int line, int column) const;
    Range range(int from, int to) const;

    void move(int start, int end, int to);
    void replace(int start, int end, const QString &replacement);
    void insert(int at, const QString &text);
    void remove(int start, int end);
    void flip(int start1, int end1, int start2, int end2);
    void copy(int start, int end, int to);

    QChar charAt(int offset) const;
    QString textOf(int start, int end) const;

    const Utils::ChangeSet &changeSet() const;

private:
    TextEditor::BaseTextEditor *_editor;
    QTextCursor _textCursor;
    Utils::ChangeSet _changeSet;
};

} // end of namespace TextEditor

#endif // TEXTEDITORQUICKFIX_H
