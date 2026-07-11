// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The module-wide log category. DECLARE here + DEFINE once in ReactiveUIToolchainModule.cpp —
// per-file DEFINE_LOG_CATEGORY_STATIC collides under unity builds (same pitfall as the shared
// UetkxChars.h constants).

#pragma once

#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRuiToolchain, Log, All);
