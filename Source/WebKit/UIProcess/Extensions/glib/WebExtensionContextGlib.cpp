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
#include "WebExtensioNContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

static constexpr auto groupNameStateKey = "ExtensionState"_s;
static constexpr auto backgroundContentEventListenersKey = "BackgroundContentEventListeners"_s;
static constexpr auto backgroundContentEventListenersVersionKey = "BackgroundContentEventListenersVersion"_s;
static constexpr auto lastSeenBaseURLStateKey = "LastSeenBaseURL"_s;
static constexpr auto lastSeenVersionStateKey = "LastSeenVersion"_s;
static constexpr auto lastSeenDisplayNameStateKey = "LastSeenDisplayName"_s;

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

GUniquePtr<GKeyFile> WebExtensionContext::currentState() const
{
    return m_state;
}

GUniquePtr<GKeyFile> WebExtensionContext::readStateFromPath(const String& stateFilePath)
{
    GUniquePtr<GKeyFile> stateFile(g_key_file_new());
    GUniqueOutPtr<GError> error;

    g_key_file_load_from_data(stateFile.get(), stateFilePath, G_KEY_FILE_NONE, &error.outPtr());
    if (error)
        RELEASE_LOG_ERROR(Extensions, "Failed to coordinate reading extension state: %" PUBLIC_LOG_STRING error.value())

    return stateFile;
}

bool WebExtensionContext::readLastBaseURLFromState(const String& filePath, URL& outLastBaseURL)
{
    GUniquePtr<GKeyFile> state(readStateFromPath(filePath));

    if (auto *baseURL = g_key_file_get_string(state.get(), groupNameStateKey, lastSeenBaseURLStateKey, nullptr))
        outLastBaseURL = URL { baseURL };

    return outLastBaseURL.isValid();
}

bool WebExtensionContext::readDisplayNameFromState(const String& filePath, String& outDisplayName)
{
    GUniquePtr<GKeyFile> state(readStateFromPath(filePath));

    if (auto *displayName = g_key_file_get_string(state.get(), groupNameStateKey, lastSeenDisplayNameStateKey, nullptr))
        outDisplayName = displayName;

    return outDisplayName.isValid();
}

GUniquePtr<GKeyFile> WebExtensionContext::readStateFromStorage()
{
    if (!storageIsPersistent()) {
        if (!m_state) {
            GUniquePtr<GKeyFile> stateFile(g_key_file_new());
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

    if (!g_key_file_save_to_file(currentState().get(), stateFilePath(), &error.outPtr()))
        RELEASE_LOG_ERROR(Extensions, "Unable to save extension state: %" PUBLIC_LOG_STRING error.value())

    if (error)
        RELEASE_LOG_ERROR(Extensions, "Failed to coordinate writing extension state: %" PUBLIC_LOG_STRING error.value())
}

void WebExtensionContext::determineInstallReasonDuringLoad()
{
    ASSERT(isLoaded());

    RefPtr extension = m_extension;
    String currentVersion = extension->version();
    GUniquePtr<gchar> previousVersion(g_key_file_get_string(m_state.get(), groupNameStateKey, lastSeenVersionStateKey, nullptr));
    m_previousVersion = previousVersion;
    g_key_file_set_string(m_state.get(), groupNameStateKey, lastSeenVersionStateKey, currentVersion);

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

void WebExtensionContext::loadBackgroundPageListenersFromStorage()
{
    if (!storageIsPersistent() || protectedExtension()->backgroundContentIsPersistent())
        return;

    m_backgroundContentEventListeners.clear();

    auto backgroundContentListenersVersionNumber = g_key_file_get_uint(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);
    if (backgroundContentListenersVersionNumber != currentBackgroundContentListenerStateVersion) {
        RELEASE_LOG_DEBUG(Extensions, "Background listener version mismatch %" PUBLIC_LOG_STRING backgroundContentListenersVersionNumber " != %" PUBLIC_LOG_STRING currentBackgroundContentListenerStateVersion);

        g_key_file_remove_key(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, nullptr);
        g_key_file_remove_key(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);

        writeStateToStorage();
        return;
    }

    GUniquePtr<gint> listenersData(g_key_file_get_integer_list(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, nullptr, nullptr));
    std::span<uint8_t> savedListeners(listenersData.get());
    StdUnorderedMap<uint8_t, uint64_t> listenersCount;
    for (auto entry : savedListeners)        
        listenersCount[savedListeners[entry]]++;

    for (const auto entry : savedListeners)
        m_backgroundContentEventListeners.add(static_cast<WebExtensionEventListenerType>(entry), listenersCount[savedListeners[entry]]);
}

void WebExtensionContext::saveBackgroundPageListenersToStorage()
{
    if (!storageIsPersistent() || protectedExtension()->backgroundContentIsPersistent())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Saving %" PUBLIC_LOG_STRING m_backgroundContentEventListeners.size(), " background content event listeners to storage");

    Vector<unsigned> listeners;
    for (auto& entry : m_backgroundContentEventListeners)
        listeners.append(static_cast<unsigned>(entry.key));

    auto *newBackgroundPageListenersAsData = listeners.span().data();
    auto *savedBackgroundPageListenersAsData = objectForKey<NSData>(m_state, backgroundContentEventListenersKey);
    GUniquePtr<gint> savedBackgroundPageListeners(g_key_file_get_integer_list(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, nullptr, nullptr));
    std::span<uint8_t> savedBackgroundPageListenersAsData(savedBackgroundPageListeners.get());
    g_key_file_set_integer_list(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, newBackgroundPageListenersAsData, listeners.count());

    auto backgroundContentListenersVersionNumber = g_key_file_get_uint(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);
    g_key_file_set_uint(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, currentBackgroundContentListenerStateVersion, nullptr);

    bool hasListenerStateChanged = newBackgroundPageListenersAsData != savedBackgroundPageListenersAsData.data();
    bool hasVersionNumberChanged = backgroundContentListenersVersionNumber != currentBackgroundContentListenerStateVersion;
    if (hasListenerStateChanged || hasVersionNumberChanged)
        writeStateToStorage();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
