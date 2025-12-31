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

#include "config.h"
#include "WebExtensionController.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "Logging.h"

namespace WebKit {

void WebExtensionController::testResult(bool result, String message, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    delegate->recordTestAssertionResult(result, message, sourceURL, lineNumber);

    if (message.isEmpty())
        message = "(no message)"_s;

    if (result) {
        RELEASE_LOG_INFO(Extensions, "Test assertion passed: %{public}@ (%{public}@:%{public}u)", message, sourceURL, lineNumber);
        return;
    }

    RELEASE_LOG_ERROR(Extensions, "Test assertion failed: %{public}@ (%{public}@:%{public}u)", message, sourceURL, lineNumber);
}

void WebExtensionController::testEqual(bool result, String expectedValue, String actualValue, String message, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    delegate->recordTestEqualityResult(result, expectedValue, actualValue, message, sourceURL, lineNumber);

    if (message.isEmpty())
        message = "Expected equality of these values"_s;

    if (result) {
        RELEASE_LOG_INFO(Extensions, "Test equality passed: %{public}@: %{public}@ === %{public}@ (%{public}@:%{public}u)", message, expectedValue, actualValue, sourceURL, lineNumber);
        return;
    }

    RELEASE_LOG_ERROR(Extensions, "Test equality failed: %{public}@: %{public}@ !== %{public}@ (%{public}@:%{public}u)", message, expectedValue, actualValue, sourceURL, lineNumber);
}

void WebExtensionController::testLogMessage(String message, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    delegate->logTestMessage(message, sourceURL, lineNumber);

    if (message.isEmpty())
        message = "(no message)"_s;

    RELEASE_LOG_INFO(Extensions, "Test log: %{public}@ (%{public}@:%{public}u)", message, sourceURL, lineNumber);
}

void WebExtensionController::testSentMessage(String message, String argument, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    delegate->receivedTestMessage(argument, WTFMove(message), sourceURL, lineNumber);

    RELEASE_LOG_INFO(Extensions, "Test sent message: %{public}@ %{public}@ (%{public}@:%{public}u)", message, argument, sourceURL, lineNumber);
}

void WebExtensionController::testAdded(String testName, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    delegate->recordTestAdded(testName, sourceURL, lineNumber);

    RELEASE_LOG_INFO(Extensions, "Test added: %{public}@ (%{public}@:%{public}u)", testName, sourceURL, lineNumber);
}

void WebExtensionController::testStarted(String testName, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    delegate->recordTestStarted(testName, sourceURL, lineNumber);

    RELEASE_LOG_INFO(Extensions, "Test started: %{public}@ (%{public}@:%{public}u)", testName, sourceURL, lineNumber);
}

void WebExtensionController::testFinished(String testName, bool result, String message, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    delegate->recordTestFinished(testName, result, message, sourceURL, lineNumber);

    if (testName.isEmpty())
        testName = "(no test name)"_s;

    if (message.isEmpty())
        message = "(no message)"_s;

    if (result) {
        RELEASE_LOG_INFO(Extensions, "Test passed: %{public}@ %{public}@ (%{public}@:%{public}u)", testName, message, sourceURL, lineNumber);
        return;
    }

    RELEASE_LOG_ERROR(Extensions, "Test failed: %{public}@ %{public}@ (%{public}@:%{public}u)", testName, message, sourceURL, lineNumber);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
