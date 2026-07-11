// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Shared code-point constants for the .uetkx toolchain TUs (a named namespace — anonymous
// namespaces collide under unity builds).

#pragma once

#include "CoreMinimal.h"

namespace UetkxChars
{
	constexpr int32 C_NL = '\n';
	constexpr int32 C_CR = '\r';
	constexpr int32 C_TAB = '\t';
	constexpr int32 C_SPACE = ' ';
	constexpr int32 C_QUOTE = '"';
	constexpr int32 C_APOS = '\'';
	constexpr int32 C_HASH = '#';
	constexpr int32 C_SLASH = '/';
	constexpr int32 C_STAR = '*';
	constexpr int32 C_BSLASH = '\\';
	constexpr int32 C_LT = '<';
	constexpr int32 C_GT = '>';
	constexpr int32 C_BANG = '!';
	constexpr int32 C_DASH = '-';
	constexpr int32 C_DOT = '.';
	constexpr int32 C_AT = '@';
	constexpr int32 C_EQ = '=';
	constexpr int32 C_PLUS = '+';
	constexpr int32 C_PERCENT = '%';
	constexpr int32 C_AMP = '&';
	constexpr int32 C_PIPE = '|';
	constexpr int32 C_CARET = '^';
	constexpr int32 C_COLON = ':';
	constexpr int32 C_QUESTION = '?';
	constexpr int32 C_COMMA = ',';
	constexpr int32 C_LPAREN = '(';
	constexpr int32 C_RPAREN = ')';
	constexpr int32 C_LBRACE = '{';
	constexpr int32 C_RBRACE = '}';
	constexpr int32 C_LBRACKET = '[';
	constexpr int32 C_RBRACKET = ']';
} // namespace UetkxChars
