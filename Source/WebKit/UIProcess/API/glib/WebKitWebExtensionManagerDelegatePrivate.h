/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebKitWebExtensionManager.h"

class WebExtensionManagerDelegate {
public:
    virtual void recordTestAssertionResult(bool result, const String& message, const String& sourceURL, unsigned lineNumber) = 0;
    virtual void recordTestEqualityResult(bool result, const String& expectedValue, const String& actualValue, const String& message, const String& sourceURL, unsigned lineNumber) = 0;
    virtual void logTestMessage(const String& message, const String& sourceURL, unsigned lineNumber) = 0;
    virtual void receivedTestMessage(const String& argument, const String& message, const String& sourceURL, unsigned lineNumber) = 0;
    virtual void recordTestAdded(const String& testName, const String& sourceURL, unsigned lineNumber) = 0;
    virtual void recordTestStarted(const String& testName, const String& sourceURL, unsigned lineNumber) = 0;
    virtual void recordTestFinished(const String& testName, bool result, const String& message, const String& sourceURL, unsigned lineNumber) = 0;
};

#endif // ENABLE(WK_WEB_EXTENSIONS)
