/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#include "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionContextParameters.h"
#include "WebExtensionContextProxyMessages.h"
#include "WebExtensionController.h"
#include "WebPageProxy.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

using namespace WebCore;

static HashMap<WebExtensionContextIdentifier, WeakRef<WebExtensionContext>>& webExtensionContexts()
{
    static NeverDestroyed<HashMap<WebExtensionContextIdentifier, WeakRef<WebExtensionContext>>> contexts;
    return contexts;
}

WebExtensionContext* WebExtensionContext::get(WebExtensionContextIdentifier identifier)
{
    return webExtensionContexts().get(identifier);
}

WebExtensionContext::WebExtensionContext()
{
    ASSERT(!get(identifier()));
    webExtensionContexts().add(identifier(), *this);
}

Ref<API::Error> WebExtensionContext::createError(Error error, const String& customLocalizedDescription, RefPtr<API::Error> underlyingError)
{
    auto errorCode = toAPI(error);
    String localizedDescription;

    switch (error) {
    case Error::Unknown:
        localizedDescription = WEB_UI_STRING_KEY("An unknown error has occurred.", "An unknown error has occurred. (WKWebExtensionContext)", "WKWebExtensionContextErrorUnknown description");
        break;

    case Error::AlreadyLoaded:
        localizedDescription = WEB_UI_STRING("Extension context is already loaded.", "WKWebExtensionContextErrorAlreadyLoaded description");
        break;

    case Error::NotLoaded:
        localizedDescription = WEB_UI_STRING("Extension context is not loaded.", "WKWebExtensionContextErrorNotLoaded description");
        break;

    case Error::BaseURLAlreadyInUse:
        localizedDescription = WEB_UI_STRING("Another extension context is loaded with the same base URL.", "WKWebExtensionContextErrorBaseURLAlreadyInUse description");
        break;

    case Error::NoBackgroundContent:
        localizedDescription = WEB_UI_STRING("No background content is available to load.", "WKWebExtensionContextErrorNoBackgroundContent description");
        break;

    case Error::BackgroundContentFailedToLoad:
        localizedDescription = WEB_UI_STRING("The background content failed to load due to an error.", "WKWebExtensionContextErrorBackgroundContentFailedToLoad description");
        break;
    }
    
    if (!customLocalizedDescription.isEmpty())
        localizedDescription = customLocalizedDescription;
    
    return API::Error::create({ "WKWebExtensionContextErrorDomain"_s, errorCode, { }, localizedDescription });
}

Vector<Ref<API::Error>> errors()
{
    Vector<Ref<Error>> array;
    array.appendVector(m_errors);
    array.appendVector(protectedExtension()->errors());
    
    return array;
}

String WebExtensionContext::stateFilePath() const
{
    if (!storageIsPersistent())
        return nullString();
    return FileSystem::pathByAppendingComponent(storageDirectory(), plistFileName());
}

void WebExtensionContext::invalidateStorage()
{
    m_registeredContentScriptsStorage = nullptr;
    m_localStorageStore = nullptr;
    m_sessionStorageStore = nullptr;
    m_syncStorageStore = nullptr;
}

void WebExtensionContext::setBaseURL(URL&& url)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    if (!url.isValid())
        return;

    m_baseURL = URL { makeString(url.protocol(), "://"_s, url.host(), '/') };
}

bool WebExtensionContext::isURLForThisExtension(const URL& url) const
{
    return url.isValid() && protocolHostAndPortAreEqual(baseURL(), url);
}

bool WebExtensionContext::isURLForAnyExtension(const URL& url)
{
    return url.isValid() && WebExtensionMatchPattern::extensionSchemes().contains(url.protocol().toString());
}

void WebExtensionContext::setUniqueIdentifier(String&& uniqueIdentifier)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    m_customUniqueIdentifier = !uniqueIdentifier.isEmpty();

    if (uniqueIdentifier.isEmpty())
        uniqueIdentifier = WTF::UUID::createVersion4().toString();

    m_uniqueIdentifier = uniqueIdentifier;
}

RefPtr<WebExtensionLocalization> WebExtensionContext::localization()
{
    if (!m_localization)
        m_localization = WebExtensionLocalization::create(protectedExtension()->localization()->localizationJSON(), baseURL().host().toString());
    return m_localization;
}

RefPtr<API::Data> WebExtensionContext::localizedResourceData(const RefPtr<API::Data>& resourceData, const String& mimeType)
{
    if (!equalLettersIgnoringASCIICase(mimeType, "text/css"_s) || !resourceData)
        return resourceData;

    RefPtr decoder = WebCore::TextResourceDecoder::create(mimeType, { }, true);
    auto stylesheetContents = decoder->decode(resourceData->span());

    auto localizedString = localizedResourceString(stylesheetContents, mimeType);
    if (localizedString == stylesheetContents)
        return resourceData;

    return API::Data::create(localizedString.utf8().span());
}

String WebExtensionContext::localizedResourceString(const String& resourceContents, const String& mimeType)
{
    if (!equalLettersIgnoringASCIICase(mimeType, "text/css"_s) || resourceContents.isEmpty() || !resourceContents.contains("__MSG_"_s))
        return resourceContents;

    RefPtr localization = this->localization();
    if (!localization)
        return resourceContents;

    return localization->localizedStringForString(resourceContents);
}

void WebExtensionContext::setUnsupportedAPIs(HashSet<String>&& unsupported)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    m_unsupportedAPIs = WTFMove(unsupported);
}

WebExtensionContext::InjectedContentVector WebExtensionContext::injectedContents() const
{
    InjectedContentVector result = protectedExtension()->staticInjectedContents();

    for (auto& entry : m_registeredScriptsMap)
        result.append(entry.value->injectedContent());

    return result;
}

bool WebExtensionContext::hasInjectedContentForURL(const URL& url)
{
    for (auto& injectedContent : injectedContents()) {
        // FIXME: <https://webkit.org/b/246492> Add support for exclude globs.
        bool isExcluded = false;
        for (auto& excludeMatchPattern : injectedContent.excludeMatchPatterns) {
            if (excludeMatchPattern->matchesURL(url)) {
                isExcluded = true;
                break;
            }
        }

        if (isExcluded)
            continue;

        // FIXME: <https://webkit.org/b/246492> Add support for include globs.
        for (auto& includeMatchPattern : injectedContent.includeMatchPatterns) {
            if (includeMatchPattern->matchesURL(url))
                return true;
        }
    }

    return false;
}

bool WebExtensionContext::hasInjectedContent()
{
    return !injectedContents().isEmpty();
}

URL WebExtensionContext::optionsPageURL() const
{
    RefPtr extension = m_extension;
    if (!extension->hasOptionsPage())
        return { };
    return { m_baseURL, extension->optionsPagePath() };
}

URL WebExtensionContext::overrideNewTabPageURL() const
{
    RefPtr extension = m_extension;
    if (!extension->hasOverrideNewTabPage())
        return { };
    return { m_baseURL, extension->overrideNewTabPagePath() };
}

void WebExtensionContext::setHasAccessToPrivateData(bool hasAccess)
{
    if (m_hasAccessToPrivateData == hasAccess)
        return;

    m_hasAccessToPrivateData = hasAccess;

    if (!isLoaded())
        return;

    if (m_hasAccessToPrivateData) {
        addDeclarativeNetRequestRulesToPrivateUserContentControllers();

        for (Ref controller : extensionController()->allPrivateUserContentControllers())
            addInjectedContent(controller);

#if ENABLE(INSPECTOR_EXTENSIONS)
        loadInspectorBackgroundPagesForPrivateBrowsing();
#endif
    } else {
        for (Ref controller : extensionController()->allPrivateUserContentControllers()) {
            removeInjectedContent(controller);
            controller->removeContentRuleList(uniqueIdentifier());
        }

#if ENABLE(INSPECTOR_EXTENSIONS)
        unloadInspectorBackgroundPagesForPrivateBrowsing();
#endif
    }
}

bool WebExtensionContext::needsPermission(const String& permission, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!permission.isEmpty());
    ASSERT(!options.contains(PermissionStateOptions::SkipRequestedPermissions));

    switch (permissionState(permission, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return false;

    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return true;
    }
}

bool WebExtensionContext::needsPermission(const URL& url, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!options.contains(PermissionStateOptions::SkipRequestedPermissions));

    switch (permissionState(url, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return false;

    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return true;
    }
}

bool WebExtensionContext::needsPermission(const WebExtensionMatchPattern& pattern, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!options.contains(PermissionStateOptions::SkipRequestedPermissions));

    switch (permissionState(pattern, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return false;

    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return true;
    }
}

bool WebExtensionContext::hasPermission(const String& permission, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!permission.isEmpty());

    options.add(PermissionStateOptions::SkipRequestedPermissions);

    switch (permissionState(permission, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return false;

    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return true;
    }
}

bool WebExtensionContext::hasPermission(const URL& url, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    options.add(PermissionStateOptions::SkipRequestedPermissions);

    switch (permissionState(url, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return false;

    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return true;
    }
}

bool WebExtensionContext::hasPermission(const WebExtensionMatchPattern& pattern, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    options.add(PermissionStateOptions::SkipRequestedPermissions);

    switch (permissionState(pattern, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return false;

    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return true;
    }
}

bool WebExtensionContext::hasPermissions(PermissionsSet permissions, MatchPatternSet matchPatterns)
{
    for (auto& permission : permissions) {
        if (!m_grantedPermissions.contains(permission))
            return false;
    }

    for (auto& pattern : matchPatterns) {
        bool matchFound = false;
        for (auto& grantedPattern : currentPermissionMatchPatterns()) {
            if (grantedPattern->matchesPattern(pattern, { WebExtensionMatchPattern::Options::IgnorePaths })) {
                matchFound = true;
                break;
            }
        }

        if (!matchFound)
            return false;
    }

    return true;
}

WebExtensionContext::PermissionState WebExtensionContext::permissionState(const String& permission, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!permission.isEmpty());

    if (tab && permission == String(WKWebExtensionPermissionTabs)) {
        if (tab->extensionHasTemporaryPermission())
            return PermissionState::GrantedExplicitly;
    }

    if (!WebExtension::supportedPermissions().contains(permission))
        return PermissionState::Unknown;

    if (deniedPermissions().contains(permission))
        return PermissionState::DeniedExplicitly;

    if (grantedPermissions().contains(permission))
        return PermissionState::GrantedExplicitly;

    if (options.contains(PermissionStateOptions::SkipRequestedPermissions))
        return PermissionState::Unknown;

    RefPtr extension = m_extension;
    if (extension->hasRequestedPermission(permission))
        return PermissionState::RequestedExplicitly;

    if (options.contains(PermissionStateOptions::IncludeOptionalPermissions)) {
        if (extension->optionalPermissions().contains(permission))
            return PermissionState::RequestedImplicitly;
    }

    return PermissionState::Unknown;
}

WebExtensionContext::PermissionState WebExtensionContext::permissionState(const URL& url, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    if (url.isEmpty())
        return PermissionState::Unknown;

    if (isURLForThisExtension(url))
        return PermissionState::GrantedImplicitly;

    if (!WebExtensionMatchPattern::validSchemes().contains(url.protocol().toStringWithoutCopying()))
        return PermissionState::Unknown;

    if (tab) {
        auto temporaryPattern = tab->temporaryPermissionMatchPattern();
        if (temporaryPattern && temporaryPattern->matchesURL(url))
            return PermissionState::GrantedExplicitly;
    }

    bool skipRequestedPermissions = options.contains(PermissionStateOptions::SkipRequestedPermissions);

    // Access the maps here to remove any expired entries, and only do it once for this call.
    auto& grantedPermissionMatchPatterns = this->grantedPermissionMatchPatterns();
    auto& deniedPermissionMatchPatterns = this->deniedPermissionMatchPatterns();

    // If the cache still has the URL, then it has not expired.
    if (m_cachedPermissionURLs.contains(url)) {
        PermissionState cachedState = m_cachedPermissionStates.get(url);

        // We only want to return an unknown cached state if the SkippingRequestedPermissions option isn't used.
        if (cachedState != PermissionState::Unknown || skipRequestedPermissions) {
            // Move the URL to the end, so it stays in the cache longer as a recent hit.
            m_cachedPermissionURLs.appendOrMoveToLast(url);

            if ((cachedState == PermissionState::RequestedExplicitly || cachedState == PermissionState::RequestedImplicitly) && skipRequestedPermissions)
                return PermissionState::Unknown;

            return cachedState;
        }
    }

    auto cacheResultAndReturn = ^PermissionState(PermissionState result) {
        m_cachedPermissionURLs.appendOrMoveToLast(url);
        m_cachedPermissionStates.set(url, result);

        ASSERT(m_cachedPermissionURLs.size() == m_cachedPermissionURLs.size());

        if (m_cachedPermissionURLs.size() <= maximumCachedPermissionResults)
            return result;

        URL firstCachedURL = m_cachedPermissionURLs.takeFirst();
        m_cachedPermissionStates.remove(firstCachedURL);

        ASSERT(m_cachedPermissionURLs.size() == m_cachedPermissionURLs.size());

        return result;
    };

    // First, check for patterns that are specific to certain domains, ignoring wildcard host patterns that
    // match all hosts. The order is denied, then granted. This makes sure denied takes precedence over granted.

    auto urlMatchesPatternIgnoringWildcardHostPatterns = ^(WebExtensionMatchPattern& pattern) {
        if (pattern.matchesAllHosts())
            return false;
        return pattern.matchesURL(url);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(deniedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::DeniedExplicitly);
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(grantedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::GrantedExplicitly);
    }

    // Next, check for patterns that are wildcard host patterns that match all hosts (<all_urls>, *://*/*, etc),
    // also checked in denied, then granted order. Doing these wildcard patterns separately allows for blanket
    // patterns to be set as default policies while allowing for specific domains to still be granted or denied.

    auto urlMatchesWildcardHostPatterns = ^(WebExtensionMatchPattern& pattern) {
        if (!pattern.matchesAllHosts())
            return false;
        return pattern.matchesURL(url);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(deniedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::DeniedImplicitly);
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(grantedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::GrantedImplicitly);
    }

    // Finally, check for requested patterns, allowing any pattern that matches. This is the default state
    // of the extension before any patterns are granted or denied, so it should always be last.

    if (skipRequestedPermissions)
        return cacheResultAndReturn(PermissionState::Unknown);

    auto requestedMatchPatterns = protectedExtension()->allRequestedMatchPatterns();
    for (auto& requestedMatchPattern : requestedMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(requestedMatchPattern))
            return cacheResultAndReturn(PermissionState::RequestedExplicitly);

        if (urlMatchesWildcardHostPatterns(requestedMatchPattern))
            return cacheResultAndReturn(PermissionState::RequestedImplicitly);
    }

    if (hasPermission(WebExtensionPermission::webNavigation(), tab, options))
        return cacheResultAndReturn(PermissionState::RequestedImplicitly);

    if (hasPermission(WebExtensionPermission::declarativeNetRequestFeedback(), tab, options))
        return cacheResultAndReturn(PermissionState::RequestedImplicitly);

    if (options.contains(PermissionStateOptions::RequestedWithTabsPermission) && hasPermission(WKWebExtensionPermissionTabs, tab, options))
        return PermissionState::RequestedImplicitly;

    if (options.contains(PermissionStateOptions::IncludeOptionalPermissions)) {
        if (WebExtensionMatchPattern::patternsMatchURL(protectedExtension()->optionalPermissionMatchPatterns(), url))
            return cacheResultAndReturn(PermissionState::RequestedImplicitly);
    }

    return cacheResultAndReturn(PermissionState::Unknown);
}

WebExtensionContext::PermissionState WebExtensionContext::permissionState(const WebExtensionMatchPattern& pattern, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    if (!pattern.isValid())
        return PermissionState::Unknown;

    if (pattern.matchesURL(baseURL()))
        return PermissionState::GrantedImplicitly;

    if (!pattern.matchesAllURLs() && !WebExtensionMatchPattern::validSchemes().contains(pattern.scheme()))
        return PermissionState::Unknown;

    if (tab) {
        auto temporaryPattern = tab->temporaryPermissionMatchPattern();
        if (temporaryPattern && temporaryPattern->matchesPattern(pattern))
            return PermissionState::GrantedExplicitly;
    }

    // Access the maps here to remove any expired entries, and only do it once for this call.
    auto& grantedPermissionMatchPatterns = this->grantedPermissionMatchPatterns();
    auto& deniedPermissionMatchPatterns = this->deniedPermissionMatchPatterns();

    // First, check for patterns that are specific to certain domains, ignoring wildcard host patterns that
    // match all hosts. The order is denied, then granted. This makes sure denied takes precedence over granted.

    auto urlMatchesPatternIgnoringWildcardHostPatterns = ^(WebExtensionMatchPattern& otherPattern) {
        if (pattern.matchesAllHosts())
            return false;
        return pattern.matchesPattern(otherPattern);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(deniedPermissionEntry.key))
            return PermissionState::DeniedExplicitly;
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(grantedPermissionEntry.key))
            return PermissionState::GrantedExplicitly;
    }

    // Next, check for patterns that are wildcard host patterns that match all hosts (<all_urls>, *://*/*, etc),
    // also checked in denied, then granted order. Doing these wildcard patterns separately allows for blanket
    // patterns to be set as default policies while allowing for specific domains to still be granted or denied.

    auto urlMatchesWildcardHostPatterns = ^(WebExtensionMatchPattern& otherPattern) {
        if (!pattern.matchesAllHosts())
            return false;
        return pattern.matchesPattern(otherPattern);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(deniedPermissionEntry.key))
            return PermissionState::DeniedImplicitly;
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(grantedPermissionEntry.key))
            return PermissionState::GrantedImplicitly;
    }

    // Finally, check for requested patterns, allowing any pattern that matches. This is the default state
    // of the extension before any patterns are granted or denied, so it should always be last.

    if (options.contains(PermissionStateOptions::SkipRequestedPermissions))
        return PermissionState::Unknown;

    auto requestedMatchPatterns = protectedExtension()->allRequestedMatchPatterns();
    for (auto& requestedMatchPattern : requestedMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(requestedMatchPattern))
            return PermissionState::RequestedExplicitly;

        if (urlMatchesWildcardHostPatterns(requestedMatchPattern))
            return PermissionState::RequestedImplicitly;
    }

    if (options.contains(PermissionStateOptions::RequestedWithTabsPermission) && hasPermission(WKWebExtensionPermissionTabs, tab, options))
        return PermissionState::RequestedImplicitly;

    if (options.contains(PermissionStateOptions::IncludeOptionalPermissions)) {
        if (WebExtensionMatchPattern::patternsMatchPattern(protectedExtension()->optionalPermissionMatchPatterns(), pattern))
            return PermissionState::RequestedImplicitly;
    }

    return PermissionState::Unknown;
}

void WebExtensionContext::setPermissionState(PermissionState state, const String& permission, WallTime expirationDate)
{
    ASSERT(!permission.isEmpty());
    ASSERT(!expirationDate.isNaN());

    auto permissions = PermissionsSet { permission };

    switch (state) {
    case PermissionState::DeniedExplicitly:
        denyPermissions(WTFMove(permissions), expirationDate);
        break;

    case PermissionState::Unknown: {
        removeGrantedPermissions(permissions);
        removeDeniedPermissions(permissions);
        break;
    }

    case PermissionState::GrantedExplicitly:
        grantPermissions(WTFMove(permissions), expirationDate);
        break;

    case PermissionState::DeniedImplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
    case PermissionState::GrantedImplicitly:
        ASSERT_NOT_REACHED();
        break;
    }
}

void WebExtensionContext::setPermissionState(PermissionState state, const URL& url, WallTime expirationDate)
{
    ASSERT(!url.isEmpty());
    ASSERT(!expirationDate.isNaN());

    RefPtr pattern = WebExtensionMatchPattern::getOrCreate(url);
    if (!pattern)
        return;

    setPermissionState(state, *pattern, expirationDate);
}

void WebExtensionContext::setPermissionState(PermissionState state, const WebExtensionMatchPattern& pattern, WallTime expirationDate)
{
    ASSERT(pattern.isValid());
    ASSERT(!expirationDate.isNaN());

    auto patterns = MatchPatternSet { const_cast<WebExtensionMatchPattern&>(pattern) };
    auto equalityOnly = pattern.matchesAllHosts() ? EqualityOnly::Yes : EqualityOnly::No;

    switch (state) {
    case PermissionState::DeniedExplicitly:
        denyPermissionMatchPatterns(WTFMove(patterns), expirationDate, equalityOnly);
        break;

    case PermissionState::Unknown: {
        removeGrantedPermissionMatchPatterns(patterns, equalityOnly);
        removeDeniedPermissionMatchPatterns(patterns, equalityOnly);
        break;
    }

    case PermissionState::GrantedExplicitly:
        grantPermissionMatchPatterns(WTFMove(patterns), expirationDate, equalityOnly);
        break;

    case PermissionState::DeniedImplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
    case PermissionState::GrantedImplicitly:
        ASSERT_NOT_REACHED();
        break;
    }
}

void WebExtensionContext::clearCachedPermissionStates()
{
    m_cachedPermissionStates.clear();
    m_cachedPermissionURLs.clear();
}

bool WebExtensionContext::hasAccessToAllURLs()
{
    for (auto& pattern : currentPermissionMatchPatterns()) {
        if (pattern->matchesAllURLs())
            return true;
    }

    return false;
}

bool WebExtensionContext::hasAccessToAllHosts()
{
    for (auto& pattern : currentPermissionMatchPatterns()) {
        if (pattern->matchesAllHosts())
            return true;
    }

    return false;
}

void WebExtensionContext::removePage(WebPageProxy& page)
{
    disconnectPortsForPage(page);
}

bool WebExtensionContext::isValidWindow(const WebExtensionWindow& window)
{
    return window.isValid() && window.extensionContext() == this && m_windowMap.get(window.identifier()) == &window;
}

bool WebExtensionContext::isValidTab(const WebExtensionTab& tab)
{
    return tab.isValid() && tab.extensionContext() == this && m_tabMap.get(tab.identifier()) == &tab;
}

WebExtensionContext::WindowVector WebExtensionContext::openWindows(IgnoreExtensionAccess ignoreExtensionAccess) const
{
    return WTF::compactMap(m_windowOrderVector, [&](auto& identifier) -> std::optional<Ref<WebExtensionWindow>> {
        RefPtr window = m_windowMap.get(identifier);
        ASSERT(window && window->isOpen());

        if (ignoreExtensionAccess == IgnoreExtensionAccess::No && !window->extensionHasAccess())
            return std::nullopt;
        return *window;
    });
}

WebExtensionContext::TabVector WebExtensionContext::openTabs(IgnoreExtensionAccess ignoreExtensionAccess) const
{
    return WTF::compactMap(m_tabMap, [&](auto& entry) -> std::optional<Ref<WebExtensionTab>> {
        if (!entry.value->isOpen())
            return std::nullopt;
        if (ignoreExtensionAccess == IgnoreExtensionAccess::No && !entry.value->extensionHasAccess())
            return std::nullopt;
        return entry.value;
    });
}

RefPtr<WebExtensionWindow> WebExtensionContext::focusedWindow(IgnoreExtensionAccess ignoreExtensionAccess) const
{
    if (m_focusedWindowIdentifier)
        return getWindow(m_focusedWindowIdentifier.value(), std::nullopt, ignoreExtensionAccess);
    return nullptr;
}

RefPtr<WebExtensionWindow> WebExtensionContext::frontmostWindow(IgnoreExtensionAccess ignoreExtensionAccess) const
{
    // Return the first non-null window, skipping private windows if access is denied.
    for (auto& windowIdentifier : m_windowOrderVector) {
        if (RefPtr window = getWindow(windowIdentifier, std::nullopt, ignoreExtensionAccess))
            return window;
    }

    return nullptr;
}

WebExtensionContextParameters WebExtensionContext::parameters() const
{
    RefPtr extension = m_extension;

    return {
        identifier(),
        baseURL(),
        uniqueIdentifier(),
        unsupportedAPIs(),
        m_grantedPermissions,
        extension->serializeLocalization(),
        extension->serializeManifest(),
        extension->manifestVersion(),
        isSessionStorageAllowedInContentScripts(),
        backgroundPageIdentifier(),
#if ENABLE(INSPECTOR_EXTENSIONS)
        inspectorPageIdentifiers(),
        inspectorBackgroundPageIdentifiers(),
#endif
        popupPageIdentifiers(),
        tabPageIdentifiers()
    };
}

bool WebExtensionContext::inTestingMode() const
{
    return m_extensionController && m_extensionController->inTestingMode();
}

const WebExtensionContext::UserContentControllerProxySet& WebExtensionContext::userContentControllers() const
{
    ASSERT(isLoaded());

    if (hasAccessToPrivateData())
        return extensionController()->allUserContentControllers();
    return extensionController()->allNonPrivateUserContentControllers();
}

WebExtensionContext::WebProcessProxySet WebExtensionContext::processes(EventListenerTypeSet&& typeSet, ContentWorldTypeSet&& contentWorldTypeSet, Function<bool(WebPageProxy&, WebFrameProxy&)>&& predicate) const
{
    if (!isLoaded())
        return { };

#if ENABLE(INSPECTOR_EXTENSIONS)
    // Inspector content world is a special alias of Main. Include it when Main is requested (and vice versa).
    if (contentWorldTypeSet.contains(WebExtensionContentWorldType::Main))
        contentWorldTypeSet.add(WebExtensionContentWorldType::Inspector);
    else if (contentWorldTypeSet.contains(WebExtensionContentWorldType::Inspector))
        contentWorldTypeSet.add(WebExtensionContentWorldType::Main);
#endif

    WebProcessProxySet result;

    for (auto type : typeSet) {
        for (auto contentWorldType : contentWorldTypeSet) {
            auto pagesEntry = m_eventListenerFrames.find({ type, contentWorldType });
            if (pagesEntry == m_eventListenerFrames.end())
                continue;

            for (auto entry : pagesEntry->value) {
                Ref frame = entry.key;
                RefPtr page = frame->page();
                if (!page)
                    continue;

                if (!hasAccessToPrivateData() && page->sessionID().isEphemeral())
                    continue;

                if (predicate && !predicate(*page, frame))
                    continue;

                Ref webProcess = frame->process();
                if (webProcess->canSendMessage())
                    result.add(webProcess);
            }
        }
    }

    return result;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
