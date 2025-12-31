/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionUtilities.h"

using namespace TestWebKitAPI;

static constexpr auto manifest = "{"
    "\"manifest_version\": 3,"
    "\"name\": \"Test Extension\","
    "\"description\": \"Test Extension\","
    "\"version\": \"1.0\","

    "\"background\": {"
        "\"scripts\": [ \"background.js\" ],"
        "\"persistent\": false"
    "},"

    "\"content_scripts\": [{"
        "\"matches\": [ \"*://*/*\" ],"
        "\"js\": [ \"content.js\" ]"
    "}]"
"}"_s;

static void testTestStartedEvent(WebExtensionTest* test, gconstpointer)
{
    auto backgroundScript = Util::constructScript({
        "browser.test.onTestStarted.addListener((data) => {"_s,
        "  browser.test.assertEq(data?.testName, 'test', 'data.testName should be')"_s,

        "  browser.test.notifyPass()"_s,
        "})"_s,

        "browser.test.sendMessage('Send Test Message')"_s
    });

    test->loadExtension({ { "manifest.json"_s, Util::stringToBytes(manifest) }, { "background.js"_s, backgroundScript } });

    test->runUntilTestMessage("Send Test Message");

    test->sendTestStarted("{ \"testName\": \"test\" }");

    test->run();
}

static void testTestFinishedEvent(WebExtensionTest* test, gconstpointer)
{
    auto backgroundScript = Util::constructScript({
        "browser.test.onTestFinished.addListener((data) => {"_s,
        "  browser.test.assertEq(data?.testName, 'test', 'data.testName should be')"_s,

        "  browser.test.notifyPass()"_s,
        "})"_s,

        "browser.test.sendMessage('Send Test Message')"_s
    });

    test->loadExtension({ { "manifest.json"_s, Util::stringToBytes(manifest) }, { "background.js"_s, backgroundScript } });

    test->runUntilTestMessage("Send Test Message");

    test->sendTestFinished("{ \"testName\": \"test\" }");

    test->run();
}

static void testMessageEvent(WebExtensionTest* test, gconstpointer)
{
    auto backgroundScript = Util::constructScript({
        "browser.test.onMessage.addListener((message, data) => {"_s,
        "  browser.test.assertEq(message, 'Test', 'message should be')"_s,
        "  browser.test.assertEq(data?.key, 'value', 'data.key should be')"_s,

        "  browser.test.notifyPass()"_s,
        "})"_s,

        "browser.test.sendMessage('Send Test Message')"_s
    });

    test->loadExtension({ { "manifest.json"_s, Util::stringToBytes(manifest) }, { "background.js"_s, backgroundScript } });

    test->runUntilTestMessage("Send Test Message");

    test->sendTestMessage("Test", "{ \"key\": \"value\" }");

    test->run();
}

static void testMessageEventWithSendMessageReply(WebExtensionTest* test, gconstpointer)
{
    auto backgroundScript = Util::constructScript({
        "browser.test.onMessage.addListener((message, data) => {"_s,
        "  browser.test.assertEq(message, 'Test', 'message should be')"_s,
        "  browser.test.assertEq(data, undefined, 'data should be')"_s,

        "  browser.test.sendMessage('Received')"_s,
        "})"_s,

        "browser.test.sendMessage('Ready')"_s
    });

    test->loadExtension({ { "manifest.json"_s, Util::stringToBytes(manifest) }, { "background.js"_s, backgroundScript } });

    test->runUntilTestMessage("Ready");

    test->sendTestMessage("Test");

    test->runUntilTestMessage("Received");
}

static void testSendMessage(WebExtensionTest* test, gconstpointer)
{
    auto backgroundScript = Util::constructScript({
        "browser.test.sendMessage('Test', { key: 'value' })"_s
    });

    test->loadExtension({ { "manifest.json"_s, Util::stringToBytes(manifest) }, { "background.js"_s, backgroundScript } });

    test->runUntilTestMessage("Test");

    String receivedMessage = test->getLatestTestMessage("Test");
    g_assert_cmpstr(receivedMessage.utf8().data(), ==, "{\"key\":\"value\"}");
}

static void testSendMessageMultipleTimes(WebExtensionTest* test, gconstpointer)
{
    auto backgroundScript = Util::constructScript({
        "browser.test.sendMessage('Test', { key: 'One' })"_s,
        "browser.test.sendMessage('Test', { key: 'Two' })"_s,
        "browser.test.sendMessage('Test', { key: 'Three' })"_s
    });

    test->loadExtension({ { "manifest.json"_s, Util::stringToBytes(manifest) }, { "background.js"_s, backgroundScript } });

    test->runUntilTestMessage("Test");
    String receivedMessage = test->getLatestTestMessage("Test");
    g_assert_cmpstr(receivedMessage.utf8().data(), ==, "{\"key\":\"One\"}");

    test->runUntilTestMessage("Test");
    String receivedMessageTwo = test->getLatestTestMessage("Test");
    g_assert_cmpstr(receivedMessageTwo.utf8().data(), ==, "{\"key\":\"Two\"}");

    test->runUntilTestMessage("Test");
    String receivedMessageThree = test->getLatestTestMessage("Test");
    g_assert_cmpstr(receivedMessageThree.utf8().data(), ==, "{\"key\":\"Three\"}");
}

static void testSendMessageOutOfOrder(WebExtensionTest* test, gconstpointer)
{
    auto backgroundScript = Util::constructScript({
        "browser.test.sendMessage('Message 1', { key: 'One' })"_s,
        "browser.test.sendMessage('Message 2', { key: 'Two' })"_s,
        "browser.test.sendMessage('Message 3', { key: 'Three' })"_s
    });

    test->loadExtension({ { "manifest.json"_s, Util::stringToBytes(manifest) }, { "background.js"_s, backgroundScript } });

    test->runUntilTestMessage("Message 2");
    String receivedMessage = test->getLatestTestMessage("Message 2");
    g_assert_cmpstr(receivedMessage.utf8().data(), ==, "{\"key\":\"Two\"}");

    test->runUntilTestMessage("Message 3");
    receivedMessage = test->getLatestTestMessage("Message 3");
    g_assert_cmpstr(receivedMessage.utf8().data(), ==, "{\"key\":\"Three\"}");

    test->runUntilTestMessage("Message 1");
    receivedMessage = test->getLatestTestMessage("Message 1");
    g_assert_cmpstr(receivedMessage.utf8().data(), ==, "{\"key\":\"One\"}");
}

static void testAddAnonymousAsyncTest(WebExtensionTest* test, gconstpointer)
{
    auto backgroundScript = Util::constructScript({
        "browser.test.assertRejects(browser.test.addTest(async () => {"_s,
        "  browser.test.assertTrue(true)"_s,
        "}))"_s,
        "  .then(() => browser.test.notifyPass())"_s,
        "  .catch(() => browser.test.notifyFail('Passing an anonymous function into addTest resolved the promise.'))"_s
    });

    test->loadExtension({ { "manifest.json"_s, Util::stringToBytes(manifest) }, { "background.js"_s, backgroundScript } });

    test->run();

    g_assert_cmpint(test->testsAdded.size(), ==, 0);
    g_assert_cmpint(test->testsStarted.size(), ==, 0);
    g_assert_cmpint(test->testResults.size(), ==, 0);
}

static void testAddAsyncTestThatPasses(WebExtensionTest* test, gconstpointer)
{
    static const char* testName = "passingTest";
    auto backgroundScript = Util::constructScript({
        "browser.test.assertResolves(browser.test.addTest(async function passingTest() {"_s,
        "  browser.test.assertTrue(true)"_s,
        "}))"_s,
        "  .then(() => browser.test.notifyPass())"_s,
        "  .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))"_s
    });

    test->loadExtension({ { "manifest.json"_s, Util::stringToBytes(manifest) }, { "background.js"_s, backgroundScript } });

    test->run();

    g_assert_cmpint(test->testsAdded.size(), ==, 1);
    g_assert_cmpstr(test->testsAdded.first().utf8().data(), ==, testName);
    g_assert_cmpint(test->testsStarted.size(), ==, 1);
    g_assert_cmpstr(test->testsStarted.first().utf8().data(), ==, testName);
}

void beforeAll()
{
    WebExtensionTest::add("WebKitWebExtensionAPITest", "test-started-event", testTestStartedEvent);
    WebExtensionTest::add("WebKitWebExtensionAPITest", "test-finished-event", testTestFinishedEvent);
    WebExtensionTest::add("WebKitWebExtensionAPITest", "message-event", testMessageEvent);
    WebExtensionTest::add("WebKitWebExtensionAPITest", "message-event-with-send-message-reply", testMessageEventWithSendMessageReply);
    WebExtensionTest::add("WebKitWebExtensionAPITest", "send-message", testSendMessage);
    WebExtensionTest::add("WebKitWebExtensionAPITest", "send-message-multiple-times", testSendMessageMultipleTimes);
    WebExtensionTest::add("WebKitWebExtensionAPITest", "send-message-out-of-order", testSendMessageOutOfOrder);
    WebExtensionTest::add("WebKitWebExtensionAPITest", "add-anonymous-async-test", testAddAnonymousAsyncTest);
    WebExtensionTest::add("WebKitWebExtensionAPITest", "add-async-test-that-passes", testAddAsyncTestThatPasses);

    g_set_prgname("org.webkit.app-TestWebKitGTK");
}

void afterAll()
{
}

#endif // ENABLE(WK_WEB_EXTENSIONS)
