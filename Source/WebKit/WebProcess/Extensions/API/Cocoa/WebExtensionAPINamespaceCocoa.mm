/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPINamespace.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"

namespace WebKit {

WebExtensionAPIAction& WebExtensionAPINamespace::action()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/action

    if (!m_action)
        m_action = WebExtensionAPIAction::create(*this);

    return *m_action;
}

WebExtensionAPICommands& WebExtensionAPINamespace::commands()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/commands

    if (!m_commands)
        m_commands = WebExtensionAPICommands::create(*this);

    return *m_commands;
}

WebExtensionAPICookies& WebExtensionAPINamespace::cookies()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/cookies

    if (!m_cookies)
        m_cookies = WebExtensionAPICookies::create(*this);

    return *m_cookies;
}

WebExtensionAPIDeclarativeNetRequest& WebExtensionAPINamespace::declarativeNetRequest()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/declarativeNetRequest

    if (!m_declarativeNetRequest)
        m_declarativeNetRequest = WebExtensionAPIDeclarativeNetRequest::create(*this);

    return *m_declarativeNetRequest;
}

#if ENABLE(INSPECTOR_EXTENSIONS)
WebExtensionAPIDevTools& WebExtensionAPINamespace::devtools()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools

    if (!m_devtools)
        m_devtools = WebExtensionAPIDevTools::create(*this);

    return *m_devtools;
}
#endif // ENABLE(INSPECTOR_EXTENSIONS)

WebExtensionAPIDOM& WebExtensionAPINamespace::dom()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/dom

    if (!m_dom)
        m_dom = WebExtensionAPIDOM::create(*this);

    return *m_dom;
}

WebExtensionAPIExtension& WebExtensionAPINamespace::extension()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/extension

    if (!m_extension)
        m_extension = WebExtensionAPIExtension::create(*this);

    return *m_extension;
}

WebExtensionAPILocalization& WebExtensionAPINamespace::i18n()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/i18n

    if (!m_i18n)
        m_i18n = WebExtensionAPILocalization::create(*this);

    return *m_i18n;
}

WebExtensionAPIMenus& WebExtensionAPINamespace::menus()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/menus

    if (!m_menus)
        m_menus = WebExtensionAPIMenus::create(*this);

    return *m_menus;
}

WebExtensionAPINotifications& WebExtensionAPINamespace::notifications()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/notifications

    if (!m_notifications)
        m_notifications = WebExtensionAPINotifications::create(*this);

    return *m_notifications;
}

WebExtensionAPIPermissions& WebExtensionAPINamespace::permissions()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/permissions

    if (!m_permissions)
        m_permissions = WebExtensionAPIPermissions::create(*this);

    return *m_permissions;
}

WebExtensionAPIScripting& WebExtensionAPINamespace::scripting()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/scripting

    if (!m_scripting)
        m_scripting = WebExtensionAPIScripting::create(*this);

    return *m_scripting;
}

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
WebExtensionAPISidePanel& WebExtensionAPINamespace::sidePanel()
{
    // Documentation: https://developer.chrome.com/docs/extensions/reference/api/sidePanel

    if (!m_sidePanel)
        m_sidePanel = WebExtensionAPISidePanel::create(*this);

    return *m_sidePanel;
}

WebExtensionAPISidebarAction& WebExtensionAPINamespace::sidebarAction()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/sidebarAction

    if (!m_sidebarAction)
        m_sidebarAction = WebExtensionAPISidebarAction::create(*this);

    return *m_sidebarAction;
}
#endif

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
WebExtensionAPIBookmarks& WebExtensionAPINamespace::bookmarks()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/bookmarks

    if (!m_bookmarks)
        m_bookmarks = WebExtensionAPIBookmarks::create(*this);

    return *m_bookmarks;
}
#endif

WebExtensionAPIStorage& WebExtensionAPINamespace::storage()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage

    if (!m_storage)
        m_storage = WebExtensionAPIStorage::create(*this);

    return *m_storage;
}

WebExtensionAPITabs& WebExtensionAPINamespace::tabs()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/tabs

    if (!m_tabs)
        m_tabs = WebExtensionAPITabs::create(*this);

    return *m_tabs;
}

WebExtensionAPIWindows& WebExtensionAPINamespace::windows()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/windows

    if (!m_windows)
        m_windows = WebExtensionAPIWindows::create(*this);

    return *m_windows;
}

WebExtensionAPIWebNavigation& WebExtensionAPINamespace::webNavigation()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webNavigation

    if (!m_webNavigation)
        m_webNavigation = WebExtensionAPIWebNavigation::create(*this);

    return *m_webNavigation;
}

WebExtensionAPIWebRequest& WebExtensionAPINamespace::webRequest()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest

    if (!m_webRequest)
        m_webRequest = WebExtensionAPIWebRequest::create(*this);

    return *m_webRequest;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
