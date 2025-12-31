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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionContextProxyMessages.h"
#include "WebExtensionPermission.h"
#include "WebKitEnumTypes.h"
#include "WebKitNavigationActionPrivate.h"
#include "WebKitPrivate.h"
#include "WebKitSettingsPrivate.h"
#include "WebKitWebView.h"
#include "WebKitWebViewPrivate.h"
#include "WebKitWebsiteDataManagerPrivate.h"
#include <wtf/URL.h>

static constexpr auto groupNameStateKey = "ExtensionState"_s;
static constexpr auto backgroundContentEventListenersKey = "BackgroundContentEventListeners"_s;
static constexpr auto backgroundContentEventListenersVersionKey = "BackgroundContentEventListenersVersion"_s;
static constexpr auto lastSeenBaseURLStateKey = "LastSeenBaseURL"_s;
static constexpr auto lastSeenVersionStateKey = "LastSeenVersion"_s;
static constexpr auto lastSeenDisplayNameStateKey = "LastSeenDisplayName"_s;
static constexpr auto sessionStorageAllowedInContentScriptsKey = "SessionStorageAllowedInContentScripts"_s;

static constexpr auto WebExtensionContextGrantedPermissionsWereRemovedSignal = "granted-permissions-were-removed"_s;
static constexpr auto WebExtensionContextGrantedPermissionMatchPatternsWereRemovedSignal = "granted-permission-match-patterns-were-removed"_s;
static constexpr auto WebExtensionContextDeniedPermissionsWereRemovedSignal = "denied-permissions-were-removed"_s;
static constexpr auto WebExtensionContextDeniedPermissionMatchPatternsWereRemovedSignal = "denied-permission-match-patterns-were-removed"_s;
static constexpr auto WebExtensionContextPermissionsWereDeniedSignal = "permissions-were-denied"_s;
static constexpr auto WebExtensionContextPermissionsWereGrantedSignal = "permissions-were-granted"_s;
static constexpr auto WebExtensionContextPermissionMatchPatternsWereDeniedSignal = "permission-match-patterns-were-denied"_s;
static constexpr auto WebExtensionContextPermissionMatchPatternsWereGrantedSignal = "permission-match-patterns-were-granted"_s;

// Update this value when any changes are made to the WebExtensionEventListenerType enum.
static constexpr auto currentBackgroundContentListenerStateVersion = 4;

namespace WebKit {

WebExtensionContext::WebExtensionContext(Ref<WebExtension>&& extension, GRefPtr<WebKitWebExtensionContext>&& contextObject)
    : WebExtensionContext()
{
    m_extension = extension.ptr();
    m_baseURL = URL { makeString("webkit-extension://"_s, uniqueIdentifier(), '/') };
    m_delegate = contextObject;
}

void WebExtensionContext::recordError(Ref<API::Error> error)
{
    RELEASE_LOG_ERROR(Extensions, "Error recorded: %s", error->localizedDescription().utf8().data());

    // Only the first occurrence of each error is recorded in the array. This prevents duplicate errors,
    // such as repeated "resource not found" errors, from being included multiple times.
    if (m_errors.containsIf([&](auto& existingError) { return existingError->localizedDescription() == error->localizedDescription(); }))
        return;

    m_errors.append(error);
}

void WebExtensionContext::clearError(Error error)
{
    if (!m_errors.size())
        return;

    auto errorCode = toAPIError(error);
    m_errors.removeAllMatching([&](auto& error) {
        return error->errorCode() == errorCode;
    });
}

Expected<bool, RefPtr<API::Error>> WebExtensionContext::load(WebExtensionController& controller, String storageDirectory)
{
    if (isLoaded()) {
        RELEASE_LOG_ERROR(Extensions, "Extension context already loaded");
        return makeUnexpected(createError(Error::AlreadyLoaded));
    }

    m_storageDirectory = storageDirectory;
    m_extensionController = controller;
    m_contentScriptWorld = API::ContentWorld::sharedWorldWithName(makeString("WebExtension-"_s, m_uniqueIdentifier));

    readStateFromStorage();

    auto lastSeenBaseURL = URL { String::fromUTF8(g_key_file_get_string(m_state.get(), groupNameStateKey, lastSeenBaseURLStateKey, nullptr)) };
    g_key_file_set_string(m_state.get(), groupNameStateKey, lastSeenBaseURLStateKey, m_baseURL.string().utf8().data());
    g_key_file_set_string(m_state.get(), groupNameStateKey, lastSeenDisplayNameStateKey, protectedExtension()->displayName().utf8().data());

    m_isSessionStorageAllowedInContentScripts = g_key_file_get_boolean(m_state.get(), groupNameStateKey, sessionStorageAllowedInContentScriptsKey, nullptr);

    determineInstallReasonDuringLoad();

    writeStateToStorage();

    moveLocalStorageIfNeeded(lastSeenBaseURL, [this, protectedThis = Ref { *this }] {
        // The extension could have been unloaded before this was called.
        if (!isLoaded())
            return;

        m_safeToInjectContent = true;

        loadBackgroundWebViewDuringLoad();

#if ENABLE(INSPECTOR_EXTENSIONS)
        loadInspectorBackgroundPagesDuringLoad();
#endif

        // Notify the WebProcess that the extension loaded before we inject content scripts.
        // This will ensure that the content world is set up correctly (e.g. configured with the `browser` namespace).
        if (RefPtr controller = extensionController())
            controller->dispatchDidLoad(*this);

        addInjectedContent();
    });

    return true;
}

Expected<bool, RefPtr<API::Error>> WebExtensionContext::unload()
{
    if (!isLoaded()) {
        RELEASE_LOG_ERROR(Extensions, "Extension context not loaded");
        return makeUnexpected(createError(Error::NotLoaded));
    }

    writeStateToStorage();

    unloadBackgroundWebView();
    removeInjectedContent();

    invalidateStorage();

    m_privilegedIdentifier = std::nullopt;

    m_actionsToPerformAfterBackgroundContentLoads.clear();
    m_backgroundContentEventListeners.clear();
    m_eventListenerFrames.clear();
    m_installReason = InstallReason::None;
    m_previousVersion = nullString();
    m_safeToLoadBackgroundContent = false;
    m_backgroundContentLoadError = nullptr;

    m_registeredScriptsMap.clear();
    m_dynamicallyInjectedUserStyleSheets.clear();
    m_injectedScriptsPerPatternMap.clear();
    m_injectedStyleSheetsPerPatternMap.clear();
    m_safeToInjectContent = false;

    m_extensionController = nullptr;
    m_contentScriptWorld = nullptr;

    m_tabMap.clear();
    m_extensionPageTabMap.clear();

    m_windowMap.clear();
    m_windowOrderVector.clear();
    m_focusedWindowIdentifier = std::nullopt;

    m_actionWindowMap.clear();
    m_actionTabMap.clear();
    m_defaultAction = nullptr;
#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    m_defaultSidebar = nullptr;
#endif
    m_popupPageActionMap.clear();

    m_ports.clear();
    m_pagePortMap.clear();
    m_portQueuedMessages.clear();
    m_nativePortMap.clear();

    m_alarmMap.clear();

    m_commands.clear();
    m_populatedCommands = false;

    m_menuItems.clear();
    m_mainMenuItems.clear();

#if ENABLE(INSPECTOR_EXTENSIONS)
    m_inspectorContextMap.clear();
#endif

    m_pendingPermissionRequests = 0;

    return true;
}

GKeyFile* WebExtensionContext::currentState() const
{
    return m_state.get();
}

GKeyFile* WebExtensionContext::readStateFromPath(const String& stateFilePath)
{
    GUniquePtr<GKeyFile> stateFile(g_key_file_new());
    GUniqueOutPtr<GError> error;

    g_key_file_load_from_file(stateFile.get(), stateFilePath.utf8().data(), G_KEY_FILE_NONE, &error.outPtr());
    if (error)
        RELEASE_LOG_ERROR(Extensions, "Failed to coordinate reading extension state: %" PUBLIC_LOG_STRING, error->message);

    return stateFile.get();
}

bool WebExtensionContext::readLastBaseURLFromState(const String& filePath, URL& outLastBaseURL)
{
    GUniquePtr<GKeyFile> state(readStateFromPath(filePath));

    if (auto *baseURL = g_key_file_get_string(state.get(), groupNameStateKey, lastSeenBaseURLStateKey, nullptr))
        outLastBaseURL = URL { String::fromUTF8(baseURL) };

    return outLastBaseURL.isValid();
}

bool WebExtensionContext::readDisplayNameFromState(const String& filePath, String& outDisplayName)
{
    GUniquePtr<GKeyFile> state(readStateFromPath(filePath));

    if (auto *displayName = g_key_file_get_string(state.get(), groupNameStateKey, lastSeenDisplayNameStateKey, nullptr))
        outDisplayName = String::fromUTF8(displayName);

    return outDisplayName.length();
}

const GKeyFile* WebExtensionContext::readStateFromStorage()
{
    if (!storageIsPersistent()) {
        if (!m_state)
            m_state.reset(g_key_file_new());
        return m_state.get();
    }

    auto savedState = readStateFromPath(stateFilePath());
    m_state.reset(savedState);
    return m_state.get();
}

void WebExtensionContext::writeStateToStorage() const
{
    if (!storageIsPersistent())
        return;

    GUniqueOutPtr<GError> error;

    if (!g_key_file_save_to_file(currentState(), stateFilePath().utf8().data(), &error.outPtr()))
        RELEASE_LOG_ERROR(Extensions, "Unable to save extension state: %" PUBLIC_LOG_STRING, error->message);

    if (error)
        RELEASE_LOG_ERROR(Extensions, "Failed to coordinate writing extension state: %" PUBLIC_LOG_STRING, error->message);
}

void WebExtensionContext::moveLocalStorageIfNeeded(const URL& previousBaseURL, CompletionHandler<void()>&& completionHandler)
{
    if (previousBaseURL == baseURL()) {
        completionHandler();
        return;
    }

    if (!m_backgroundWebView) {
        completionHandler();
        return;
    }

    WebKitWebsiteDataManager* dataManager = webkitWebViewGetWebsiteDataManager(m_backgroundWebView.get());
    Ref dataStore = webkitWebsiteDataManagerGetDataStore(dataManager);

    auto oldOrigin = WebCore::SecurityOriginData::fromURLWithoutStrictOpaqueness(previousBaseURL);
    auto newOrigin = WebCore::SecurityOriginData::fromURLWithoutStrictOpaqueness(baseURL());
    dataStore->renameOriginInWebsiteData(WTF::move(oldOrigin), WTF::move(newOrigin), { WebsiteDataType::IndexedDBDatabases, WebsiteDataType::LocalStorage }, [completionHandler = WTF::move(completionHandler)] mutable {
        completionHandler();
    });
}

void WebExtensionContext::invalidateStorage()
{
    m_localStorageStore = nullptr;
    m_sessionStorageStore = nullptr;
    m_syncStorageStore = nullptr;
}

void WebExtensionContext::setInspectable(bool inspectable)
{
    m_inspectable = inspectable;

    Ref pageProxy = webkitWebViewGetPage(m_backgroundWebView.get());
    pageProxy->setInspectable(inspectable);

    for (auto entry : m_extensionPageTabMap) {
        Ref page = entry.key;
        page->setInspectable(inspectable);
    }

    for (auto entry : m_popupPageActionMap) {
        Ref page = entry.key;
        page->setInspectable(inspectable);
    }
}

static String permissionNotification(WebExtensionContext::PermissionNotification notification)
{
    switch (notification) {
    case WebExtensionContext::PermissionNotification::PermissionsWereGranted:
        return WebExtensionContextPermissionsWereGrantedSignal;
    case WebExtensionContext::PermissionNotification::PermissionsWereDenied:
        return WebExtensionContextPermissionsWereDeniedSignal;
    case WebExtensionContext::PermissionNotification::GrantedPermissionsWereRemoved:
        return WebExtensionContextGrantedPermissionsWereRemovedSignal;
    case WebExtensionContext::PermissionNotification::DeniedPermissionsWereRemoved:
        return WebExtensionContextDeniedPermissionsWereRemovedSignal;
    case WebExtensionContext::PermissionNotification::PermissionMatchPatternsWereGranted:
        return WebExtensionContextPermissionMatchPatternsWereGrantedSignal;
    case WebExtensionContext::PermissionNotification::PermissionMatchPatternsWereDenied:
        return WebExtensionContextPermissionMatchPatternsWereDeniedSignal;
    case WebExtensionContext::PermissionNotification::GrantedPermissionMatchPatternsWereRemoved:
        return WebExtensionContextGrantedPermissionMatchPatternsWereRemovedSignal;
    case WebExtensionContext::PermissionNotification::DeniedPermissionMatchPatternsWereRemoved:
        return WebExtensionContextDeniedPermissionMatchPatternsWereRemovedSignal;
    case WebExtensionContext::PermissionNotification::None:
        return nullString();
    }

    return nullString();
}

void WebExtensionContext::permissionsDidChange(WebExtensionContext::PermissionNotification notification, const PermissionsSet& permissions)
{
    if (permissions.isEmpty())
        return;

    if (isLoaded()) {
        RefPtr extensionController = this->extensionController();
        if (!extensionController)
            return;

        extensionController->sendToAllProcesses(Messages::WebExtensionContextProxy::UpdateGrantedPermissions(m_grantedPermissions), identifier());

        if (permissions.contains(WebExtensionPermission::clipboardWrite())) {
            bool granted = hasPermission(WebExtensionPermission::clipboardWrite());

            enumerateExtensionPages([&](auto& page, bool&) {
                page.preferences().setJavaScriptCanAccessClipboard(granted);
            });
        }

        // if (notification == PermissionNotification::PermissionsWereGranted)
        //     firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnAdded, permissions, { });
        // else if (notification == PermissionNotification::GrantedPermissionsWereRemoved)
        //     firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnRemoved, permissions, { });
    }

    g_signal_emit_by_name(m_delegate.get(), permissionNotification(notification).utf8().data());
}

void WebExtensionContext::permissionsDidChange(WebExtensionContext::PermissionNotification notification, const MatchPatternSet& matchPatterns)
{
    if (matchPatterns.isEmpty())
        return;

    clearCachedPermissionStates();

    if (isLoaded()) {
        updateCORSDisablingPatternsOnAllExtensionPages();

        if (notification == PermissionNotification::PermissionMatchPatternsWereGranted) {
            addInjectedContent(injectedContents(), matchPatterns);
            // firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnAdded, { }, matchPatterns);
        } else if (notification == PermissionNotification::GrantedPermissionMatchPatternsWereRemoved) {
            removeInjectedContent(matchPatterns);
            // firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnRemoved, { }, matchPatterns);
        } else
            updateInjectedContent();
    }

    g_signal_emit_by_name(m_delegate.get(), permissionNotification(notification).utf8().data());
}

void WebExtensionContext::removePage(WebPageProxy& page)
{
    // This is a stub for now
}

std::optional<WebCore::PageIdentifier> WebExtensionContext::backgroundPageIdentifier() const
{
    if (!m_backgroundWebView || protectedExtension()->backgroundContentIsServiceWorker())
        return std::nullopt;

    Ref pageProxy = webkitWebViewGetPage(m_backgroundWebView.get());
    return pageProxy->webPageIDInMainFrameProcess();
}

void WebExtensionContext::enumerateExtensionPages(NOESCAPE Function<void(WebPageProxy&, bool&)>&& action)
{
    if (!isLoaded())
        return;

    bool stop = false;
    for (Ref page : extensionController()->allPages()) {
        auto* webView = page->platformView();
        if (isURLForThisExtension(webkitSettingsGetWebExtensionContext(webkit_web_view_get_settings(webView))->baseURL())) {
            action(page, stop);
            if (stop)
                return;
        }
    }
}

void WebExtensionContext::updateCORSDisablingPatternsOnAllExtensionPages()
{
    enumerateExtensionPages([&](auto& page, bool& stop) {
        page.setCORSDisablingPatterns(corsDisablingPatterns());
    });
}

WebKitSettings* WebExtensionContext::webViewConfiguration(WebViewPurpose purpose)
{
    if (!isLoaded())
        return nullptr;

    WebKitSettings* settings = extensionController()->protectedConfiguration()->webViewConfiguration();
    WebKit::WebPreferences* preferences = webkitSettingsGetPreferences(settings);

    webkit_settings_set_javascript_can_access_clipboard(settings, hasPermission(WebExtensionPermission::clipboardWrite()));

    if (purpose == WebViewPurpose::Background || purpose == WebViewPurpose::Inspector) {
        // FIXME: <https://webkit.org/b/263286> Consider allowing the background page to throttle or be suspended.
        preferences->setHiddenPageDOMTimerThrottlingEnabled(false);
        preferences->setPageVisibilityBasedProcessSuppressionEnabled(false);
        preferences->setShouldTakeNearSuspendedAssertions(true);
        preferences->setBackgroundWebContentRunningBoardThrottlingEnabled(false);
        preferences->setShouldDropNearSuspendedAssertionAfterDelay(false);
    }

    // Most Configuration options are configured inside the WebKitWebView instead of here
    webkitSettingsSetWebExtensionContext(settings, *this);

    return settings;
}

bool WebExtensionContext::isBackgroundPage(WebPageProxyIdentifier pageProxyIdentifier) const
{
    if (!m_backgroundWebView)
        return false;

    RefPtr pageProxy = webkitWebViewGetPage(m_backgroundWebView.get());
    return pageProxy->identifier() == pageProxyIdentifier;
}

void WebExtensionContext::unloadBackgroundWebView()
{
    if (!m_backgroundWebView)
        return;

    m_backgroundContentIsLoaded = false;
    m_unloadBackgroundWebViewTimer = nullptr;
    m_backgroundWebViewActivity = nullptr;

    webkit_web_view_try_close(m_backgroundWebView.get());
    m_backgroundWebView = nullptr;
}

void WebExtensionContext::setBackgroundWebViewInspectionName(const String& name)
{
    m_backgroundWebViewInspectionName = name;

    Ref pageProxy = webkitWebViewGetPage(m_backgroundWebView.get());
    pageProxy->setRemoteInspectionNameOverride(name);
}

void WebExtensionContext::unloadBackgroundContentIfPossible()
{
    if (!m_backgroundWebView || protectedExtension()->backgroundContentIsPersistent())
        return;

    if (m_pendingPermissionRequests) {
        RELEASE_LOG_DEBUG(Extensions, "Not unloading background content because it has pending permission requests");
        scheduleBackgroundContentToUnload();
        return;
    }

    Ref pageProxy = webkitWebViewGetPage(m_backgroundWebView.get());

    if (pageProxy->hasInspectorFrontend()) {
        RELEASE_LOG_DEBUG(Extensions, "Not unloading background content because it is being inspected");
        scheduleBackgroundContentToUnload();
        return;
    }

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (!m_inspectorContextMap.isEmptyIgnoringNullReferences()) {
        scheduleBackgroundContentToUnload();
        RELEASE_LOG_DEBUG(Extensions, "Not unloading background content because an inspector background page is open");
        return;
    }
#endif

    RELEASE_LOG_DEBUG(Extensions, "Unloading non-persistent background content");

    unloadBackgroundWebView();
}

void WebExtensionContext::determineInstallReasonDuringLoad()
{
    ASSERT(isLoaded());

    RefPtr extension = m_extension;
    String currentVersion = extension->version();
    GUniquePtr<gchar> previousVersion(g_key_file_get_string(m_state.get(), groupNameStateKey, lastSeenVersionStateKey, nullptr));
    m_previousVersion = String::fromUTF8(previousVersion.get());
    g_key_file_set_string(m_state.get(), groupNameStateKey, lastSeenVersionStateKey, currentVersion.utf8().data());

    bool extensionVersionDidChange = !m_previousVersion.isEmpty() && m_previousVersion != currentVersion;

    m_shouldFireStartupEvent = extensionController()->isFreshlyCreated();

    if (extensionVersionDidChange) {
        // Clear background event listeners on extension update.
        g_key_file_remove_key(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, nullptr);
        g_key_file_remove_key(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);

        RELEASE_LOG_DEBUG(Extensions, "Queued installed event with extension update reason");
        m_installReason = InstallReason::ExtensionUpdate;
    } else if (!m_shouldFireStartupEvent) {
        RELEASE_LOG_DEBUG(Extensions, "Queued installed event with extension install reason");
        m_installReason = InstallReason::ExtensionInstall;
    } else
        m_installReason = InstallReason::None;
}

void WebExtensionContext::loadBackgroundPageListenersFromStorage()
{
    if (!storageIsPersistent() || protectedExtension()->backgroundContentIsPersistent())
        return;

    m_backgroundContentEventListeners.clear();

    auto backgroundContentListenersVersionNumber = g_key_file_get_uint64(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);
    if (backgroundContentListenersVersionNumber != currentBackgroundContentListenerStateVersion) {
        RELEASE_LOG_DEBUG(Extensions, "Background listener version mismatch %" PUBLIC_LOG_STRING, backgroundContentListenersVersionNumber, " != %" PUBLIC_LOG_STRING, currentBackgroundContentListenerStateVersion);

        g_key_file_remove_key(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, nullptr);
        g_key_file_remove_key(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);

        writeStateToStorage();
        return;
    }

    HashCountedSet<unsigned> savedListeners;
    gsize listenersDataLength;
    auto* listenersData = g_key_file_get_integer_list(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, &listenersDataLength, nullptr);
    for (unsigned listener : unsafeMakeSpan(reinterpret_cast<unsigned*>(listenersData), listenersDataLength))
        savedListeners.add(listener);

    for (auto listener : savedListeners)
        m_backgroundContentEventListeners.add(static_cast<WebExtensionEventListenerType>(listener.key), listener.value);
}

void WebExtensionContext::saveBackgroundPageListenersToStorage()
{
    if (!storageIsPersistent() || protectedExtension()->backgroundContentIsPersistent())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Saving %" PUBLIC_LOG_STRING, m_backgroundContentEventListeners.size(), " background content event listeners to storage");

    Vector<unsigned> listeners;
    for (auto& entry : m_backgroundContentEventListeners)
        listeners.append(static_cast<unsigned>(entry.key));

    auto* newBackgroundPageListenersAsData = listeners.mutableSpan().data();
    gsize savedBackgroundPageListenersDataLength;
    auto* savedBackgroundPageListenersData = g_key_file_get_integer_list(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, &savedBackgroundPageListenersDataLength, nullptr);

    Vector<unsigned> savedBackgroundPageListeners;
    for (unsigned listener : unsafeMakeSpan(reinterpret_cast<unsigned*>(savedBackgroundPageListenersData), savedBackgroundPageListenersDataLength))
        savedBackgroundPageListeners.append(listener);
    g_key_file_set_integer_list(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, reinterpret_cast<gint*>(newBackgroundPageListenersAsData), listeners.size());

    auto savedListenerVersionNumber = g_key_file_get_uint64(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);
    g_key_file_set_uint64(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, currentBackgroundContentListenerStateVersion);

    bool hasListenerStateChanged = newBackgroundPageListenersAsData != reinterpret_cast<unsigned*>(savedBackgroundPageListenersData);
    bool hasVersionNumberChanged = savedListenerVersionNumber != currentBackgroundContentListenerStateVersion;
    if (hasListenerStateChanged || hasVersionNumberChanged)
        writeStateToStorage();
}

void WebExtensionContext::performTasksAfterBackgroundContentLoads()
{
    if (!isLoaded())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Background content loaded");

    if (m_shouldFireStartupEvent)
        m_shouldFireStartupEvent = false;

    if (m_installReason != InstallReason::None) {
        m_installReason = InstallReason::None;
        m_previousVersion = nullString();
    }

    RELEASE_LOG_DEBUG(Extensions, "Performing %{public}zu task(s) after background content loaded", m_actionsToPerformAfterBackgroundContentLoads.size());

    for (auto& action : m_actionsToPerformAfterBackgroundContentLoads)
        action();

    m_backgroundContentIsLoaded = true;
    m_actionsToPerformAfterBackgroundContentLoads.clear();

    saveBackgroundPageListenersToStorage();
    scheduleBackgroundContentToUnload();
}

bool WebExtensionContext::decidePolicyForNavigationAction(WebKitWebView *webView, WebKitNavigationPolicyDecision *navigationPolicy)
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    ASSERT(webView == m_backgroundWebView || isInspectorBackgroundPage(webView));
#else
    ASSERT(webView == m_backgroundWebView);
#endif

    WebKitNavigationAction* action = webkit_navigation_policy_decision_get_navigation_action(navigationPolicy);
    RefPtr navigationAction = webkitNavigationActionGetAction(action);

    auto url = URL { String::fromUTF8(webkit_uri_request_get_uri(webkit_navigation_action_get_request(action))) };
    if (!navigationAction->targetFrame()->isMainFrame() || isURLForThisExtension(url))
        return true;

    return false;
}

void WebExtensionContext::didFinishDocumentLoad(WebKitWebView *webView)
{
    if (webView != m_backgroundWebView)
        return;

    // The service worker will notify the load via a completion handler instead.
    if (protectedExtension()->backgroundContentIsServiceWorker())
        return;

    performTasksAfterBackgroundContentLoads();
}

void WebExtensionContext::didFailNavigation(WebKitWebView *webView, RefPtr<API::Error> error)
{
    if (webView != m_backgroundWebView)
        return;

    m_backgroundContentLoadError = createError(Error::BackgroundContentFailedToLoad, nullString(), error);
    recordErrorIfNeeded(backgroundContentLoadError());

    unloadBackgroundWebView();
}

void WebExtensionContext::webViewWebContentProcessDidTerminate(WebKitWebView *webView)
{
    if (webView == m_backgroundWebView) {
        unloadBackgroundWebView();

        if (protectedExtension()->backgroundContentIsPersistent())
            loadBackgroundWebView();

        return;
    }

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (isInspectorBackgroundPage(webView)) {
        webkit_web_view_load_request(m_backgroundWebView.get(), webkit_uri_request_new(inspectorBackgroundPageURL().utf8().data()));
        return;
    }
#endif

    ASSERT_NOT_REACHED();
}

#if ENABLE(INSPECTOR_EXTENSIONS)

HashSet<Ref<WebProcessProxy>> WebExtensionContext::processes(const API::InspectorExtension& inspectorExtension) const
{
    ASSERT(isLoaded());
    ASSERT(protectedExtension()->hasInspectorBackgroundPage());

    HashSet<Ref<WebProcessProxy>> result;

    RefPtr inspectorProxy = inspector(inspectorExtension);
    if (!inspectorProxy)
        return result;

    ASSERT(m_inspectorContextMap.contains(*inspectorProxy));

    const auto& inspectorContext = m_inspectorContextMap.get(*inspectorProxy);
    if (auto *backgroundWebView = inspectorContext.backgroundWebView.get()) {
        RefPtr pageProxy = webkitWebViewGetPage(backgroundWebView);
        result.add(pageProxy->siteIsolatedProcess());
    }

    return result;
}

#endif // ENABLE(INSPECTOR_EXTENSIONS)

String WebExtensionContext::declarativeNetRequestContentRuleListFilePath()
{
    if (!m_declarativeNetRequestContentRuleListFilePath.isEmpty())
        return m_declarativeNetRequestContentRuleListFilePath;

    auto directoryPath = storageIsPersistent() ? storageDirectory() : String(FileSystem::createTemporaryDirectory("DeclarativeNetRequest"_s));
    m_declarativeNetRequestContentRuleListFilePath = FileSystem::pathByAppendingComponent(directoryPath, "DeclarativeNetRequestContentRuleList.data"_s);

    return m_declarativeNetRequestContentRuleListFilePath;
}

void WebExtensionContext::setSessionStorageAllowedInContentScripts(bool allowed)
{
    m_isSessionStorageAllowedInContentScripts = allowed;

    g_key_file_set_boolean(m_state.get(), groupNameStateKey, sessionStorageAllowedInContentScriptsKey, allowed);

    writeStateToStorage();

    if (!isLoaded())
        return;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
