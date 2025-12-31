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
#include "WebExtensionUtilities.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebKitSettingsPrivate.h"
#include "WebKitWebExtensionContextInternal.h"
#include "WebKitWebExtensionInternal.h"
#include "WebKitWebExtensionManagerInternal.h"

WebExtensionTest::WebExtensionTest()
    : m_mainLoop(g_main_loop_new(nullptr, TRUE))
{
}

WebExtensionTest::~WebExtensionTest()
{
    unload();

    g_main_loop_unref(m_mainLoop);
}

void WebExtensionTest::initializeExtension(HashMap<String, GRefPtr<GBytes>>&& resources)
{
    GUniqueOutPtr<GError> error;
    GRefPtr<WebKitWebExtensionManager> manager = adoptGRef(webkit_web_extension_manager_new_with_non_persistent_settings());

    auto settings = webkit_web_extension_manager_get_settings(manager.get());
    webkitSettingsSetSiteIsolationEnabled(settings, false);

    GRefPtr<WebKitWebExtension> extension = webkitWebExtensionCreate(WTFMove(resources), &error.outPtr());
    g_assert_no_error(error.get());
    GRefPtr<WebKitWebExtensionContext> context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    m_extension = extension;
    m_context = context;
    m_manager = manager;

    // Grant all requested API permissions.
    const gchar* const* requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    if (requestedPermissions) {
        for (; *requestedPermissions != nullptr; requestedPermissions++)
            webkit_web_extension_context_set_permission_status_for_permission(context.get(), *requestedPermissions, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY, nullptr);
    }

    webkitWebExtensionManagerSetTestingMode(manager.get(), true);

    // This should always be self.
    webkitWebExtensionManagerSetPrivateDelegate(manager.get(), this);
}

void WebExtensionTest::loadExtension(HashMap<String, GRefPtr<GBytes>>&& resources)
{
    initializeExtension(WTFMove(resources));
    load();
}

void WebExtensionTest::sendTestMessage(const char* message)
{
    sendTestMessage(message, nullptr);
}

void WebExtensionTest::sendTestMessage(const char* message, const char* argument)
{
    webkitWebExtensionContextSendTestMessage(m_context.get(), message, argument);
}

void WebExtensionTest::sendTestStarted(const char* argument)
{
    webkitWebExtensionContextSendTestStarted(m_context.get(), argument);
}

void WebExtensionTest::sendTestFinished(const char* argument)
{
    webkitWebExtensionContextSendTestFinished(m_context.get(), argument);
}

void WebExtensionTest::load()
{
    GUniqueOutPtr<GError> error;
    g_assert_true(webkit_web_extension_manager_load_extension_context(m_manager.get(), m_context.get(), &error.outPtr()));
    g_assert_no_error(error.get());
}

void WebExtensionTest::unload()
{
    GUniqueOutPtr<GError> error;
    g_assert_true(webkit_web_extension_manager_unload_extension_context(m_manager.get(), m_context.get(), &error.outPtr()));
    g_assert_no_error(error.get());
}

void WebExtensionTest::quitMainLoop()
{
    g_main_loop_quit(m_mainLoop);
}

void WebExtensionTest::run()
{
    g_main_loop_run(m_mainLoop);
}

struct RunAsyncData {
    WebExtensionTest* test;
    String message;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(RunAsyncData);

// It is difficult to get the test message and return it from the timeout,
// instead we'll use this function to wait for the test message syncronously
// and use the getLatestTestMessage function to confirm the results.
void WebExtensionTest::runUntilTestMessage(const char* message)
{
    auto msg = String::fromUTF8(message);
    if (m_messages.contains(msg)) {
        auto messagesArray = m_messages.get(msg);
        if (messagesArray.size())
            return;
    }

    auto data = createRunAsyncData();
    data->test = this;
    data->message = msg;

    g_timeout_add_full(G_PRIORITY_DEFAULT, 1000, [](gpointer userData) -> gboolean {
        auto data = reinterpret_cast<RunAsyncData*>(userData);

        if (!data->test->m_messages.contains(data->message))
            return G_SOURCE_CONTINUE;

        auto messagesArray = data->test->m_messages.get(data->message);
        if (!messagesArray.size())
            return G_SOURCE_CONTINUE;

        data->test->quitMainLoop();
        return G_SOURCE_REMOVE;
    }, data, reinterpret_cast<GDestroyNotify>(destroyRunAsyncData));
    run();
}

String WebExtensionTest::getLatestTestMessage(const char* message)
{
    String msg = String::fromUTF8(message);
    if (!m_messages.contains(msg))
        return nullString();

    auto messagesArray = m_messages.take(msg);
    if (!messagesArray.size())
        return nullString();

    String argument = messagesArray.first();
    messagesArray.removeAt(0);
    m_messages.set(msg, messagesArray);

    return argument;
}

// WebExtensionManagerDelegate overrides

void WebExtensionTest::recordTestAssertionResult(bool result, const String& messageValue, const String& sourceURL, unsigned lineNumber)
{
    if (result || m_runningTestFromQueue)
        return;

    String message = !messageValue.isEmpty() ? messageValue : "Assertion failed with no message."_s;

    // We already did the assertions internally, all we need to do here is just display the message
    g_assertion_message(G_LOG_DOMAIN, sourceURL.utf8().data(), lineNumber, nullptr, message.utf8().data());
}

void WebExtensionTest::recordTestEqualityResult(bool result, const String& expectedValue, const String& actualValue, const String& messageValue, const String& sourceURL, unsigned lineNumber)
{
    if (result || m_runningTestFromQueue)
        return;

    StringBuilder message;
    message.append(!messageValue.isEmpty() ? messageValue : "Expected equality of these values"_s);
    message.append(":\n  Actual: "_s);
    message.append(actualValue);
    message.append("\nExpected: "_s);
    message.append(expectedValue);

    g_assertion_message(G_LOG_DOMAIN, sourceURL.utf8().data(), lineNumber, nullptr,
        message.toString().utf8().data());
}

void WebExtensionTest::logTestMessage(const String& message, const String& sourceURL, unsigned lineNumber)
{
    SAFE_PRINTF("\n%s:%u\n%s\n\n", sourceURL.utf8(), lineNumber, message.utf8());
}

void WebExtensionTest::receivedTestMessage(const String& argument, const String& message, const String& sourceURL, unsigned lineNumber)
{
    m_receivedMessage = true;

    auto messagesArray = m_messages.find(message);
    if (messagesArray == m_messages.end()) {
        Vector<String> vector { argument };
        m_messages.add(message, WTFMove(vector));
    } else
        messagesArray->value.append(argument);
}

void WebExtensionTest::recordTestAdded(const String& testName, const String& sourceURL, unsigned lineNumber)
{
    testsAdded.append(testName);
}

void WebExtensionTest::recordTestStarted(const String& testName, const String& sourceURL, unsigned lineNumber)
{
    m_runningTestFromQueue = true;
    testsStarted.append(testName);
}

void WebExtensionTest::recordTestFinished(const String& testName, bool result, const String& messageValue, const String& sourceURL, unsigned lineNumber)
{
    if (m_runningTestFromQueue) {
        m_runningTestFromQueue = false;

        testResults.set(testName, result);

        return;
    }

    quitMainLoop();

    if (result)
        return;

    String message = !messageValue.isEmpty() ? messageValue : "Test failed with no message."_s;

    g_assertion_message(G_LOG_DOMAIN, sourceURL.utf8().data(), lineNumber, nullptr, message.utf8().data());
}

#endif // ENABLE(WK_WEB_EXTENSIONS)
