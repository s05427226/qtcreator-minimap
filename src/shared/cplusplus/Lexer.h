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
// Copyright (c) 2008 Roberto Raggi <roberto.raggi@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef CPLUSPLUS_LEXER_H
#define CPLUSPLUS_LEXER_H

#include "CPlusPlusForwardDeclarations.h"
#include "Token.h"


namespace CPlusPlus {

class CPLUSPLUS_EXPORT Lexer
{
    Lexer(const Lexer &other);
    void operator =(const Lexer &other);

public:
    enum State {
        State_Default,
        State_MultiLineComment,
        State_MultiLineDoxyComment
    };

    Lexer(TranslationUnit *unit);
    Lexer(const char *firstChar, const char *lastChar);
    ~Lexer();

    Control *control() const;
    TranslationUnit *translationUnit() const;

    bool qtMocRunEnabled() const;
    void setQtMocRunEnabled(bool onoff);

    bool cxx0xEnabled() const;
    void setCxxOxEnabled(bool onoff);

    bool objCEnabled() const;
    void setObjCEnabled(bool onoff);

    void scan(Token *tok);

    inline void operator()(Token *tok)
    { scan(tok); }

    unsigned tokenOffset() const;
    unsigned tokenLength() const;
    const char *tokenBegin() const;
    const char *tokenEnd() const;
    unsigned currentLine() const;

    bool scanCommentTokens() const;
    void setScanCommentTokens(bool onoff);

    bool scanKeywords() const;
    void setScanKeywords(bool onoff);

    bool scanAngleStringLiteralTokens() const;
    void setScanAngleStringLiteralTokens(bool onoff);

    void setStartWithNewline(bool enabled);

    int state() const;
    void setState(int state);

    bool isIncremental() const;
    void setIncremental(bool isIncremental);

private:
    void scan_helper(Token *tok);
    void setSource(const char *firstChar, const char *lastChar);
    static int classify(const char *string, int length, bool q, bool cxx0x);
    static int classifyObjCAtKeyword(const char *s, int n);
    static int classifyOperator(const char *string, int length);

    inline void yyinp()
    {
        if (++_currentChar == _lastChar)
            _yychar = 0;
        else {
            _yychar = *_currentChar;
            if (_yychar == '\n')
                pushLineStartOffset();
        }
    }

    void pushLineStartOffset();

private:
    struct Flags {
        unsigned _isIncremental: 1;
        unsigned _scanCommentTokens: 1;
        unsigned _scanKeywords: 1;
        unsigned _scanAngleStringLiteralTokens: 1;
        unsigned _qtMocRunEnabled: 1;
        unsigned _cxx0xEnabled: 1;
        unsigned _objCEnabled: 1;
    };

    TranslationUnit *_translationUnit;
    const char *_firstChar;
    const char *_currentChar;
    const char *_lastChar;
    const char *_tokenStart;
    unsigned char _yychar;
    int _state;
    union {
        unsigned _flags;
        Flags f;
    };
    unsigned _currentLine;
};

} // namespace CPlusPlus


#endif // CPLUSPLUS_LEXER_H
