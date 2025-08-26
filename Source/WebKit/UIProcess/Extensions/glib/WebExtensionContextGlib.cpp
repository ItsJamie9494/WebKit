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
#include <wtf/URL.h>
#include <wtf/StdUnorderedMap.h>

#if ENABLE(WK_WEB_EXTENSIONS)

static constexpr auto groupNameStateKey = "ExtensionState"_s;
static constexpr auto backgroundContentEventListenersKey = "BackgroundContentEventListeners"_s;
static constexpr auto backgroundContentEventListenersVersionKey = "BackgroundContentEventListenersVersion"_s;
static constexpr auto lastSeenBaseURLStateKey = "LastSeenBaseURL"_s;
static constexpr auto lastSeenVersionStateKey = "LastSeenVersion"_s;
static constexpr auto lastSeenDisplayNameStateKey = "LastSeenDisplayName"_s;

static constexpr auto WebExtensionContextGrantedPermissionsWereRemovedSignal = "granted-permissions-were-removed"_s;
static constexpr auto WebExtensionContextGrantedPermissionMatchPatternsWereRemovedSignal = "granted-permission-match-patterns-were-removed"_s;
static constexpr auto WebExtensionContextDeniedPermissionsWereRemovedSignal = "denied-permissions-were-removed"_s;
static constexpr auto WebExtensionContextDeniedPermissionMatchPatternsWereRemovedSignal = "denied-permission-match-patterns-were-removed"_s;
static constexpr auto WebExtensionContextPermissionsWereDeniedSignal = "permissions-were-denied"_s;
static constexpr auto WebExtensionContextPermissionsWereGrantedSignal = "permissions-were-granted"_s;
static constexpr auto WebExtensionContextPermissionMatchPatternsWereDeniedSignal = "permission-match-pattern-were-denied"_s;
static constexpr auto WebExtensionContextPermissionMatchPatternsWereGrantedSignal = "permission-match-pattern-were-granted"_s;

// Update this value when any changes are made to the WebExtensionEventListenerType enum.
static constexpr auto currentBackgroundContentListenerStateVersion = 4;

namespace WebKit {

WebExtensionContext::WebExtensionContext(Ref<WebExtension>&& extension)
    : WebExtensionContext()
{
    m_extension = extension.ptr();
    m_baseURL = URL { makeString("webkit-extension://"_s, uniqueIdentifier(), '/') };

    // FIXME: ContextDelegate
}

WebExtensionContext::WebExtensionContext(Ref<WebExtension>&& extension, GRefPtr<WebKitWebExtensionContext>&& contextObject)
    : WebExtensionContext()
{
    m_extension = extension.ptr();
    m_baseURL = URL { makeString("webkit-extension://"_s, uniqueIdentifier(), '/') };
    m_delegate = contextObject;
}

GRefPtr<GKeyFile> WebExtensionContext::currentState() const
{
    return m_state;
}

GRefPtr<GKeyFile> WebExtensionContext::readStateFromPath(const String& stateFilePath)
{
    GRefPtr<GKeyFile> stateFile(g_key_file_new());
    GUniqueOutPtr<GError> error;

    g_key_file_load_from_file(stateFile.get(), stateFilePath.utf8().data(), G_KEY_FILE_NONE, &error.outPtr());
    if (error)
        RELEASE_LOG_ERROR(Extensions, "Failed to coordinate reading extension state: %" PUBLIC_LOG_STRING, error->message);

    return stateFile;
}

bool WebExtensionContext::readLastBaseURLFromState(const String& filePath, URL& outLastBaseURL)
{
    GRefPtr<GKeyFile> state(readStateFromPath(filePath));

    if (auto *baseURL = g_key_file_get_string(state.get(), groupNameStateKey, lastSeenBaseURLStateKey, nullptr))
        outLastBaseURL = URL { String::fromUTF8(baseURL) };

    return outLastBaseURL.isValid();
}

bool WebExtensionContext::readDisplayNameFromState(const String& filePath, String& outDisplayName)
{
    GRefPtr<GKeyFile> state(readStateFromPath(filePath));

    if (auto *displayName = g_key_file_get_string(state.get(), groupNameStateKey, lastSeenDisplayNameStateKey, nullptr))
        outDisplayName = String::fromUTF8(displayName);

    return outDisplayName.length();
}

GRefPtr<GKeyFile> WebExtensionContext::readStateFromStorage()
{
    if (!storageIsPersistent()) {
        if (!m_state) {
            GRefPtr<GKeyFile> stateFile(g_key_file_new());
            m_state = stateFile;
        }
        return m_state;
    }

    auto savedState = readStateFromPath(stateFilePath());
    m_state = savedState;
    return savedState;
}

void WebExtensionContext::writeStateToStorage() const {
    if (!storageIsPersistent())
        return;

    GUniqueOutPtr<GError> error;

    if (!g_key_file_save_to_file(currentState().get(), stateFilePath().utf8().data(), &error.outPtr()))
        RELEASE_LOG_ERROR(Extensions, "Unable to save extension state: %" PUBLIC_LOG_STRING, error->message);

    if (error)
        RELEASE_LOG_ERROR(Extensions, "Failed to coordinate writing extension state: %" PUBLIC_LOG_STRING, error->message);
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

        // Clear other state that is not persistent between extension updates.
        clearDeclarativeNetRequestRulesetState();
        clearRegisteredContentScripts();

        RELEASE_LOG_DEBUG(Extensions, "Queued installed event with extension update reason");
        m_installReason = InstallReason::ExtensionUpdate;
    } else if (!m_shouldFireStartupEvent) {
        RELEASE_LOG_DEBUG(Extensions, "Queued installed event with extension install reason");
        m_installReason = InstallReason::ExtensionInstall;
    } else
        m_installReason = InstallReason::None;
}

// void WebExtensionContext::loadBackgroundPageListenersFromStorage()
// {
//     if (!storageIsPersistent() || protectedExtension()->backgroundContentIsPersistent())
//         return;

//     m_backgroundContentEventListeners.clear();

//     auto backgroundContentListenersVersionNumber = g_key_file_get_uint64(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);
//     if (backgroundContentListenersVersionNumber != currentBackgroundContentListenerStateVersion) {
//         RELEASE_LOG_DEBUG(Extensions, "Background listener version mismatch %" PUBLIC_LOG_STRING, backgroundContentListenersVersionNumber, " != %" PUBLIC_LOG_STRING, currentBackgroundContentListenerStateVersion);

//         g_key_file_remove_key(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, nullptr);
//         g_key_file_remove_key(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);

//         writeStateToStorage();
//         return;
//     }

//     GUniquePtr<gint> listenerList(g_key_file_get_integer_list(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, nullptr, nullptr));
//     std::span<uint8_t> savedListeners(listenerList.get());
//     StdUnorderedMap<uint8_t, uint64_t> listenersCount;
//     for (auto entry : savedListeners)        
//         listenersCount[savedListeners[entry]]++;

//     for (const auto entry : savedListeners)
//         m_backgroundContentEventListeners.add(static_cast<WebExtensionEventListenerType>(entry), listenersCount[savedListeners[entry]]);
// }

// void WebExtensionContext::saveBackgroundPageListenersToStorage()
// {
//     if (!storageIsPersistent() || protectedExtension()->backgroundContentIsPersistent())
//         return;

//     RELEASE_LOG_DEBUG(Extensions, "Saving %" PUBLIC_LOG_STRING, m_backgroundContentEventListeners.size(), " background content event listeners to storage");

//     Vector<unsigned> listeners;
//     for (auto& entry : m_backgroundContentEventListeners)
//         listeners.append(static_cast<unsigned>(entry.key));

//     auto *newBackgroundPageListenersAsData = listeners.span().data();
//     auto savedBackgroundPageListenersData = g_key_file_get_integer_list(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, nullptr, nullptr);
//     Vector<unsigned> savedBackgroundPageListeners;
//     for (unsigned listener : span(const_cast<unsigned**>(savedBackgroundPageListenersData)))
//         savedBackgroundPageListeners.append(listener);
//     g_key_file_set_integer_list(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, static_cast<gint*>(newBackgroundPageListenersAsData), listeners.size());

//     auto backgroundContentListenersVersionNumber = g_key_file_get_uint64(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);
//     g_key_file_set_uint64(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, currentBackgroundContentListenerStateVersion);

//     bool hasListenerStateChanged = newBackgroundPageListenersAsData != savedBackgroundPageListenersAsData.data();
//     bool hasVersionNumberChanged = backgroundContentListenersVersionNumber != currentBackgroundContentListenerStateVersion;
//     if (hasListenerStateChanged || hasVersionNumberChanged)
//         writeStateToStorage();
// }

void WebExtensionContext::setInspectable(bool inspectable)
{
    m_inspectable = inspectable;

    // FIXME: background web view, tabs
}

void WebExtensionContext::setBackgroundWebViewInspectionName(const String& name)
{
    // FIXME: no-op
    // m_backgroundWebViewInspectionName = name;
    // m_backgroundWebView.get()._remoteInspectionNameOverride = name.createNSString().get();
}

const WebExtensionContext::PermissionsMap& WebExtensionContext::grantedPermissions()
{
    return removeExpired(m_grantedPermissions, m_nextGrantedPermissionsExpirationDate, WebExtensionContextGrantedPermissionsWereRemovedSignal);
}

const WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::grantedPermissionMatchPatterns()
{
    return removeExpired(m_grantedPermissionMatchPatterns, m_nextGrantedPermissionMatchPatternsExpirationDate, WebExtensionContextGrantedPermissionMatchPatternsWereRemovedSignal);
}

const WebExtensionContext::PermissionsMap& WebExtensionContext::deniedPermissions()
{
    return removeExpired(m_deniedPermissions, m_nextDeniedPermissionsExpirationDate, WebExtensionContextDeniedPermissionsWereRemovedSignal);
}

const WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::deniedPermissionMatchPatterns()
{
    return removeExpired(m_deniedPermissionMatchPatterns, m_nextDeniedPermissionMatchPatternsExpirationDate, WebExtensionContextDeniedPermissionMatchPatternsWereRemovedSignal);
}

void WebExtensionContext::grantPermissions(PermissionsSet&& permissions, WallTime expirationDate)
{
    ASSERT(!expirationDate.isNaN());

    if (permissions.isEmpty())
        return;

    if (m_nextGrantedPermissionsExpirationDate > expirationDate)
        m_nextGrantedPermissionsExpirationDate = expirationDate;

    PermissionsSet addedPermissions;
    for (auto& permission : permissions) {
        if (m_grantedPermissions.add(permission, expirationDate))
            addedPermissions.addVoid(permission);
    }

    if (addedPermissions.isEmpty())
        return;

    removeDeniedPermissions(addedPermissions);

    permissionsDidChange(WebExtensionContextPermissionsWereGrantedSignal, addedPermissions);
}

void WebExtensionContext::denyPermissions(PermissionsSet&& permissions, WallTime expirationDate)
{
    ASSERT(!expirationDate.isNaN());

    if (permissions.isEmpty())
        return;

    if (m_nextDeniedPermissionsExpirationDate > expirationDate)
        m_nextDeniedPermissionsExpirationDate = expirationDate;

    PermissionsSet addedPermissions;
    for (auto& permission : permissions) {
        if (m_deniedPermissions.add(permission, expirationDate))
            addedPermissions.addVoid(permission);
    }

    if (addedPermissions.isEmpty())
        return;

    removeGrantedPermissions(addedPermissions);

    permissionsDidChange(WebExtensionContextPermissionsWereDeniedSignal, addedPermissions);
}

void WebExtensionContext::permissionsDidChange(const String& notificationName, const PermissionsSet& permissions)
{
    if (permissions.isEmpty())
        return;
    // FIXME: loaded

    g_signal_emit_by_name(m_delegate.get(), makeString("notify::"_s, notificationName).utf8().data());
}

void WebExtensionContext::permissionsDidChange(const String& notificationName, const MatchPatternSet& matchPatterns)
{
    if (matchPatterns.isEmpty())
        return;

    clearCachedPermissionStates();
    // FIXME: loaded

    g_signal_emit_by_name(m_delegate.get(), makeString("notify::"_s, notificationName).utf8().data());
}

WebExtensionContext::PermissionsMap& WebExtensionContext::removeExpired(PermissionsMap& permissionMap, WallTime& nextExpirationDate, const String& notificationName)
{
    WallTime currentTime = WallTime::now();

    // If the next expiration date hasn't passed yet, there is nothing to remove.
    if (nextExpirationDate != WallTime::nan() && nextExpirationDate > currentTime)
        return permissionMap;

    nextExpirationDate = WallTime::infinity();

    PermissionsSet removedPermissions;
    permissionMap.removeIf([&](auto& entry) {
        if (entry.value <= currentTime) {
            removedPermissions.add(entry.key);
            return true;
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedPermissions.isEmpty() || !notificationName)
        return permissionMap;

    permissionsDidChange(notificationName, removedPermissions);

    return permissionMap;
}

WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::removeExpired(PermissionMatchPatternsMap& matchPatternMap, WallTime& nextExpirationDate, const String& notificationName)
{
    WallTime currentTime = WallTime::now();

    // If the next expiration date hasn't passed yet, there is nothing to remove.
    if (nextExpirationDate != WallTime::nan() && nextExpirationDate > currentTime)
        return matchPatternMap;

    nextExpirationDate = WallTime::infinity();

    MatchPatternSet removedMatchPatterns;
    matchPatternMap.removeIf([&](auto& entry) {
        if (entry.value <= currentTime) {
            removedMatchPatterns.add(entry.key);
            return true;
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedMatchPatterns.isEmpty() || !notificationName)
        return matchPatternMap;

    permissionsDidChange(notificationName, removedMatchPatterns);

    return matchPatternMap;
}

bool WebExtensionContext::removeGrantedPermissions(PermissionsSet& permissionsToRemove)
{
    return removePermissions(m_grantedPermissions, permissionsToRemove, m_nextGrantedPermissionsExpirationDate, WebExtensionContextGrantedPermissionsWereRemovedSignal);
}

bool WebExtensionContext::removeGrantedPermissionMatchPatterns(MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly)
{
    // Clear activeTab permissions if the patterns match.
    for (Ref tab : openTabs()) {
        auto temporaryPattern = tab->temporaryPermissionMatchPattern();
        if (!temporaryPattern)
            continue;

        for (auto& pattern : matchPatternsToRemove) {
            if (temporaryPattern->matchesPattern(pattern))
                tab->setTemporaryPermissionMatchPattern(nullptr);
        }
    }

    if (!removePermissionMatchPatterns(m_grantedPermissionMatchPatterns, matchPatternsToRemove, equalityOnly, m_nextGrantedPermissionMatchPatternsExpirationDate, WebExtensionContextGrantedPermissionMatchPatternsWereRemovedSignal))
        return false;

    removeInjectedContent(matchPatternsToRemove);

    return true;
}

bool WebExtensionContext::removeDeniedPermissions(PermissionsSet& permissionsToRemove)
{
    return removePermissions(m_deniedPermissions, permissionsToRemove, m_nextDeniedPermissionsExpirationDate, WebExtensionContextGrantedPermissionMatchPatternsWereRemovedSignal);
}

bool WebExtensionContext::removeDeniedPermissionMatchPatterns(MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly)
{
    if (!removePermissionMatchPatterns(m_deniedPermissionMatchPatterns, matchPatternsToRemove, equalityOnly, m_nextDeniedPermissionMatchPatternsExpirationDate, WebExtensionContextDeniedPermissionMatchPatternsWereRemovedSignal))
        return false;

    updateInjectedContent();

    return true;
}

void WebExtensionContext::grantPermissionMatchPatterns(MatchPatternSet&& permissionMatchPatterns, WallTime expirationDate, EqualityOnly equalityOnly)
{
    ASSERT(!expirationDate.isNaN());

    if (permissionMatchPatterns.isEmpty())
        return;

    if (m_nextGrantedPermissionMatchPatternsExpirationDate > expirationDate)
        m_nextGrantedPermissionMatchPatternsExpirationDate = expirationDate;

    MatchPatternSet addedMatchPatterns;
    for (auto& pattern : permissionMatchPatterns) {
        if (m_grantedPermissionMatchPatterns.add(pattern, expirationDate))
            addedMatchPatterns.addVoid(pattern);
    }

    if (addedMatchPatterns.isEmpty())
        return;

    removeDeniedPermissionMatchPatterns(addedMatchPatterns, equalityOnly);

    permissionsDidChange(WebExtensionContextPermissionMatchPatternsWereGrantedSignal, addedMatchPatterns);
}

void WebExtensionContext::denyPermissionMatchPatterns(MatchPatternSet&& permissionMatchPatterns, WallTime expirationDate, EqualityOnly equalityOnly)
{
    ASSERT(!expirationDate.isNaN());

    if (permissionMatchPatterns.isEmpty())
        return;

    if (m_nextDeniedPermissionMatchPatternsExpirationDate > expirationDate)
        m_nextDeniedPermissionMatchPatternsExpirationDate = expirationDate;

    MatchPatternSet addedMatchPatterns;
    for (auto& pattern : permissionMatchPatterns) {
        if (m_deniedPermissionMatchPatterns.add(pattern, expirationDate))
            addedMatchPatterns.addVoid(pattern);
    }

    if (addedMatchPatterns.isEmpty())
        return;

    removeGrantedPermissionMatchPatterns(addedMatchPatterns, equalityOnly);

    permissionsDidChange(WebExtensionContextPermissionMatchPatternsWereDeniedSignal, addedMatchPatterns);
}

bool WebExtensionContext::backgroundContentIsLoaded() const
{
    // FIXME: no-op
    return false;
    // return m_backgroundWebView && m_backgroundContentIsLoaded && m_actionsToPerformAfterBackgroundContentLoads.isEmpty();
}

void WebExtensionContext::loadBackgroundWebViewIfNeeded()
{
    // FIXME: no-op
    // ASSERT(isLoaded());

    // if (!protectedExtension()->hasBackgroundContent() || m_backgroundWebView || !safeToLoadBackgroundContent())
    //     return;

    // loadBackgroundWebView();
}

void WebExtensionContext::scheduleBackgroundContentToUnload()
{
    // FIXME: no-op
//     if (!m_backgroundWebView || protectedExtension()->backgroundContentIsPersistent())
//         return;

// #ifdef NDEBUG
//     static const auto testRunnerDelayBeforeUnloading = 3_s;
// #else
//     static const auto testRunnerDelayBeforeUnloading = 6_s;
// #endif

//     static const auto delayBeforeUnloading = isNotRunningInTestRunner() ? 30_s : testRunnerDelayBeforeUnloading;

//     RELEASE_LOG_DEBUG(Extensions, "Scheduling background content to unload in %{public}.0f seconds", delayBeforeUnloading.seconds());

//     if (!m_unloadBackgroundWebViewTimer)
//         m_unloadBackgroundWebViewTimer = makeUnique<RunLoop::Timer>(RunLoop::currentSingleton(), "WebExtensionContext::UnloadBackgroundWebViewTimer"_s, this, &WebExtensionContext::unloadBackgroundContentIfPossible);
//     m_unloadBackgroundWebViewTimer->startOneShot(delayBeforeUnloading);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
