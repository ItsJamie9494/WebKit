/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "TestMain.h"
#include "Utilities.h"
#include "WTFTestUtilities.h"
#include "WebKitWebExtensionManagerDelegatePrivate.h"

#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>

#if ENABLE(WK_WEB_EXTENSIONS)

class WebExtensionTest : public Test, WebExtensionManagerDelegate {
public:
    MAKE_GLIB_TEST_FIXTURE(WebExtensionTest);
    WebExtensionTest();
    virtual ~WebExtensionTest();

    void initializeExtension(HashMap<String, GRefPtr<GBytes>>&& resources);
    void loadExtension(HashMap<String, GRefPtr<GBytes>>&& resources);
    WebKitWebExtension* extension() const { return m_extension.get(); }
    WebKitWebExtensionContext* context() const { return m_context.get(); }
    WebKitWebExtensionManager* manager() const { return m_manager.get(); }

    void sendTestMessage(const char* message);
    void sendTestMessage(const char* message, const char* argument);
    void sendTestStarted(const char* argument);
    void sendTestFinished(const char* argument);

    void load();
    void unload();

    void run();
    void runUntilTestMessage(const char* message);
    String getLatestTestMessage(const char* message);

    // WebExtensionManagerDelegate overrides

    void recordTestAssertionResult(bool result, const String& message, const String& sourceURL, unsigned lineNumber) override;
    void recordTestEqualityResult(bool result, const String& expectedValue, const String& actualValue, const String& message, const String& sourceURL, unsigned lineNumber) override;
    void logTestMessage(const String& message, const String& sourceURL, unsigned lineNumber) override;
    void receivedTestMessage(const String& argument, const String& message, const String& sourceURL, unsigned lineNumber) override;
    void recordTestAdded(const String& testName, const String& sourceURL, unsigned lineNumber) override;
    void recordTestStarted(const String& testName, const String& sourceURL, unsigned lineNumber) override;
    void recordTestFinished(const String& testName, bool result, const String& message, const String& sourceURL, unsigned lineNumber) override;

    Vector<String> testsAdded;
    Vector<String> testsStarted;
    HashMap<String, bool> testResults;
private:
    void quitMainLoop();

    GRefPtr<WebKitWebExtension> m_extension;
    GRefPtr<WebKitWebExtensionContext> m_context;
    GRefPtr<WebKitWebExtensionManager> m_manager;
    GMainLoop* m_mainLoop;

    bool m_done { false };
    bool m_runningTestFromQueue { false };
    bool m_receivedMessage { false };

    HashMap<String, Vector<String>> m_messages;
};

namespace TestWebKitAPI {

namespace Util {

GRefPtr<GBytes> makePNGData(int width, int height, int color);

inline GRefPtr<GBytes> constructScript(Vector<String> lines)
{
    auto script = makeStringByJoining(lines, "\n"_s);
    auto utf8 = script.utf8();
    // new_static can result in memory corruption later on (where the string no longer acts as utf8)
    return adoptGRef(g_bytes_new(utf8.data(), utf8.length()));
}

inline GRefPtr<GBytes> stringToBytes(const String& string)
{
    return adoptGRef(g_bytes_new_static(string.utf8().data(), string.utf8().length()));
}

void runFor(double seconds);

} // namespace Util

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
