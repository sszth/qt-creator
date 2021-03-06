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

#include "clangassistproposalitem.h"

#include "clangcompletionchunkstotextconverter.h"
#include "clangfixitoperation.h"
#include "clangutils.h"

#include <cplusplus/Icons.h>
#include <cplusplus/MatchingText.h>
#include <cplusplus/SimpleLexer.h>
#include <cplusplus/Token.h>

#include <texteditor/completionsettings.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditorsettings.h>

#include <QCoreApplication>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <utils/algorithm.h>
#include <utils/textutils.h>
#include <utils/qtcassert.h>

using namespace CPlusPlus;
using namespace ClangBackEnd;
using namespace TextEditor;

namespace ClangCodeModel {
namespace Internal {

bool ClangAssistProposalItem::prematurelyApplies(const QChar &typedCharacter) const
{
    bool applies = false;

    if (m_completionOperator == T_SIGNAL || m_completionOperator == T_SLOT)
        applies = QString::fromLatin1("(,").contains(typedCharacter);
    else if (m_completionOperator == T_STRING_LITERAL || m_completionOperator == T_ANGLE_STRING_LITERAL)
        applies = (typedCharacter == QLatin1Char('/')) && text().endsWith(QLatin1Char('/'));
    else if (firstCodeCompletion().completionKind == CodeCompletion::ObjCMessageCompletionKind)
        applies = QString::fromLatin1(";.,").contains(typedCharacter);
    else
        applies = QString::fromLatin1(";.,:(").contains(typedCharacter);

    if (applies)
        m_typedCharacter = typedCharacter;

    return applies;
}

bool ClangAssistProposalItem::implicitlyApplies() const
{
    return true;
}

static QString textUntilPreviousStatement(TextDocumentManipulatorInterface &manipulator,
                                          int startPosition)
{
    static const QString stopCharacters(";{}#");

    int endPosition = 0;
    for (int i = startPosition; i >= 0 ; --i) {
        if (stopCharacters.contains(manipulator.characterAt(i))) {
            endPosition = i + 1;
            break;
        }
    }

    return manipulator.textAt(endPosition, startPosition - endPosition);
}

// 7.3.3: using typename(opt) nested-name-specifier unqualified-id ;
static bool isAtUsingDeclaration(TextDocumentManipulatorInterface &manipulator,
                                 int basePosition)
{
    SimpleLexer lexer;
    lexer.setLanguageFeatures(LanguageFeatures::defaultFeatures());
    const QString textToLex = textUntilPreviousStatement(manipulator, basePosition);
    const Tokens tokens = lexer(textToLex);
    if (tokens.empty())
        return false;

    // The nested-name-specifier always ends with "::", so check for this first.
    const Token lastToken = tokens[tokens.size() - 1];
    if (lastToken.kind() != T_COLON_COLON)
        return false;

    return ::Utils::contains(tokens, [](const Token &token) {
        return token.kind() == T_USING;
    });
}

static QString methodDefinitionParameters(const CodeCompletionChunks &chunks)
{
    QString result;

    auto typedTextChunkIt = std::find_if(chunks.begin(), chunks.end(),
                                         [](const CodeCompletionChunk &chunk) {
        return chunk.kind == CodeCompletionChunk::TypedText;
    });
    if (typedTextChunkIt == chunks.end())
        return result;

    std::for_each(++typedTextChunkIt, chunks.end(), [&result](const CodeCompletionChunk &chunk) {
        if (chunk.kind == CodeCompletionChunk::Placeholder && chunk.text.contains('=')) {
            Utf8String text = chunk.text.mid(0, chunk.text.indexOf('='));
            if (text.endsWith(' '))
                text.chop(1);
            result += text;
        } else {
            result += chunk.text;
        }
    });

    return result;
}

void ClangAssistProposalItem::apply(TextDocumentManipulatorInterface &manipulator,
                                    int basePosition) const
{
    const CodeCompletion ccr = firstCodeCompletion();

    if (!ccr.requiredFixIts.empty()) {
        // Important: Calculate shift before changing the document.
        basePosition += fixItsShift(manipulator);

        ClangFixItOperation fixItOperation(Utf8String(), ccr.requiredFixIts);
        fixItOperation.perform();
    }

    QString textToBeInserted = m_text;
    QString extraCharacters;
    int extraLength = 0;
    int cursorOffset = 0;
    bool setAutoCompleteSkipPos = false;
    int currentPosition = manipulator.currentPosition();

    if (m_completionOperator == T_SIGNAL || m_completionOperator == T_SLOT) {
        extraCharacters += QLatin1Char(')');
        if (m_typedCharacter == QLatin1Char('(')) // Eat the opening parenthesis
            m_typedCharacter = QChar();
    } else if (ccr.completionKind == CodeCompletion::KeywordCompletionKind) {
        CompletionChunksToTextConverter converter;
        converter.setupForKeywords();

        converter.parseChunks(ccr.chunks);

        textToBeInserted = converter.text();
        if (converter.hasPlaceholderPositions())
            cursorOffset = converter.placeholderPositions().at(0) - converter.text().size();
    } else if (ccr.completionKind == CodeCompletion::NamespaceCompletionKind) {
        CompletionChunksToTextConverter converter;

        converter.parseChunks(ccr.chunks); // Appends "::" after name space name

        textToBeInserted = converter.text();
    } else if (!ccr.text.isEmpty()) {
        const CompletionSettings &completionSettings =
                TextEditorSettings::instance()->completionSettings();
        const bool autoInsertBrackets = completionSettings.m_autoInsertBrackets;

        if (autoInsertBrackets &&
                (ccr.completionKind == CodeCompletion::FunctionCompletionKind
                 || ccr.completionKind == CodeCompletion::FunctionDefinitionCompletionKind
                 || ccr.completionKind == CodeCompletion::DestructorCompletionKind
                 || ccr.completionKind == CodeCompletion::ConstructorCompletionKind
                 || ccr.completionKind == CodeCompletion::SignalCompletionKind
                 || ccr.completionKind == CodeCompletion::SlotCompletionKind)) {
            // When the user typed the opening parenthesis, he'll likely also type the closing one,
            // in which case it would be annoying if we put the cursor after the already automatically
            // inserted closing parenthesis.
            const bool skipClosingParenthesis = m_typedCharacter != QLatin1Char('(');
            QTextCursor cursor = manipulator.textCursorAt(basePosition);

            bool abandonParen = false;
            if (::Utils::Text::matchPreviousWord(manipulator, cursor, "&")) {
                ::Utils::Text::moveToPrevChar(manipulator, cursor);
                ::Utils::Text::moveToPrevChar(manipulator, cursor);
                const QChar prevChar = manipulator.characterAt(cursor.position());
                cursor.setPosition(basePosition);
                abandonParen = QString("(;,{}").contains(prevChar);
            }
            if (!abandonParen)
                abandonParen = isAtUsingDeclaration(manipulator, basePosition);

            if (!abandonParen && ccr.completionKind == CodeCompletion::FunctionDefinitionCompletionKind) {
                const CodeCompletionChunk resultType = ccr.chunks.first();
                if (resultType.kind == CodeCompletionChunk::ResultType) {
                    if (::Utils::Text::matchPreviousWord(manipulator, cursor, resultType.text.toString())) {
                        extraCharacters += methodDefinitionParameters(ccr.chunks);
                        // To skip the next block.
                        abandonParen = true;
                    }
                } else {
                    // Do nothing becasue it's not a function definition.

                    // It's a clang bug that the function might miss a ResultType chunk
                    // when the base class method is called from the overriding method
                    // of the derived class. For example:
                    // void Derived::foo() override { Base::<complete here> }
                }
            }
            if (!abandonParen) {
                if (completionSettings.m_spaceAfterFunctionName)
                    extraCharacters += QLatin1Char(' ');
                extraCharacters += QLatin1Char('(');
                if (m_typedCharacter == QLatin1Char('('))
                    m_typedCharacter = QChar();

                // If the function doesn't return anything, automatically place the semicolon,
                // unless we're doing a scope completion (then it might be function definition).
                const QChar characterAtCursor = manipulator.characterAt(currentPosition);
                bool endWithSemicolon = m_typedCharacter == QLatin1Char(';')/*
                                                || (function->returnType()->isVoidType() && m_completionOperator != T_COLON_COLON)*/; //###
                const QChar semicolon = m_typedCharacter.isNull() ? QLatin1Char(';') : m_typedCharacter;

                if (endWithSemicolon && characterAtCursor == semicolon) {
                    endWithSemicolon = false;
                    m_typedCharacter = QChar();
                }

                // If the function takes no arguments, automatically place the closing parenthesis
                if (!hasOverloadsWithParameters() && !ccr.hasParameters && skipClosingParenthesis) {
                    extraCharacters += QLatin1Char(')');
                    if (endWithSemicolon) {
                        extraCharacters += semicolon;
                        m_typedCharacter = QChar();
                    }
                } else {
                    const QChar lookAhead = manipulator.characterAt(manipulator.currentPosition() + 1);
                    if (MatchingText::shouldInsertMatchingText(lookAhead)) {
                        extraCharacters += QLatin1Char(')');
                        --cursorOffset;
                        setAutoCompleteSkipPos = true;
                        if (endWithSemicolon) {
                            extraCharacters += semicolon;
                            --cursorOffset;
                            m_typedCharacter = QChar();
                        }
                    }
                }
            }
        }
    }

    // Append an unhandled typed character, adjusting cursor offset when it had been adjusted before
    if (!m_typedCharacter.isNull()) {
        extraCharacters += m_typedCharacter;
        if (cursorOffset != 0)
            --cursorOffset;
    }

    // Avoid inserting characters that are already there
    QTextCursor cursor = manipulator.textCursorAt(basePosition);
    cursor.movePosition(QTextCursor::EndOfWord);
    const QString textAfterCursor = manipulator.textAt(currentPosition,
                                                       cursor.position() - currentPosition);

    if (textToBeInserted != textAfterCursor
            && textToBeInserted.indexOf(textAfterCursor, currentPosition - basePosition) >= 0) {
        currentPosition = cursor.position();
    }

    for (int i = 0; i < extraCharacters.length(); ++i) {
        const QChar a = extraCharacters.at(i);
        const QChar b = manipulator.characterAt(currentPosition + i);
        if (a == b)
            ++extraLength;
        else
            break;
    }

    textToBeInserted += extraCharacters;

    const int length = currentPosition - basePosition + extraLength;

    const bool isReplaced = manipulator.replace(basePosition, length, textToBeInserted);
    manipulator.setCursorPosition(basePosition + textToBeInserted.length());
    if (isReplaced) {
        if (cursorOffset)
            manipulator.setCursorPosition(manipulator.currentPosition() + cursorOffset);
        if (setAutoCompleteSkipPos)
            manipulator.setAutoCompleteSkipPosition(manipulator.currentPosition());

        if (ccr.completionKind == CodeCompletion::KeywordCompletionKind)
            manipulator.autoIndent(basePosition, textToBeInserted.size());
    }
}

void ClangAssistProposalItem::setText(const QString &text)
{
    m_text = text;
}

QString ClangAssistProposalItem::text() const
{
    return m_text;
}

const QVector<ClangBackEnd::FixItContainer> &ClangAssistProposalItem::firstCompletionFixIts() const
{
    return firstCodeCompletion().requiredFixIts;
}

std::pair<int, int> fixItPositionsRange(const FixItContainer &fixIt, const QTextCursor &cursor)
{
    const QTextBlock startLine = cursor.document()->findBlockByNumber(fixIt.range.start.line - 1);
    const QTextBlock endLine = cursor.document()->findBlockByNumber(fixIt.range.end.line - 1);

    const int fixItStartPos = ::Utils::Text::positionInText(
                cursor.document(),
                static_cast<int>(fixIt.range.start.line),
                Utils::cppEditorColumn(startLine, static_cast<int>(fixIt.range.start.column)));
    const int fixItEndPos = ::Utils::Text::positionInText(
                cursor.document(),
                static_cast<int>(fixIt.range.end.line),
                Utils::cppEditorColumn(endLine, static_cast<int>(fixIt.range.end.column)));
    return std::make_pair(fixItStartPos, fixItEndPos);
}

static QString textReplacedByFixit(const FixItContainer &fixIt)
{
    TextEditorWidget *textEditorWidget = TextEditorWidget::currentTextEditorWidget();
    if (!textEditorWidget)
        return QString();
    const std::pair<int, int> fixItPosRange = fixItPositionsRange(fixIt,
                                                                  textEditorWidget->textCursor());
    return textEditorWidget->textAt(fixItPosRange.first,
                                    fixItPosRange.second - fixItPosRange.first);
}

QString ClangAssistProposalItem::fixItText() const
{
    const FixItContainer &fixIt = firstCompletionFixIts().first();
    return QCoreApplication::translate("ClangCodeModel::ClangAssistProposalItem",
                                       "Requires to correct \"%1\" to \"%2\"")
            .arg(textReplacedByFixit(fixIt))
            .arg(fixIt.text.toString());
}

int ClangAssistProposalItem::fixItsShift(const TextDocumentManipulatorInterface &manipulator) const
{
    const QVector<ClangBackEnd::FixItContainer> &requiredFixIts = firstCompletionFixIts();
    if (requiredFixIts.empty())
        return 0;

    int shift = 0;
    const QTextCursor cursor = manipulator.textCursorAt(0);
    for (const FixItContainer &fixIt : requiredFixIts) {
        const std::pair<int, int> fixItPosRange = fixItPositionsRange(fixIt, cursor);
        shift += fixIt.text.toString().length() - (fixItPosRange.second - fixItPosRange.first);
    }
    return shift;
}

QIcon ClangAssistProposalItem::icon() const
{
    using CPlusPlus::Icons;
    static const char SNIPPET_ICON_PATH[] = ":/texteditor/images/snippet.png";
    static const QIcon snippetIcon = QIcon(QLatin1String(SNIPPET_ICON_PATH));

    const ClangBackEnd::CodeCompletion &completion = firstCodeCompletion();
    switch (completion.completionKind) {
        case CodeCompletion::ClassCompletionKind:
        case CodeCompletion::TemplateClassCompletionKind:
        case CodeCompletion::TypeAliasCompletionKind:
            return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::Class);
        case CodeCompletion::EnumerationCompletionKind:
            return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::Enum);
        case CodeCompletion::EnumeratorCompletionKind:
            return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::Enumerator);
        case CodeCompletion::ConstructorCompletionKind:
        case CodeCompletion::DestructorCompletionKind:
        case CodeCompletion::FunctionCompletionKind:
        case CodeCompletion::FunctionDefinitionCompletionKind:
        case CodeCompletion::TemplateFunctionCompletionKind:
        case CodeCompletion::ObjCMessageCompletionKind:
            switch (completion.availability) {
                case CodeCompletion::Available:
                case CodeCompletion::Deprecated:
                    return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::FuncPublic);
                default:
                    return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::FuncPrivate);
            }
        case CodeCompletion::SignalCompletionKind:
            return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::Signal);
        case CodeCompletion::SlotCompletionKind:
            switch (completion.availability) {
                case CodeCompletion::Available:
                case CodeCompletion::Deprecated:
                    return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::SlotPublic);
                case CodeCompletion::NotAccessible:
                case CodeCompletion::NotAvailable:
                    return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::SlotPrivate);
            }
            break;
        case CodeCompletion::NamespaceCompletionKind:
            return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::Namespace);
        case CodeCompletion::PreProcessorCompletionKind:
            return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::Macro);
        case CodeCompletion::VariableCompletionKind:
            switch (completion.availability) {
                case CodeCompletion::Available:
                case CodeCompletion::Deprecated:
                    return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::VarPublic);
                default:
                    return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::VarPrivate);
            }
        case CodeCompletion::KeywordCompletionKind:
            return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::Keyword);
        case CodeCompletion::ClangSnippetKind:
            return snippetIcon;
        case CodeCompletion::Other:
            return ::Utils::CodeModelIcon::iconForType(::Utils::CodeModelIcon::Unknown);
        default:
            break;
    }

    return QIcon();
}

QString ClangAssistProposalItem::detail() const
{
    QString detail;
    for (const ClangBackEnd::CodeCompletion &codeCompletion : m_codeCompletions) {
        if (!detail.isEmpty())
            detail += "<br>";
        detail += CompletionChunksToTextConverter::convertToToolTipWithHtml(
                    codeCompletion.chunks, codeCompletion.completionKind);

        if (!codeCompletion.briefComment.isEmpty())
            detail += "<br>" + codeCompletion.briefComment.toString();
    }

    if (requiresFixIts())
        detail += "<br><br><b>" + fixItText() + "</b>";

    return detail;
}

bool ClangAssistProposalItem::isSnippet() const
{
    return false;
}

bool ClangAssistProposalItem::isValid() const
{
    return true;
}

quint64 ClangAssistProposalItem::hash() const
{
    return 0;
}

bool ClangAssistProposalItem::requiresFixIts() const
{
    return !firstCompletionFixIts().empty();
}

bool ClangAssistProposalItem::hasOverloadsWithParameters() const
{
    return m_hasOverloadsWithParameters;
}

void ClangAssistProposalItem::setHasOverloadsWithParameters(bool hasOverloadsWithParameters)
{
    m_hasOverloadsWithParameters = hasOverloadsWithParameters;
}

void ClangAssistProposalItem::keepCompletionOperator(unsigned compOp)
{
    m_completionOperator = compOp;
}

void ClangAssistProposalItem::appendCodeCompletion(const CodeCompletion &codeCompletion)
{
    m_codeCompletions.push_back(codeCompletion);
}

const ClangBackEnd::CodeCompletion &ClangAssistProposalItem::firstCodeCompletion() const
{
    return m_codeCompletions.at(0);
}

void ClangAssistProposalItem::removeFirstCodeCompletion()
{
    QTC_ASSERT(!m_codeCompletions.empty(), return;);
    m_codeCompletions.erase(m_codeCompletions.begin());
}

} // namespace Internal
} // namespace ClangCodeModel
