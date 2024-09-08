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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtension.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WKNSData.h"
#import "WKNSError.h"
#import "APIData.h"
#import "CocoaHelpers.h"
#import "FoundationSPI.h"
#import "Logging.h"
#import "WKWebExtensionInternal.h"
#import "WKWebExtensionPermissionPrivate.h"
#import "WebExtensionConstants.h"
#import "WebExtensionUtilities.h"
#import "_WKWebExtensionLocalization.h"
#import <CoreFoundation/CFBundle.h>
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <UniformTypeIdentifiers/UTType.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#import <pal/spi/mac/NSImageSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <UIKit/UIKit.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(CoreSVG)
SOFT_LINK(CoreSVG, CGSVGDocumentCreateFromData, CGSVGDocumentRef, (CFDataRef data, CFDictionaryRef options), (data, options))
SOFT_LINK(CoreSVG, CGSVGDocumentRelease, void, (CGSVGDocumentRef document), (document))
#endif

namespace WebKit {

static NSString * const manifestVersionManifestKey = @"manifest_version";

static NSString * const nameManifestKey = @"name";
static NSString * const shortNameManifestKey = @"short_name";
static NSString * const versionManifestKey = @"version";
static NSString * const versionNameManifestKey = @"version_name";
static NSString * const descriptionManifestKey = @"description";

static NSString * const iconsManifestKey = @"icons";

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
static NSString * const iconVariantsManifestKey = @"icon_variants";
static NSString * const colorSchemesManifestKey = @"color_schemes";
static NSString * const lightManifestKey = @"light";
static NSString * const darkManifestKey = @"dark";
static NSString * const anyManifestKey = @"any";
#endif

static NSString * const actionManifestKey = @"action";
static NSString * const browserActionManifestKey = @"browser_action";
static NSString * const pageActionManifestKey = @"page_action";

static NSString * const defaultIconManifestKey = @"default_icon";
static NSString * const defaultLocaleManifestKey = @"default_locale";
static NSString * const defaultTitleManifestKey = @"default_title";
static NSString * const defaultPopupManifestKey = @"default_popup";

static NSString * const backgroundManifestKey = @"background";
static NSString * const backgroundPageManifestKey = @"page";
static NSString * const backgroundServiceWorkerManifestKey = @"service_worker";
static NSString * const backgroundScriptsManifestKey = @"scripts";
static NSString * const backgroundPersistentManifestKey = @"persistent";
static NSString * const backgroundPageTypeKey = @"type";
static NSString * const backgroundPageTypeModuleValue = @"module";
static NSString * const backgroundPreferredEnvironmentManifestKey = @"preferred_environment";
static NSString * const backgroundDocumentManifestKey = @"document";

static NSString * const generatedBackgroundPageFilename = @"_generated_background_page.html";
static NSString * const generatedBackgroundServiceWorkerFilename = @"_generated_service_worker.js";

static NSString * const devtoolsPageManifestKey = @"devtools_page";

static NSString * const contentScriptsManifestKey = @"content_scripts";
static NSString * const contentScriptsMatchesManifestKey = @"matches";
static NSString * const contentScriptsExcludeMatchesManifestKey = @"exclude_matches";
static NSString * const contentScriptsIncludeGlobsManifestKey = @"include_globs";
static NSString * const contentScriptsExcludeGlobsManifestKey = @"exclude_globs";
static NSString * const contentScriptsMatchesAboutBlankManifestKey = @"match_about_blank";
static NSString * const contentScriptsRunAtManifestKey = @"run_at";
static NSString * const contentScriptsDocumentIdleManifestKey = @"document_idle";
static NSString * const contentScriptsDocumentStartManifestKey = @"document_start";
static NSString * const contentScriptsDocumentEndManifestKey = @"document_end";
static NSString * const contentScriptsAllFramesManifestKey = @"all_frames";
static NSString * const contentScriptsJSManifestKey = @"js";
static NSString * const contentScriptsCSSManifestKey = @"css";
static NSString * const contentScriptsWorldManifestKey = @"world";
static NSString * const contentScriptsIsolatedManifestKey = @"ISOLATED";
static NSString * const contentScriptsMainManifestKey = @"MAIN";
static NSString * const contentScriptsCSSOriginManifestKey = @"css_origin";
static NSString * const contentScriptsAuthorManifestKey = @"author";
static NSString * const contentScriptsUserManifestKey = @"user";

static NSString * const permissionsManifestKey = @"permissions";
static NSString * const optionalPermissionsManifestKey = @"optional_permissions";
static NSString * const hostPermissionsManifestKey = @"host_permissions";
static NSString * const optionalHostPermissionsManifestKey = @"optional_host_permissions";

static NSString * const optionsUIManifestKey = @"options_ui";
static NSString * const optionsUIPageManifestKey = @"page";
static NSString * const optionsPageManifestKey = @"options_page";
static NSString * const chromeURLOverridesManifestKey = @"chrome_url_overrides";
static NSString * const browserURLOverridesManifestKey = @"browser_url_overrides";
static NSString * const newTabManifestKey = @"newtab";

static NSString * const contentSecurityPolicyManifestKey = @"content_security_policy";
static NSString * const contentSecurityPolicyExtensionPagesManifestKey = @"extension_pages";

static NSString * const commandsManifestKey = @"commands";
static NSString * const commandsSuggestedKeyManifestKey = @"suggested_key";
static NSString * const commandsDescriptionKeyManifestKey = @"description";

static NSString * const webAccessibleResourcesManifestKey = @"web_accessible_resources";
static NSString * const webAccessibleResourcesResourcesManifestKey = @"resources";
static NSString * const webAccessibleResourcesMatchesManifestKey = @"matches";

static NSString * const declarativeNetRequestManifestKey = @"declarative_net_request";
static NSString * const declarativeNetRequestRulesManifestKey = @"rule_resources";
static NSString * const declarativeNetRequestRulesetIDManifestKey = @"id";
static NSString * const declarativeNetRequestRuleEnabledManifestKey = @"enabled";
static NSString * const declarativeNetRequestRulePathManifestKey = @"path";

static NSString * const externallyConnectableManifestKey = @"externally_connectable";
static NSString * const externallyConnectableMatchesManifestKey = @"matches";
static NSString * const externallyConnectableIDsManifestKey = @"ids";

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
static NSString * const sidebarActionManifestKey = @"sidebar_action";
static NSString * const sidePanelManifestKey = @"side_panel";
static NSString * const sidebarActionTitleManifestKey = @"default_title";
static NSString * const sidebarActionPathManifestKey = @"default_panel";
static NSString * const sidePanelPathManifestKey = @"default_path";
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

WebExtension::WebExtension(NSBundle *appExtensionBundle)
    : m_bundle(appExtensionBundle)
    , m_resourceBaseURL(appExtensionBundle.resourceURL.URLByStandardizingPath.absoluteURL)
    , m_manifest(JSON::Object::create())
{
    ASSERT(appExtensionBundle);

    String manifestData = resourceStringForPath("manifest.json"_s);
    if (manifestData.isEmpty()) {
        createError(Error::InvalidManifest); // FIXME: Should this be a separate constructor function as well?
        return;
    }
    
    auto manifestJSON = JSON::Value::parseJSON(manifestData);
    if (!manifestJSON) {
        createError(Error::InvalidManifest); // FIXME: Should this be a separate constructor function as well?
        return;
    }

    m_manifest = manifestJSON.releaseNonNull();
}

Ref<API::Data> WebExtension::serializeLocalization()
{
    return API::Data::createWithoutCopying(encodeJSONData(m_localization.get().localizationDictionary));
}

SecStaticCodeRef WebExtension::bundleStaticCode() const
{
    if (!m_bundle)
        return nullptr;

    if (m_bundleStaticCode)
        return m_bundleStaticCode.get();

    SecStaticCodeRef staticCodeRef;
    OSStatus error = SecStaticCodeCreateWithPath(bridge_cast(m_bundle.get().bundleURL), kSecCSDefaultFlags, &staticCodeRef);
    if (error != noErr || !staticCodeRef) {
        if (staticCodeRef)
            CFRelease(staticCodeRef);
        return nullptr;
    }

    m_bundleStaticCode = adoptCF(staticCodeRef);

    return m_bundleStaticCode.get();
}

NSData *WebExtension::bundleHash() const
{
    auto staticCode = bundleStaticCode();
    if (!staticCode)
        return nil;

    CFDictionaryRef codeSigningDictionary = nil;
    OSStatus error = SecCodeCopySigningInformation(staticCode, kSecCSDefaultFlags, &codeSigningDictionary);
    if (error != noErr || !codeSigningDictionary) {
        if (codeSigningDictionary)
            CFRelease(codeSigningDictionary);
        return nil;
    }

    auto *result = bridge_cast(checked_cf_cast<CFDataRef>(CFDictionaryGetValue(codeSigningDictionary, kSecCodeInfoUnique)));
    CFRelease(codeSigningDictionary);

    return result;
}

#if PLATFORM(MAC)
bool WebExtension::validateResourceData(NSURL *resourceURL, NSData *resourceData, NSError **outError)
{
    ASSERT([resourceURL isFileURL]);
    ASSERT(resourceData);

    if (!m_bundle)
        return true;

    auto staticCode = bundleStaticCode();
    if (!staticCode)
        return false;

    NSURL *bundleSupportFilesURL = CFBridgingRelease(CFBundleCopySupportFilesDirectoryURL(m_bundle.get()._cfBundle));
    NSString *bundleSupportFilesURLString = bundleSupportFilesURL.absoluteString;
    NSString *resourceURLString = resourceURL.absoluteString;
    ASSERT([resourceURLString hasPrefix:bundleSupportFilesURLString]);

    NSString *relativePathToResource = [resourceURLString substringFromIndex:bundleSupportFilesURLString.length].stringByRemovingPercentEncoding;
    OSStatus result = SecCodeValidateFileResource(staticCode, bridge_cast(relativePathToResource), bridge_cast(resourceData), kSecCSDefaultFlags);

    if (outError && result != noErr)
        *outError = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];

    return result == noErr;
}
#endif // PLATFORM(MAC)

UTType *WebExtension::resourceTypeForPath(StringView path)
{
    UTType *result;

    NSString *ns_path = path.createNSStringWithoutCopying().get();

    if ([ns_path hasPrefix:@"data:"]) {
        auto mimeTypeRange = [ns_path rangeOfString:@";"];
        if (mimeTypeRange.location != NSNotFound) {
            auto *mimeType = [ns_path substringWithRange:NSMakeRange(5, mimeTypeRange.location - 5)];
            result = [UTType typeWithMIMEType:mimeType];
        }
    } else if (auto *fileExtension = ns_path.pathExtension; fileExtension.length)
        result = [UTType typeWithFilenameExtension:fileExtension];
    else if (auto *fileURL = (__bridge NSURL *)resourceFileURLForPath(path)->createCFURL().get())
        [fileURL getResourceValue:&result forKey:NSURLContentTypeKey error:nil];

    return result;
}

std::span<const uint8_t> WebExtension::resourceDataForPath(String path, RefPtr<API::Error> *outError, CacheResult cacheResult, SuppressNotFoundErrors suppressErrors)
{
    ASSERT(path);

    if (outError)
        *outError = nullptr;

    NSString *ns_path = (__bridge NSString *)path.createCFString().get();

    if ([ns_path hasPrefix:@"data:"]) {
        if (auto base64Range = [ns_path rangeOfString:@";base64,"]; base64Range.location != NSNotFound) {
            auto *base64String = [ns_path substringFromIndex:NSMaxRange(base64Range)];
            auto data = retainPtr([[NSData alloc] initWithBase64EncodedString:base64String options:0]);
            return API::Data::createWithoutCopying(data)->span();
        }

        if (auto commaRange = [ns_path rangeOfString:@","]; commaRange.location != NSNotFound) {
            auto *urlEncodedString = [ns_path substringFromIndex:NSMaxRange(commaRange)];
            auto *decodedString = [urlEncodedString stringByRemovingPercentEncoding];
            auto data = retainPtr([decodedString dataUsingEncoding:NSUTF8StringEncoding]);
            return API::Data::createWithoutCopying(data)->span();
        }

        ASSERT([ns_path isEqualToString:@"data:"]);
        return std::span<const uint8_t>();
    }

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if ([ns_path hasPrefix:@"/"])
        ns_path = [ns_path substringFromIndex:1];

    if (auto cachedObject = m_resources.getOptional(path)) {
        if (auto cachedString = String::fromUTF8(*cachedObject); !cachedString.isEmpty())
            return cachedString.utf8().span();
        
        ASSERT(!(*cachedObject).empty());

        return *cachedObject;

        // FIXME: We may need to convert the raw data to JSON and cache it
    }

    bool isServiceWorker = backgroundContentIsServiceWorker();
    if (!isServiceWorker && [ns_path isEqualToString:generatedBackgroundPageFilename])
        return generatedBackgroundContent().utf8().span();

    if (isServiceWorker && [ns_path isEqualToString:generatedBackgroundServiceWorkerFilename])
        return generatedBackgroundContent().utf8().span();

    NSURL *resourceURL;
    if (auto url = resourceFileURLForPath(path)) {
        resourceURL = (__bridge NSURL *)(*url).createCFURL().get();
    } else {
        if (suppressErrors == SuppressNotFoundErrors::No && outError)
            *outError = createError(Error::ResourceNotFound, WEB_UI_FORMAT_CFSTRING("Unable to find \"%@\" in the extension’s resources. It is an invalid path.", "WKWebExtensionErrorResourceNotFound description with invalid file path", (__bridge CFStringRef)ns_path));
        return std::span<const uint8_t>();
    }

    NSError *fileReadError;
    NSData *resultData = [NSData dataWithContentsOfURL:resourceURL options:NSDataReadingMappedIfSafe error:&fileReadError];
    if (!resultData) {
        if (suppressErrors == SuppressNotFoundErrors::No && outError) {
            auto error = API::Error::create(*(new WebCore::ResourceError(fileReadError)));
            *outError = createError(Error::ResourceNotFound, WEB_UI_FORMAT_CFSTRING("Unable to find \"%@\" in the extension’s resources.", "WKWebExtensionErrorResourceNotFound description with file name", (__bridge CFStringRef)ns_path), &error.leakRef());
        }
        return std::span<const uint8_t>();
    }

#if PLATFORM(MAC)
    NSError *validationError;
    if (!validateResourceData(resourceURL, resultData, &validationError)) {
        if (outError) {
            auto error = API::Error::create(*(new WebCore::ResourceError(validationError)));
            *outError = createError(Error::InvalidResourceCodeSignature, WEB_UI_FORMAT_CFSTRING("Unable to validate \"%@\" with the extension’s code signature. It likely has been modified since the extension was built.", "WKWebExtensionErrorInvalidResourceCodeSignature description with file name", (__bridge CFStringRef)ns_path), &error.leakRef());
        }
        return std::span<const uint8_t>();
    }
#endif

    auto ns_data = retainPtr(resultData);
    auto data = API::Data::createWithoutCopying(ns_data)->span();

    if (cacheResult == CacheResult::Yes) {
        m_resources.set(path, data);
    }

    return data;
}

// static WKWebExtensionError toAPI(WebExtension::Error error)
// {
//     switch (error) {
//     case WebExtension::Error::Unknown:
//         return WKWebExtensionErrorUnknown;
//     case WebExtension::Error::ResourceNotFound:
//         return WKWebExtensionErrorResourceNotFound;
//     case WebExtension::Error::InvalidManifest:
//         return WKWebExtensionErrorInvalidManifest;
//     case WebExtension::Error::UnsupportedManifestVersion:
//         return WKWebExtensionErrorUnsupportedManifestVersion;
//     case WebExtension::Error::InvalidAction:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidActionIcon:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidBackgroundContent:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidCommands:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidContentScripts:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidContentSecurityPolicy:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidDeclarativeNetRequest:
//         return WKWebExtensionErrorInvalidDeclarativeNetRequestEntry;
//     case WebExtension::Error::InvalidDescription:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidExternallyConnectable:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidIcon:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidName:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidOptionsPage:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidURLOverrides:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidVersion:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidWebAccessibleResources:
//         return WKWebExtensionErrorInvalidManifestEntry;
//     case WebExtension::Error::InvalidBackgroundPersistence:
//         return WKWebExtensionErrorInvalidBackgroundPersistence;
//     case WebExtension::Error::InvalidResourceCodeSignature:
//         return WKWebExtensionErrorInvalidResourceCodeSignature;
//     }
// }

// NSError *WebExtension::createError(Error error, NSString *customLocalizedDescription, NSError *underlyingError)
// {
//     auto errorCode = toAPI(error);
//     NSString *localizedDescription;

//     switch (error) {
//     case Error::Unknown:
//         localizedDescription = WEB_UI_STRING("An unknown error has occurred.", "WKWebExtensionErrorUnknown description");
//         break;

//     case Error::ResourceNotFound:
//         ASSERT(customLocalizedDescription);
//         break;

//     case Error::InvalidManifest:
// ALLOW_NONLITERAL_FORMAT_BEGIN
//         if (NSString *debugDescription = underlyingError.userInfo[NSDebugDescriptionErrorKey])
//             localizedDescription = [NSString stringWithFormat:WEB_UI_STRING("Unable to parse manifest: %@", "WKWebExtensionErrorInvalidManifest description, because of a JSON error"), debugDescription];
//         else
//             localizedDescription = WEB_UI_STRING("Unable to parse manifest because of an unexpected format.", "WKWebExtensionErrorInvalidManifest description");
// ALLOW_NONLITERAL_FORMAT_END
//         break;

//     case Error::UnsupportedManifestVersion:
//         localizedDescription = WEB_UI_STRING("An unsupported `manifest_version` was specified.", "WKWebExtensionErrorUnsupportedManifestVersion description");
//         break;

//     case Error::InvalidAction:
//         if (supportsManifestVersion(3))
//             localizedDescription = WEB_UI_STRING("Missing or empty `action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for action only");
//         else
//             localizedDescription = WEB_UI_STRING("Missing or empty `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for browser_action or page_action");
//         break;

//     case Error::InvalidActionIcon:
// #if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
//         if (m_actionDictionary.get()[iconVariantsManifestKey]) {
//             if (supportsManifestVersion(3))
//                 localizedDescription = WEB_UI_STRING("Empty or invalid `icon_variants` for the `action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icon_variants in action only");
//             else
//                 localizedDescription = WEB_UI_STRING("Empty or invalid `icon_variants` for the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icon_variants in browser_action or page_action");
//         } else
// #endif
//         if (supportsManifestVersion(3))
//             localizedDescription = WEB_UI_STRING("Empty or invalid `default_icon` for the `action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for default_icon in action only");
//         else
//             localizedDescription = WEB_UI_STRING("Empty or invalid `default_icon` for the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for default_icon in browser_action or page_action");
//         break;

//     case Error::InvalidBackgroundContent:
//         localizedDescription = WEB_UI_STRING("Empty or invalid `background` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for background");
//         break;

//     case Error::InvalidCommands:
//         localizedDescription = WEB_UI_STRING("Invalid `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for commands");
//         break;

//     case Error::InvalidContentScripts:
//         localizedDescription = WEB_UI_STRING("Empty or invalid `content_scripts` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for content_scripts");
//         break;

//     case Error::InvalidContentSecurityPolicy:
//         localizedDescription = WEB_UI_STRING("Empty or invalid `content_security_policy` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for content_security_policy");
//         break;

//     case Error::InvalidDeclarativeNetRequest:
// ALLOW_NONLITERAL_FORMAT_BEGIN
//         if (NSString *debugDescription = underlyingError.userInfo[NSDebugDescriptionErrorKey])
//             localizedDescription = [NSString stringWithFormat:WEB_UI_STRING("Unable to parse `declarativeNetRequest` rules: %@", "WKWebExtensionErrorInvalidDeclarativeNetRequest description, because of a JSON error"), debugDescription];
//         else
//             localizedDescription = WEB_UI_STRING("Unable to parse `declarativeNetRequest` rules because of an unexpected error.", "WKWebExtensionErrorInvalidDeclarativeNetRequest description");
// ALLOW_NONLITERAL_FORMAT_END
//         break;

//     case Error::InvalidDescription:
//         localizedDescription = WEB_UI_STRING("Missing or empty `description` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for description");
//         break;

//     case Error::InvalidExternallyConnectable:
//         localizedDescription = WEB_UI_STRING("Empty or invalid `externally_connectable` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for externally_connectable");
//         break;

//     case Error::InvalidIcon:
// #if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
//         if ([manifest() objectForKey:iconVariantsManifestKey])
//             localizedDescription = WEB_UI_STRING("Empty or invalid `icon_variants` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icon_variants");
//         else
// #endif
//             localizedDescription = WEB_UI_STRING("Missing or empty `icons` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icons");
//         break;

//     case Error::InvalidName:
//         localizedDescription = WEB_UI_STRING("Missing or empty `name` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for name");
//         break;

//     case Error::InvalidOptionsPage:
//         if ([manifest() objectForKey:optionsUIManifestKey])
//             localizedDescription = WEB_UI_STRING("Empty or invalid `options_ui` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for options UI");
//         else
//             localizedDescription = WEB_UI_STRING("Empty or invalid `options_page` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for options page");
//         break;

//     case Error::InvalidURLOverrides:
//         if ([manifest() objectForKey:browserURLOverridesManifestKey])
//             localizedDescription = WEB_UI_STRING("Empty or invalid `browser_url_overrides` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for browser URL overrides");
//         else
//             localizedDescription = WEB_UI_STRING("Empty or invalid `chrome_url_overrides` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for chrome URL overrides");
//         break;

//     case Error::InvalidVersion:
//         localizedDescription = WEB_UI_STRING("Missing or empty `version` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for version");
//         break;

//     case Error::InvalidWebAccessibleResources:
//         localizedDescription = WEB_UI_STRING("Invalid `web_accessible_resources` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for web_accessible_resources");
//         break;

//     case Error::InvalidBackgroundPersistence:
//         localizedDescription = WEB_UI_STRING("Invalid `persistent` manifest entry.", "WKWebExtensionErrorInvalidBackgroundPersistence description");
//         break;

//     case Error::InvalidResourceCodeSignature:
//         ASSERT(customLocalizedDescription);
//         break;
//     }

//     if (customLocalizedDescription.length)
//         localizedDescription = customLocalizedDescription;

//     NSDictionary *userInfo;
//     if (underlyingError)
//         userInfo = @{ NSLocalizedDescriptionKey: localizedDescription, NSUnderlyingErrorKey: underlyingError };
//     else
//         userInfo = @{ NSLocalizedDescriptionKey: localizedDescription };

//     return [[NSError alloc] initWithDomain:WKWebExtensionErrorDomain code:errorCode userInfo:userInfo];
// }

void WebExtension::recordError(Ref<API::Error> error)
{
    if (!m_errors.isEmpty())
        m_errors = Vector<Ref<API::Error>>();

    RELEASE_LOG_ERROR(Extensions, "Error recorded: %{public}@", privacyPreservingDescription(::WebKit::wrapper(error)));

    // Only the first occurrence of each error is recorded in the array. This prevents duplicate errors,
    // such as repeated "resource not found" errors, from being included multiple times.
    if (m_errors.contains(error))
        return;

    [wrapper() willChangeValueForKey:@"errors"];
    m_errors.append(error);
    [wrapper() didChangeValueForKey:@"errors"];
}

_WKWebExtensionLocalization *WebExtension::localization()
{
    if (!manifestParsedSuccessfully())
        return nil;

    return m_localization.get();
}

NSLocale *WebExtension::defaultLocale()
{
    if (!manifestParsedSuccessfully())
        return nil;

    return m_defaultLocale.get();
}

RefPtr<WebCore::Icon> WebExtension::imageForPath(String imagePath, RefPtr<API::Error> *outError, WebCore::FloatSize sizeForResizing)
{
    ASSERT(imagePath);

    NSData *imageData = ::WebKit::wrapper(API::Data::create(resourceDataForPath(imagePath, outError, CacheResult::Yes))).get();
    if (!imageData)
        return nil;

    CocoaImage *result;

#if !USE(NSIMAGE_FOR_SVG_SUPPORT)
    UTType *imageType = resourceTypeForPath(imagePath);
    if ([imageType.identifier isEqualToString:UTTypeSVG.identifier]) {
#if USE(APPKIT)
        static Class svgImageRep = NSClassFromString(@"_NSSVGImageRep");
        RELEASE_ASSERT(svgImageRep);

        _NSSVGImageRep *imageRep = [[svgImageRep alloc] initWithData:imageData];
        if (!imageRep)
            return nil;

        result = [[NSImage alloc] init];
        [result addRepresentation:imageRep];
        result.size = imageRep.size;
#else
        CGSVGDocumentRef document = CGSVGDocumentCreateFromData(bridge_cast(imageData), nullptr);
        if (!document)
            return nil;

        // Since we need to rasterize, scale the image for the densest display, so it will have enough pixels to be sharp.
        result = [UIImage _imageWithCGSVGDocument:document scale:largestDisplayScale() orientation:UIImageOrientationUp];
        CGSVGDocumentRelease(document);
#endif // not USE(APPKIT)
    }
#endif // !USE(NSIMAGE_FOR_SVG_SUPPORT)

    if (!result)
        result = [[CocoaImage alloc] initWithData:imageData];

#if USE(APPKIT)
    if (!CGSizeEqualToSize(sizeForResizing, CGSizeZero)) {
        // Proportionally scale the size.
        auto originalSize = result.size;
        auto aspectWidth = sizeForResizing.width / originalSize.width;
        auto aspectHeight = sizeForResizing.height / originalSize.height;
        auto aspectRatio = std::min(aspectWidth, aspectHeight);

        result.size = CGSizeMake(originalSize.width * aspectRatio, originalSize.height * aspectRatio);
    }

    return WebCore::Icon::create(result);
#else
    // Rasterization is needed because UIImageAsset will not register the image unless it is a CGImage.
    // If the image is already a CGImage bitmap, this operation is a no-op.
    result = result._rasterizedImage;

    if (!CGSizeEqualToSize(sizeForResizing, CGSizeZero) && !CGSizeEqualToSize(result.size, sizeForResizing))
        result = [result imageByPreparingThumbnailOfSize:sizeForResizing];

    return WebCore::Icon::create(result);
#endif // not USE(APPKIT)
}

RefPtr<WebCore::Icon> WebExtension::bestImageInIconsDictionary(const JSON::Value& value, WebCore::FloatSize idealSize, const Function<void(Ref<API::Error> *)>& reportError)
{
    if (!value.asObject()->size())
        return nullptr;

    auto idealPointSize = idealSize.width > idealSize.height ? idealSize.width : idealSize.height;
    auto *screenScales = availableScreenScales();
    auto *uniquePaths = [NSMutableSet set];
#if PLATFORM(IOS_FAMILY)
    auto *scalePaths = [NSMutableDictionary dictionary];
#endif

    for (NSNumber *scale in screenScales) {
        auto pixelSize = idealPointSize * scale.doubleValue;
        auto *iconPath = pathForBestImageInIconsDictionary(iconsDictionary, pixelSize);
        if (!iconPath)
            continue;

        [uniquePaths addObject:iconPath];

#if PLATFORM(IOS_FAMILY)
        scalePaths[scale] = iconPath;
#endif
    }

    if (!uniquePaths.count)
        return nil;

#if USE(APPKIT)
    // Return a combined image so the system can select the most appropriate representation based on the current screen scale.
    NSImage *resultImage;

    for (NSString *iconPath in uniquePaths) {
        NSError *resourceError;
        if (auto *image = imageForPath(iconPath, &resourceError, idealSize)) {
            if (!resultImage)
                resultImage = image;
            else
                [resultImage addRepresentations:image.representations];
        } else if (reportError && resourceError)
            reportError(resourceError);
    }

    return resultImage;
#else
    if (uniquePaths.count == 1) {
        [scalePaths removeAllObjects];

        // Add a single value back that has 0 for the scale, which is the
        // unspecified (universal) trait value for display scale.
        scalePaths[@0] = uniquePaths.anyObject;
    }

    auto *images = mapObjects<NSDictionary>(scalePaths, ^id(NSNumber *scale, NSString *path) {
        NSError *resourceError;
        if (auto *image = imageForPath(path, &resourceError, idealSize))
            return image;

        if (reportError && resourceError)
            reportError(resourceError);

        return nil;
    });

    if (images.count == 1)
        return images.allValues.firstObject;

    // Make a dynamic image asset that returns an image based on the trait collection.
    auto *imageAsset = [UIImageAsset _dynamicAssetNamed:NSUUID.UUID.UUIDString generator:^(UIImageAsset *, UIImageConfiguration *configuration, UIImage *) {
        return images[@(configuration.traitCollection.displayScale)] ?: images[@0];
    }];

    // The returned image retains its link to the image asset and adapts to trait changes,
    // automatically displaying the correct variant based on the current traits.
    return [imageAsset imageWithTraitCollection:UITraitCollection.currentTraitCollection];
#endif // not USE(APPKIT)
}

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
CocoaImage *WebExtension::bestImageForIconVariants(NSArray *variants, CGSize idealSize, const Function<void(NSError *)>& reportError)
{
    auto idealPointSize = idealSize.width > idealSize.height ? idealSize.width : idealSize.height;
    auto *lightIconsDictionary = iconsDictionaryForBestIconVariant(variants, idealPointSize, ColorScheme::Light);
    auto *darkIconsDictionary = iconsDictionaryForBestIconVariant(variants, idealPointSize, ColorScheme::Dark);

    // If the light and dark icons dictionaries are the same, or if either is nil, return the available image directly.
    if (!lightIconsDictionary || !darkIconsDictionary || [lightIconsDictionary isEqualToDictionary:darkIconsDictionary])
        return bestImageInIconsDictionary(lightIconsDictionary ?: darkIconsDictionary, idealSize, reportError);

    auto *lightImage = bestImageInIconsDictionary(lightIconsDictionary, idealSize, reportError);
    auto *darkImage = bestImageInIconsDictionary(darkIconsDictionary, idealSize, reportError);

    // If either the light or dark icon is nil, return the available image directly.
    if (!lightImage || !darkImage)
        return lightImage ?: darkImage;

#if USE(APPKIT)
    // The images need to be the same size to draw correctly in the block.
    auto imageSize = lightImage.size.width >= darkImage.size.width ? lightImage.size : darkImage.size;
    lightImage.size = imageSize;
    darkImage.size = imageSize;

    // Make a dynamic image that draws the light or dark image based on the current appearance.
    return [NSImage imageWithSize:imageSize flipped:NO drawingHandler:^BOOL(NSRect rect) {
        static auto *darkAppearanceNames = @[
            NSAppearanceNameDarkAqua,
            NSAppearanceNameVibrantDark,
            NSAppearanceNameAccessibilityHighContrastDarkAqua,
            NSAppearanceNameAccessibilityHighContrastVibrantDark,
        ];

        if ([NSAppearance.currentDrawingAppearance bestMatchFromAppearancesWithNames:darkAppearanceNames])
            [darkImage drawInRect:rect];
        else
            [lightImage drawInRect:rect];

        return YES;
    }];
#else
    // Make a dynamic image asset that returns the light or dark image based on the trait collection.
    auto *imageAsset = [UIImageAsset _dynamicAssetNamed:NSUUID.UUID.UUIDString generator:^(UIImageAsset *, UIImageConfiguration *configuration, UIImage *) {
        return configuration.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark ? darkImage : lightImage;
    }];

    // The returned image retains its link to the image asset and adapts to trait changes,
    // automatically displaying the correct variant based on the current traits.
    return [imageAsset imageWithTraitCollection:UITraitCollection.currentTraitCollection];
#endif // not USE(APPKIT)
}
#endif // ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
