/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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
#include "WebExtension.h"

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

static constexpr auto manifestVersionManifestKey = "manifest_version"_s;

static constexpr auto nameManifestKey = "name"_s;
static constexpr auto shortNameManifestKey = "short_name"_s;
static constexpr auto versionManifestKey = "version"_s;
static constexpr auto versionNameManifestKey = "version_name"_s;
static constexpr auto descriptionManifestKey = "description"_s;

static constexpr auto commandsManifestKey = "commands"_s;
static constexpr auto commandsSuggestedKeyManifestKey = "suggested_key"_s;
static constexpr auto commandsDescriptionKeyManifestKey = "description"_s;

static const size_t maximumNumberOfShortcutCommands = 4;

bool WebExtension::manifestParsedSuccessfully()
{
    if (m_parsedManifest)
        return !!m_manifestJSON->asObject();

    // If we haven't parsed yet, trigger a parse by calling the getter.
    return !!manifest() && !!manifestObject();
}

double WebExtension::manifestVersion()
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return 0;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/manifest_version
    if (auto value = manifestObject->getDouble(manifestVersionManifestKey))
        return *value;

    return 0;
}

const String& WebExtension::displayName()
{
    populateDisplayStringsIfNeeded();
    return m_displayName;
}

const String& WebExtension::displayShortName()
{
    populateDisplayStringsIfNeeded();
    return m_displayShortName;
}

const String& WebExtension::displayVersion()
{
    populateDisplayStringsIfNeeded();
    return m_displayVersion;
}

const String& WebExtension::displayDescription()
{
    populateDisplayStringsIfNeeded();
    return m_displayDescription;
}

const String& WebExtension::version()
{
    populateDisplayStringsIfNeeded();
    return m_version;
}

void WebExtension::populateDisplayStringsIfNeeded()
{
    if (m_parsedManifestDisplayStrings)
        return;

    m_parsedManifestDisplayStrings = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/name

    m_displayName = manifestObject->getString(nameManifestKey);
    m_displayShortName = manifestObject->getString(shortNameManifestKey);

    if (m_displayShortName.isEmpty())
        m_displayShortName = m_displayName;

    if (m_displayName.isEmpty())
        recordError(createError(Error::InvalidName));

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/version

    m_version = manifestObject->getString(versionManifestKey);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/version_name

    m_displayVersion = manifestObject->getString(versionNameManifestKey);

    if (m_displayVersion.isEmpty())
        m_displayVersion = m_version;

    if (m_version.isEmpty())
        recordError(createError(Error::InvalidVersion));

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/description

    m_displayDescription = manifestObject->getString(descriptionManifestKey);
    if (m_displayDescription.isEmpty())
        recordError(createError(Error::InvalidDescription));
}

const WebExtension::CommandsVector& WebExtension::commands()
{
    populateCommandsIfNeeded();
    return m_commands;
}

bool WebExtension::hasCommands()
{
    populateCommandsIfNeeded();
    return !m_commands.isEmpty();
}

using ModifierFlags = WebExtension::ModifierFlags;

static bool parseCommandShortcut(const String& shortcut, OptionSet<ModifierFlags>& modifierFlags, String& key)
{
    modifierFlags = { };
    key = emptyString();

    // An empty shortcut is allowed.
    if (shortcut.isEmpty())
        return true;

    static NeverDestroyed<HashMap<String, ModifierFlags>> modifierMap = HashMap<String, ModifierFlags> {
        { "Ctrl"_s, ModifierFlags::Command },
        { "Command"_s, ModifierFlags::Command },
        { "Alt"_s, ModifierFlags::Option },
        { "MacCtrl"_s, ModifierFlags::Control },
        { "Shift"_s, ModifierFlags::Shift }
    };

    static NeverDestroyed<HashMap<String, String>> specialKeyMap = HashMap<String, String> {
        { "Comma"_s, ","_s },
        { "Period"_s, "."_s },
        { "Space"_s, " "_s },
        { "F1"_s, "\\uF704"_s },
        { "F2"_s, "\\uF705"_s },
        { "F3"_s, "\\uF706"_s },
        { "F4"_s, "\\uF707"_s },
        { "F5"_s, "\\uF708"_s },
        { "F6"_s, "\\uF709"_s },
        { "F7"_s, "\\uF70A"_s },
        { "F8"_s, "\\uF70B"_s },
        { "F9"_s, "\\uF70C"_s },
        { "F10"_s, "\\uF70D"_s },
        { "F11"_s, "\\uF70E"_s },
        { "F12"_s, "\\uF70F"_s },
        { "Insert"_s, "\\uF727"_s },
        { "Delete"_s, "\\uF728"_s },
        { "Home"_s, "\\uF729"_s },
        { "End"_s, "\\uF72B"_s },
        { "PageUp"_s, "\\uF72C"_s },
        { "PageDown"_s, "\\uF72D"_s },
        { "Up"_s, "\\uF700"_s },
        { "Down"_s, "\\uF701"_s },
        { "Left"_s, "\\uF702"_s },
        { "Right"_s, "\\uF703"_s }
    };

    auto parts = shortcut.split('+');

    // Reject shortcuts with fewer than two or more than three components.
    if (parts.size() < 2 || parts.size() > 3)
        return false;

    key = parts.takeLast();

    // Keys should not be present in the modifier map.
    if (modifierMap.get().contains(key))
        return false;

    if (key.length() == 1) {
        // Single-character keys must be alphanumeric.
        if (!isASCIIAlphanumeric(key[0]))
            return false;

        key = key.convertToASCIILowercase();
    } else {
        auto entry = specialKeyMap.get().find(key);

        // Non-alphanumeric keys must be in the special key map.
        if (entry == specialKeyMap.get().end())
            return false;

        key = entry->value;
    }

    for (auto& part : parts) {
        // Modifiers must exist in the modifier map.
        if (!modifierMap.get().contains(part))
            return false;

        modifierFlags.add(modifierMap.get().get(part));
    }

    // At least one valid modifier is required.
    if (!modifierFlags)
        return false;

    return true;
}

void WebExtension::populateCommandsIfNeeded()
{
    if (m_parsedManifestCommands)
        return;

    m_parsedManifestCommands = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/commands

    auto commandsObject = manifestObject->getObject(commandsManifestKey);
    if (!commandsObject) {
        recordError(createError(Error::InvalidCommands));
        return;
    }

    size_t commandsWithShortcuts = 0;
    std::optional<String> error;

    bool hasActionCommand = false;

    for (auto commandIdentifier : commandsObject->keys()) {
        if (commandIdentifier.isEmpty()) {
            error = WEB_UI_STRING("Empty or invalid identifier in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command identifier");
            continue;
        }

        auto commandDictionary = commandsObject->getObject(commandIdentifier);
        if (!commandDictionary->size()) {
            error = WEB_UI_STRING("Empty or invalid command in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command");
            continue;
        }

        CommandData commandData;
        commandData.identifier = commandIdentifier;
        commandData.activationKey = emptyString();
        commandData.modifierFlags = { };

        bool isActionCommand = false;
        if (supportsManifestVersion(3) && commandData.identifier == "_execute_action"_s)
            isActionCommand = true;
        else if (!supportsManifestVersion(3) && (commandData.identifier == "_execute_browser_action"_s || commandData.identifier == "_execute_page_action"_s))
            isActionCommand = true;

        if (isActionCommand && !hasActionCommand)
            hasActionCommand = true;

        // Descriptions are required for standard commands, but are optional for action commands.
        auto description = commandDictionary->getString(commandsDescriptionKeyManifestKey);
        if (description.isEmpty() && !isActionCommand) {
            error = WEB_UI_STRING("Empty or invalid `description` in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command description");
            continue;
        }

        if (isActionCommand && !description.isEmpty()) {
            description = displayActionLabel();
            if (!description.isEmpty())
                description = displayShortName();
        }

        commandData.description = description;

        if (auto suggestedKeyDictionary = commandDictionary->getObject(commandsSuggestedKeyManifestKey)) {
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
            const auto macPlatform = "mac"_s;
            const auto iosPlatform = "ios"_s;
#elif PLATFORM(GTK)
            const auto linuxPlatform = "linux"_s;
#endif
            const auto defaultPlatform = "default"_s;

#if PLATFORM(MAC)
            auto platformShortcut = !suggestedKeyDictionary->getString(macPlatform).isEmpty() ? suggestedKeyDictionary->getString(macPlatform) : suggestedKeyDictionary->getString(iosPlatform);
#elif PLATFORM(IOS_FAMILY)
            auto platformShortcut = !suggestedKeyDictionary->getString(iosPlatform).isEmpty() ? suggestedKeyDictionary->getString(iosPlatform) : suggestedKeyDictionary->getString(macPlatform);
#elif PLATFORM(GTK)
            auto platformShortcut = suggestedKeyDictionary->getString(linuxPlatform);
#else
            auto platformShortcut = suggestedKeyDictionary->getString(defaultPlatform);
#endif
            if (platformShortcut.isEmpty())
                platformShortcut = suggestedKeyDictionary->getString(defaultPlatform);

            if (!parseCommandShortcut(platformShortcut, commandData.modifierFlags, commandData.activationKey)) {
                error = WEB_UI_STRING("Invalid `suggested_key` in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command shortcut");
                continue;
            }

            if (!commandData.activationKey.isEmpty() && ++commandsWithShortcuts > maximumNumberOfShortcutCommands) {
                error = WEB_UI_STRING("Too many shortcuts specified for `commands`, only 4 shortcuts are allowed.", "WKWebExtensionErrorInvalidManifestEntry description for too many command shortcuts");
                commandData.activationKey = emptyString();
                commandData.modifierFlags = { };
            }
        }

        m_commands.append(WTFMove(commandData));
    }

    if (!hasActionCommand) {
        String commandIdentifier;
        if (hasAction())
            commandIdentifier = "_execute_action"_s;
        else if (hasBrowserAction())
            commandIdentifier = "_execute_browser_action"_s;
        else if (hasPageAction())
            commandIdentifier = "_execute_page_action"_s;

        if (!commandIdentifier.isEmpty())
            m_commands.append({ commandIdentifier, displayActionLabel(), emptyString(), { } });
    }

    if (error)
        recordError(createError(Error::InvalidCommands, *error));
}

} // namespace WebKit

#endif // WK_WEB_EXTENSIONS
