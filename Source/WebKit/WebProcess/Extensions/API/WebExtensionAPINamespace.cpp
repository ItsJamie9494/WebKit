/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "WebExtensionAPINamespace.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionControllerProxy.h"
#include "WebExtensionPermission.h"

#if ENABLE(INSPECTOR_EXTENSIONS) || ENABLE(WK_WEB_EXTENSIONS_SIDEBAR) ||  ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
#include "WebPage.h"
#include <WebCore/Page.h>
#include <WebCore/Settings.h>
#endif

namespace WebKit {

static bool doesDictionaryExist(RefPtr<JSON::Value> value, const String& name)
{
    RefPtr object = value->asObject();
    if (!object)
        return false;

    RefPtr namedValue = object->getValue(name);
    if (!namedValue)
        return false;

    return namedValue->type() == JSON::Value::Type::Object;
}

#if ENABLE(INSPECTOR_EXTENSIONS)
static bool doesStringExist(RefPtr<JSON::Value> value, const String& name)
{
    RefPtr object = value->asObject();
    if (!object)
        return false;

    RefPtr namedValue = object->getValue(name);
    if (!namedValue)
        return false;

    return namedValue->type() == JSON::Value::Type::String;
}
#endif

bool WebExtensionAPINamespace::isPropertyAllowed(const ASCIILiteral& name, WebPage* page)
{
    Ref extensionContext = this->extensionContext();
    if (extensionContext->isUnsupportedAPI(propertyPath(), name)) [[unlikely]]
        return false;

    if (name == "action"_s)
        return extensionContext->supportsManifestVersion(3) && doesDictionaryExist(extensionContext->manifest(), "action"_s);

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
    if (name == "bookmarks"_s)
        return page->corePage()->settings().webExtensionBookmarksEnabled() && extensionContext->hasPermission("bookmarks"_s);
#endif

    if (name == "commands"_s)
        return doesDictionaryExist(extensionContext->manifest(), "commands"_s);

    if (name == "declarativeNetRequest"_s)
        return extensionContext->hasPermission(name) || extensionContext->hasPermission("declarativeNetRequestWithHostAccess"_s);

    if (name == "browserAction"_s)
        return !extensionContext->supportsManifestVersion(3) && doesDictionaryExist(extensionContext->manifest(), "browser_action"_s);

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (name == "devtools"_s)
        return doesStringExist(extensionContext->manifest(), "devtools_page"_s) && page && (page->isInspectorPage() || extensionContext->isInspectorBackgroundPage(*page));
#else
    if (name == "devtools"_s)
        return false;
#endif

    if (name == "notifications"_s) {
        // FIXME: <rdar://problem/57202210> Add support for browser.notifications.
        // Notifications are currently only available in test mode as an empty stub.
        if (!extensionContext->inTestingMode())
            return false;
        goto finish;
    }

    if (name == "pageAction"_s)
        return !extensionContext->supportsManifestVersion(3) && doesDictionaryExist(extensionContext->manifest(), "page_action"_s);

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    // If the extension requests both sidePanel and sidebarAction, we will give them sidebarAction --
    // we check in sidePanel that there is no sidebar_action key, but we do not check in sidebarAction
    // that there is no sidePanel permission
    if (name == "sidePanel"_s)
        return page->corePage()->settings().webExtensionSidebarEnabled() && extensionContext->hasPermission("sidePanel"_s) && doesDictionaryExist(extensionContext->manifest(), "sidebar_action"_s);
    if (name == "sidebarAction"_s)
        return page->corePage()->settings().webExtensionSidebarEnabled() && doesDictionaryExist(extensionContext->manifest(), "sidebar_action"_s);
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

    if (name == "storage"_s)
        return extensionContext->hasPermission(name) || extensionContext->hasPermission("unlimitedStorage"_s);

    if (name == "test"_s)
        return extensionContext->inTestingMode();

finish:
    // The rest of the property names marked dynamic in WebExtensionAPINamespace.idl match permission names.
    // Check for the permission to determine if the property is allowed to be accessed.
    return extensionContext->hasPermission(name);
}

WebExtensionAPIAlarms& WebExtensionAPINamespace::alarms()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms

    if (!m_alarms)
        m_alarms = WebExtensionAPIAlarms::create(*this);

    return *m_alarms;
}

WebExtensionAPIRuntime& WebExtensionAPINamespace::runtime() const
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime

    if (!m_runtime) {
        m_runtime = WebExtensionAPIRuntime::create(contentWorldType(), protectedExtensionContext());
        m_runtime->setPropertyPath("runtime"_s, this);
    }

    return *m_runtime;
}

WebExtensionAPITest& WebExtensionAPINamespace::test()
{
    // Documentation: None (Testing Only)

    if (!m_test)
        m_test = WebExtensionAPITest::create(*this);

    return *m_test;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
