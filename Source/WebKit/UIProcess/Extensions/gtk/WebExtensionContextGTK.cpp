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
#include "WebExtensionContext.h"

#include "WebKitSettingsPrivate.h"
#include "WebKitWebViewPrivate.h"
#include <wtf/glib/Application.h>

#if ENABLE(WK_WEB_EXTENSIONS)

static gboolean decidePolicyCb(WebKitWebView *webView, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, WebKit::WebExtensionContext* context)
{
    ASSERT(context);

    if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
        return false;

    if (context->decidePolicyForNavigationAction(webView, WEBKIT_NAVIGATION_POLICY_DECISION(decision)))
        webkit_policy_decision_use(decision);
    else
        webkit_policy_decision_ignore(decision);

    return true;
}

static gboolean didFinishDocumentLoadCb(WebKitWebView *webView, WebKitLoadEvent loadEvent, WebKit::WebExtensionContext* context)
{
    ASSERT(context);

    context->didFinishDocumentLoad(webView);

    return true;
}

static gboolean didFailNavigationCb(WebKitWebView *webView, WebKitLoadEvent loadEvent, gchar* failingURI, GError* error, WebKit::WebExtensionContext* context)
{
    ASSERT(context);

    context->didFailNavigation(webView, API::Error::create({ String::fromUTF8(g_quark_to_string(error->domain)), error->code, URL { String::fromUTF8(failingURI) }, String::fromUTF8(error->message) }));

    return true;
}

static gboolean webProcessTerminatedCb(WebKitWebView *webView, WebKitWebProcessTerminationReason reason, WebKit::WebExtensionContext* context)
{
    ASSERT(context);

    context->webViewWebContentProcessDidTerminate(webView);

    return true;
}

namespace WebKit {

WebKitWebView* WebExtensionContext::relatedWebView()
{
    ASSERT(isLoaded());

    if (m_backgroundWebView)
        return m_backgroundWebView.get();

    return nullptr;
}

void WebExtensionContext::loadBackgroundWebView()
{
    ASSERT(isLoaded());

    if (!protectedExtension()->hasBackgroundContent())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    RELEASE_LOG_DEBUG(Extensions, "Loading background content");

    ASSERT(safeToLoadBackgroundContent());

    ASSERT(!m_backgroundContentIsLoaded);
    m_backgroundContentIsLoaded = false;

    ASSERT(!m_backgroundWebView);

    bool isManifestVersion3 = protectedExtension()->supportsManifestVersion(3);

    WebKitSettings* settings = webViewConfiguration();
    WebKit::WebPreferences* preferences = webkitSettingsGetPreferences(settings);
    WebKitWebView* webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-extension-mode", isManifestVersion3 ? WEBKIT_WEB_EXTENSION_MODE_MANIFESTV3 : WEBKIT_WEB_EXTENSION_MODE_MANIFESTV2,
        "related-view", preferences->siteIsolationEnabled() ? nullptr : relatedWebView(),
        "settings", settings,
        nullptr));
    m_backgroundWebView = webView;

    g_signal_connect(m_backgroundWebView.get(), "decide-policy", G_CALLBACK(decidePolicyCb), this);
    g_signal_connect(m_backgroundWebView.get(), "load-changed", G_CALLBACK(didFinishDocumentLoadCb), this);
    g_signal_connect(m_backgroundWebView.get(), "load-failed", G_CALLBACK(didFailNavigationCb), this);
    g_signal_connect(m_backgroundWebView.get(), "web-process-terminated", G_CALLBACK(webProcessTerminatedCb), this);

    Ref pageProxy = webkitWebViewGetPage(m_backgroundWebView.get());
    pageProxy->setInspectable(m_inspectable);

    setBackgroundWebViewInspectionName(backgroundWebViewInspectionName());
    clearError(Error::BackgroundContentFailedToLoad);
    m_backgroundContentLoadError = nullptr;

    Ref backgroundProcess = pageProxy->siteIsolatedProcess();

    // Use foreground activity to keep background content responsive to events.
    m_backgroundWebViewActivity = backgroundProcess->protectedThrottler()->foregroundActivity("Web Extension background content"_s);

    if (!protectedExtension()->backgroundContentIsServiceWorker()) {
        webkit_web_view_load_request(m_backgroundWebView.get(), webkit_uri_request_new(backgroundContentURL().string().utf8().data()));
        return;
    }

    webkitWebViewLoadServiceWorker(m_backgroundWebView.get(), backgroundContentURL().string().utf8().data(), protectedExtension()->backgroundContentUsesModules(), [this, protectedThis = Ref { *this }](bool success) {
        if (!success) {
            m_backgroundContentLoadError = createError(Error::BackgroundContentFailedToLoad);
            recordErrorIfNeeded(backgroundContentLoadError());
            return;
        }

        performTasksAfterBackgroundContentLoads();
    });
}

void WebExtensionContext::unloadBackgroundWebView()
{
    if (!m_backgroundWebView)
        return;

    m_backgroundContentIsLoaded = false;
    m_unloadBackgroundWebViewTimer = nullptr;
    m_backgroundWebViewActivity = nullptr;

    webkit_web_view_try_close(m_backgroundWebView.get());
    g_signal_handlers_disconnect_by_func(m_backgroundWebView.get(), reinterpret_cast<gpointer>(decidePolicyCb), this);
    g_signal_handlers_disconnect_by_func(m_backgroundWebView.get(), reinterpret_cast<gpointer>(didFinishDocumentLoadCb), this);
    g_signal_handlers_disconnect_by_func(m_backgroundWebView.get(), reinterpret_cast<gpointer>(didFailNavigationCb), this);
    g_signal_handlers_disconnect_by_func(m_backgroundWebView.get(), reinterpret_cast<gpointer>(webProcessTerminatedCb), this);
    m_backgroundWebView = nullptr;
}

bool WebExtensionContext::isNotRunningInTestRunner()
{
    // This value is manually set in each test runner that uses a WebExtensionContext
    return WTF::applicationID() != "org.webkit.app-TestWebKitGTK"_s;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
