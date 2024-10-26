/*
 * Copyright (C) 2024 Igalia, S.L. All rights reserved.
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
#include "WebExtensionLocalization.h"

#include "Logging.h"
#include "WebExtension.h"
#include "WebExtensionUtilities.h"
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/Language.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

#if ENABLE(WK_WEB_EXTENSIONS)

static constexpr auto messageKey = "message"_s;
static constexpr auto placeholdersKey = "placeholders"_s;
static constexpr auto placeholderDictionaryContentKey = "content"_s;

static constexpr auto predefinedMessageUILocale = "@@ui_locale"_s;
static constexpr auto predefinedMessageLanguageDirection = "@@bidi_dir"_s;
static constexpr auto predefinedMessageLanguageDirectionReversed = "@@bidi_reversed_dir"_s;
static constexpr auto predefinedMessageTextLeadingEdge = "@@bidi_start_edge"_s;
static constexpr auto predefinedMessageTextTrailingEdge = "@@bidi_end_edge"_s;
static constexpr auto predefinedMessageExtensionID = "@@extension_id"_s;

static constexpr auto predefinedMessageValueLeftToRight = "ltr"_s;
static constexpr auto predefinedMessageValueRightToLeft = "rtl"_s;
static constexpr auto predefinedMessageValueTextEdgeLeft = "left"_s;
static constexpr auto predefinedMessageValueTextEdgeRight = "right"_s;

WebExtensionLocalization::WebExtensionLocalization(WebExtension& webExtension)
{
    auto defaultLocaleString = webExtension.defaultLocale();
    if (defaultLocaleString.isEmpty()) {
        RELEASE_LOG_DEBUG(Extensions, "No default locale provided");
        return;
    }

    auto defaultLocaleJSON = localizationJSONForWebExtension(webExtension, defaultLocaleString);
    if (!defaultLocaleJSON) {
        RELEASE_LOG_DEBUG(Extensions, "No localization found for default locale %{public}", defaultLocaleString);
        return;
    }

    RELEASE_LOG_DEBUG(Extensions, "Loaded default locale %{public}", defaultLocaleString);

    auto bestLocaleString = webExtension.bestMatchLocale();
    auto defaultLocaleComponents = parseLocale(defaultLocaleString);
    auto bestLocaleComponents = parseLocale(bestLocaleString);
    auto bestLocaleLanguageOnlyString = bestLocaleComponents.languageCode;

    RELEASE_LOG_DEBUG(Extensions, "Best locale is %{public}", bestLocaleString);

    RefPtr<JSON::Object> languageJSON;
    if (!bestLocaleLanguageOnlyString.isEmpty() && bestLocaleLanguageOnlyString != defaultLocaleString) {
        languageJSON = localizationJSONForWebExtension(webExtension, bestLocaleLanguageOnlyString);
        if (languageJSON)
            RELEASE_LOG_DEBUG(Extensions, "Loaded language-only locale %{public}", bestLocaleLanguageOnlyString);
    }

    RefPtr<JSON::Object> regionalJSON;
    if (bestLocaleString != bestLocaleLanguageOnlyString && bestLocaleString != defaultLocaleString) {
        regionalJSON = localizationJSONForWebExtension(webExtension, bestLocaleString);
        if (regionalJSON)
            RELEASE_LOG_DEBUG(Extensions, "Loaded regional locale %{public}", bestLocaleString);
    }

    loadRegionalLocalization(regionalJSON, languageJSON, defaultLocaleJSON, bestLocaleString);
}

WebExtensionLocalization::WebExtensionLocalization(RefPtr<JSON::Object> localizedJSON, const String& uniqueIdentifier)
{
    ASSERT(!uniqueIdentifier.isEmpty());
    ASSERT(localizedJSON);

    String localeString = nullString();
    if (auto predefinedMessages = localizedJSON->getObject(predefinedMessageUILocale))
        localeString = predefinedMessages->getString(messageKey);

    loadRegionalLocalization(localizedJSON, nullptr, nullptr);
}

WebExtensionLocalization::WebExtensionLocalization(RefPtr<JSON::Object> regionalLocalization, RefPtr<JSON::Object> languageLocalization, RefPtr<JSON::Object> defaultLocalization, const String& withBestLocale, const String& uniqueIdentifier)
{
    loadRegionalLocalization(regionalLocalization, languageLocalization, defaultLocalization, withBestLocale, uniqueIdentifier);
}

void WebExtensionLocalization::loadRegionalLocalization(RefPtr<JSON::Object> regionalLocalization, RefPtr<JSON::Object> languageLocalization, RefPtr<JSON::Object> defaultLocalization, const String& withBestLocale, const String& uniqueIdentifier)
{
    m_localeString = withBestLocale;
    m_uniqueIdentifier = uniqueIdentifier;

    RefPtr<JSON::Object> localizationJSON = predefinedMessages();
    localizationJSON = JSONWithLowercaseKeys(localizationJSON);
    localizationJSON = mergeJSON(localizationJSON, JSONWithLowercaseKeys(regionalLocalization));
    localizationJSON = mergeJSON(localizationJSON, JSONWithLowercaseKeys(languageLocalization));
    localizationJSON = mergeJSON(localizationJSON, JSONWithLowercaseKeys(defaultLocalization));

    ASSERT(localizationJSON);

    m_localizationJSON = localizationJSON;

    return;
}

RefPtr<JSON::Object> WebExtensionLocalization::localizedJSONforJSON(RefPtr<JSON::Object> json)
{
    if (!json)
        return nullptr;

    RefPtr<JSON::Object> localizedJSON = JSON::Object::create();

    for (String key : json->keys()) {
        auto valueType = json->getValue(key)->type();

        if (valueType == JSON::Value::Type::String)
            localizedJSON->setString(key, localizedStringForString(json->getString(key)));
        else if (valueType == JSON::Value::Type::Array)
            localizedJSON->setArray(key, *localizedArrayForArray(json->getArray(key)));
        else if (valueType == JSON::Value::Type::Object)
            localizedJSON->setObject(key, *localizedJSONforJSON(json->getObject(key)));
    }

    return localizedJSON;
}

String WebExtensionLocalization::localizedStringForKey(String key, Vector<String> placeholders)
{
    if (placeholders.size() > 9)
        return nullString();

    if (!m_localizationJSON || !m_localizationJSON->size())
        return emptyString();

    RefPtr<JSON::Object> stringJSON = m_localizationJSON->getObject(key.convertToASCIILowercase());

    String localizedString = stringJSON->getString(messageKey);
    if (!localizedString.length())
        return emptyString();

    RefPtr<JSON::Object> namedPlaceholders = JSONWithLowercaseKeys(stringJSON->getObject(placeholdersKey)); // FIXME: validate that this won't crash

    localizedString = stringByReplacingNamedPlaceholdersInString(localizedString, namedPlaceholders);
    localizedString = stringByReplacingPositionalPlaceholdersInString(localizedString, placeholders);

    localizedString = localizedString.impl()->replace("$$"_s, "$"_s);

    return localizedString;
}

RefPtr<JSON::Array> WebExtensionLocalization::localizedArrayForArray(RefPtr<JSON::Array> json)
{
    if (!json)
        return nullptr;

    RefPtr<JSON::Array> localizedJSON = JSON::Array::create();

    int index = 0;
    for (auto key : *json) {
        auto valueType = key->type();

        if (valueType == JSON::Value::Type::String)
            localizedJSON->pushString(localizedStringForString(json->get(index)->asString()));
        else if (valueType == JSON::Value::Type::Array)
            localizedJSON->pushArray(*localizedArrayForArray(json->get(index)->asArray()));
        else if (valueType == JSON::Value::Type::Object)
            localizedJSON->pushObject(*localizedJSONforJSON(json->get(index)->asObject()));

        index += 1;
    }

    return localizedJSON;
}

String WebExtensionLocalization::localizedStringForString(String sourceString)
{
    String localizedString = sourceString;

    JSC::Yarr::RegularExpression localizableStringRegularExpression = JSC::Yarr::RegularExpression("__MSG_([A-Za-z0-9_@]+)__"_s, { }); // FIXME: Using JSC seems awful here

    int index = 0;
    while (index < static_cast<int>(localizedString.length())) {
        int matchLength;
        index = localizableStringRegularExpression.match(sourceString, index, &matchLength);
        if (index < 0)
            break;

        String key = sourceString.substring(index, matchLength);
        String localizedReplacement = localizedStringForKey(key);

        localizedString = sourceString.impl()->replace(key, localizedReplacement);

        index += localizedReplacement.length();
    }

    return localizedString;
}

RefPtr<JSON::Object> WebExtensionLocalization::localizationJSONForWebExtension(WebExtension& webExtension, const String& withLocale)
{
    StringBuilder pathBuilder;
    pathBuilder.append("_locales/"_s);
    pathBuilder.append(m_localeString);
    pathBuilder.append("/messages.json"_s);
    String path = pathBuilder.toString();

    RefPtr<API::Error> error;
    RefPtr data = webExtension.resourceDataForPath(path, error, WebExtension::CacheResult::No, WebExtension::SuppressNotFoundErrors::Yes);
    if (!data || error) {
        webExtension.recordErrorIfNeeded(error);
        return nullptr;
    }

    return JSON::Value::parseJSON(String::fromUTF8(data->span()))->asObject();
}

RefPtr<JSON::Object> WebExtensionLocalization::predefinedMessages()
{
    RefPtr<JSON::Object> predefinedMessages;

    auto createMessageKey = [](String messageValue) {
        Ref<JSON::Object> object = JSON::Object::create();
        object->setString(messageKey, messageValue);
        return object;
    };

    // Get the first message we can and hope we can get a writing direction
    if (auto firstMessageObject = m_localizationJSON->getObject(*(m_localizationJSON->keys().begin()))) {
        auto firstMessage = firstMessageObject->getString(messageKey);

        if (auto writingDirection = firstMessage.defaultWritingDirection(); writingDirection == U_LEFT_TO_RIGHT) {
            predefinedMessages->setObject(predefinedMessageLanguageDirection, createMessageKey(predefinedMessageValueLeftToRight));
            predefinedMessages->setObject(predefinedMessageLanguageDirectionReversed, createMessageKey(predefinedMessageValueRightToLeft));
            predefinedMessages->setObject(predefinedMessageTextLeadingEdge, createMessageKey(predefinedMessageValueTextEdgeLeft));
            predefinedMessages->setObject(predefinedMessageTextTrailingEdge, createMessageKey(predefinedMessageValueTextEdgeRight));
        } else {
            predefinedMessages->setObject(predefinedMessageLanguageDirection, createMessageKey(predefinedMessageValueRightToLeft));
            predefinedMessages->setObject(predefinedMessageLanguageDirectionReversed, createMessageKey(predefinedMessageValueLeftToRight));
            predefinedMessages->setObject(predefinedMessageTextLeadingEdge, createMessageKey(predefinedMessageValueTextEdgeRight));
            predefinedMessages->setObject(predefinedMessageTextTrailingEdge, createMessageKey(predefinedMessageValueTextEdgeLeft));
        }
    }

    if (!m_localeString.isNull())
        predefinedMessages->setString(predefinedMessageUILocale, m_localeString);

    if (!m_uniqueIdentifier.isNull())
        predefinedMessages->setString(predefinedMessageExtensionID, m_uniqueIdentifier);

    return predefinedMessages;
}

String WebExtensionLocalization::stringByReplacingNamedPlaceholdersInString(String sourceString, RefPtr<JSON::Object> placeholders)
{
    String localizedString = sourceString;

    JSC::Yarr::RegularExpression localizableStringRegularExpression = JSC::Yarr::RegularExpression("(?:[^$]|^)(\\$([A-Za-z0-9_@]+)\\$)"_s, { }); // FIXME: Using JSC seems awful here

    int index = 0;
    while (index < static_cast<int>(localizedString.length())) {
        int matchLength;
        index = localizableStringRegularExpression.match(sourceString, index, &matchLength);
        if (index < 0)
            break;

        String key = sourceString.substring(index, matchLength).convertToASCIILowercase();
        String localizedReplacement = placeholders->getObject(key) ? placeholders->getObject(key)->getString(placeholderDictionaryContentKey) : emptyString();

        localizedString = sourceString.impl()->replace(sourceString.substring(index, matchLength), localizedReplacement);

        index += localizedReplacement.length();
    }

    return localizedString;
}

String WebExtensionLocalization::stringByReplacingPositionalPlaceholdersInString(String sourceString, Vector<String> placeholders)
{
    String localizedString = sourceString;

    JSC::Yarr::RegularExpression localizableStringRegularExpression = JSC::Yarr::RegularExpression("(?:[^$]|^)(\\$([0-9]))"_s, { }); // FIXME: Using JSC seems awful here

    int index = 0;
    int placeholderIndex = 0;
    while (index < static_cast<int>(localizedString.length())) {
        int matchLength;
        index = localizableStringRegularExpression.match(sourceString, index, &matchLength);
        if (index < 0)
            break;

        String replacement;
        if (placeholderIndex > 0 && (size_t) placeholderIndex <= placeholders.size()) {
            replacement = placeholders[placeholderIndex];
            placeholderIndex += 1;
        } else
            replacement = emptyString();

        localizedString = sourceString.impl()->replace(sourceString.substring(index, matchLength), replacement);

        index += replacement.length();
    }

    return localizedString;
}

#endif

} // namespace WebKit
