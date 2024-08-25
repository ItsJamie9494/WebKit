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
#import "WKWebExtensionInternal.h"

#import "APIData.h"
#import "WKNSData.h"
#import "WKNSDictionary.h"
#import "WKNSError.h"
#import "CocoaHelpers.h"
#import "CocoaImage.h"
#import "WKWebExtensionMatchPatternInternal.h"
#import "WebExtension.h"
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/URL.h>

NSErrorDomain const WKWebExtensionErrorDomain = @"WKWebExtensionErrorDomain";

@implementation WKWebExtension

#if ENABLE(WK_WEB_EXTENSIONS)

WK_OBJECT_DEALLOC_IMPL_ON_MAIN_THREAD(WKWebExtension, WebExtension, _webExtension);

+ (void)extensionWithAppExtensionBundle:(NSBundle *)appExtensionBundle completionHandler:(void (^)(WKWebExtension *extension, NSError *error))completionHandler
{
    NSParameterAssert([appExtensionBundle isKindOfClass:NSBundle.class]);

    // FIXME: <https://webkit.org/b/276194> Make the WebExtension class load data on a background thread.
    // Use an async dispatch in the meantime to prevent clients from expecting synchronous results.

    dispatch_async(dispatch_get_main_queue(), ^{
        Ref result = WebKit::WebExtension::create(appExtensionBundle);

        if (!result->manifestParsedSuccessfully()) {
            NSError * __autoreleasing error = wrapper(result->errors().last());
            completionHandler(nil, error);
            return;
        }

        completionHandler(result->wrapper(), nil);
    });
}

+ (void)extensionWithResourceBaseURL:(NSURL *)resourceBaseURL completionHandler:(void (^)(WKWebExtension *extension, NSError *error))completionHandler
{
    NSParameterAssert([resourceBaseURL isKindOfClass:NSURL.class]);
    NSParameterAssert([resourceBaseURL isFileURL]);
    NSParameterAssert([resourceBaseURL hasDirectoryPath]);

    // FIXME: <https://webkit.org/b/276194> Make the WebExtension class load data on a background thread.
    // Use an async dispatch in the meantime to prevent clients from expecting synchronous results.

    dispatch_async(dispatch_get_main_queue(), ^{
        RefPtr result = WebKit::WebExtension::createWithURL(URL(resourceBaseURL));

        if (!result)
            return;

        if (!result->manifestParsedSuccessfully()) {
            NSError * __autoreleasing error = wrapper(result->errors().last());
            completionHandler(nil, error);
            return;
        }

        completionHandler(result->wrapper(), nil);
    });
}

// FIXME: Remove after Safari has adopted new methods.
- (instancetype)initWithAppExtensionBundle:(NSBundle *)appExtensionBundle error:(NSError **)error
{
    return [self _initWithAppExtensionBundle:appExtensionBundle error:error];
}

- (instancetype)_initWithAppExtensionBundle:(NSBundle *)appExtensionBundle error:(NSError **)error
{
    NSParameterAssert([appExtensionBundle isKindOfClass:NSBundle.class]);

    if (error)
        *error = nil;

    if (!(self = [super init]))
        return nil;

    Ref result = WebKit::WebExtension::create(appExtensionBundle);

    if (!result->manifestParsedSuccessfully()) {
        if (error) {
            NSError * __autoreleasing internalError = wrapper(result->errors().last());
            *error = internalError;
        }
        return nil;
    }

    return result->wrapper();
}

// FIXME: Remove after Safari has adopted new methods.
- (instancetype)initWithResourceBaseURL:(NSURL *)resourceBaseURL error:(NSError **)error
{
    return [self _initWithResourceBaseURL:resourceBaseURL error:error];
}

- (instancetype)_initWithResourceBaseURL:(NSURL *)resourceBaseURL error:(NSError **)error
{
    NSParameterAssert([resourceBaseURL isKindOfClass:NSURL.class]);
    NSParameterAssert([resourceBaseURL isFileURL]);
    NSParameterAssert([resourceBaseURL hasDirectoryPath]);

    if (error)
        *error = nil;

    if (!(self = [super init]))
        return nil;

    RefPtr result = WebKit::WebExtension::createWithURL(URL(resourceBaseURL));

    if (!result)
        return nil;

    if (!result->manifestParsedSuccessfully()) {
        if (error) {
            NSError * __autoreleasing internalError = wrapper(result->errors().last());
            *error = internalError;
        }
        return nil;
    }

    return result->wrapper();
}

- (instancetype)_initWithManifestDictionary:(NSDictionary<NSString *, id> *)manifest
{
    NSParameterAssert([manifest isKindOfClass:NSDictionary.class]);

    return [self _initWithManifestDictionary:manifest resources:nil];
}

- (instancetype)_initWithManifestDictionary:(NSDictionary<NSString *, id> *)manifest resources:(NSDictionary<NSString *, id> *)resources
{
    NSParameterAssert([manifest isKindOfClass:NSDictionary.class]);

    if (!(self = [super init]))
        return nil;

    auto jsonManifest = String(WebKit::encodeJSONString(manifest));
    auto manifestData = JSON::Value::parseJSON(jsonManifest);
    auto resourcesData = WebKit::toDataMap(resources);
    API::Object::constructInWrapper<WebKit::WebExtension>(self, *manifestData, resourcesData);

    return self;
}

- (instancetype)_initWithResources:(NSDictionary<NSString *, id> *)resources
{
    NSParameterAssert([resources isKindOfClass:NSDictionary.class]);

    if (!(self = [super init]))
        return nil;

    auto resourcesData = WebKit::toDataMap(resources);
    API::Object::constructInWrapper<WebKit::WebExtension>(self, resourcesData);

    return self;
}

- (NSDictionary<NSString *, id> *)manifest
{
    NSData *data = wrapper(_webExtension->serializeManifest().get());
    NSDictionary *dictionary = [NSKeyedUnarchiver unarchiveObjectWithData:data];
    return dictionary;
}

- (double)manifestVersion
{
    return _webExtension->manifestVersion();
}

- (BOOL)supportsManifestVersion:(double)version
{
    return _webExtension->supportsManifestVersion(version);
}

- (NSLocale *)defaultLocale
{
    return _webExtension->defaultLocale();
}

- (NSString *)displayName
{
    return _webExtension->displayName();
}

- (NSString *)displayShortName
{
    return _webExtension->displayShortName();
}

- (NSString *)displayVersion
{
    return _webExtension->displayVersion();
}

- (NSString *)displayDescription
{
    return _webExtension->displayDescription();
}

- (NSString *)displayActionLabel
{
    return _webExtension->displayActionLabel();
}

- (NSString *)version
{
    return _webExtension->version();
}

- (CocoaImage *)iconForSize:(CGSize)size
{
    return _webExtension->icon(WebCore::FloatSize(size.width, size.height))->image().get();
}

- (CocoaImage *)actionIconForSize:(CGSize)size
{
    return _webExtension->actionIcon(WebCore::FloatSize(size.width, size.height))->image().get();
}

- (NSSet<WKWebExtensionPermission> *)requestedPermissions
{
    return WebKit::toAPI(_webExtension->requestedPermissions());
}

- (NSSet<WKWebExtensionPermission> *)optionalPermissions
{
    return WebKit::toAPI(_webExtension->optionalPermissions());
}

- (NSSet<WKWebExtensionMatchPattern *> *)requestedPermissionMatchPatterns
{
    return toAPI(_webExtension->requestedPermissionMatchPatterns());
}

- (NSSet<WKWebExtensionMatchPattern *> *)optionalPermissionMatchPatterns
{
    return toAPI(_webExtension->optionalPermissionMatchPatterns());
}

- (NSSet<WKWebExtensionMatchPattern *> *)allRequestedMatchPatterns
{
    return toAPI(_webExtension->allRequestedMatchPatterns());
}

- (NSArray<NSError *> *)errors
{
    return createNSArray(_webExtension->errors(), [] (auto&& child) -> id {
        return wrapper(child);
    }).autorelease();
}

- (BOOL)hasBackgroundContent
{
    return _webExtension->hasBackgroundContent();
}

- (BOOL)hasPersistentBackgroundContent
{
    return _webExtension->backgroundContentIsPersistent();
}

- (BOOL)hasInjectedContent
{
    return _webExtension->hasStaticInjectedContent();
}

- (BOOL)hasOptionsPage
{
    return _webExtension->hasOptionsPage();
}

- (BOOL)hasOverrideNewTabPage
{
    return _webExtension->hasOverrideNewTabPage();
}

- (BOOL)hasCommands
{
    return _webExtension->hasCommands();
}

- (BOOL)hasContentModificationRules
{
    return _webExtension->hasContentModificationRules();
}

- (BOOL)_hasServiceWorkerBackgroundContent
{
    return _webExtension->backgroundContentIsServiceWorker();
}

- (BOOL)_hasModularBackgroundContent
{
    return _webExtension->backgroundContentUsesModules();
}

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
- (BOOL)_hasSidebar
{
    return _webExtension->hasSidebar();
}
#else // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
- (BOOL)_hasSidebar
{
    return NO;
}
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_webExtension;
}

- (WebKit::WebExtension&)_webExtension
{
    return *_webExtension;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

+ (void)extensionWithAppExtensionBundle:(NSBundle *)appExtensionBundle completionHandler:(void (^)(WKWebExtension *extension, NSError *error))completionHandler
{
    completionHandler(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil]);
}

+ (void)extensionWithResourceBaseURL:(NSURL *)resourceBaseURL completionHandler:(void (^)(WKWebExtension *extension, NSError *error))completionHandler
{
    completionHandler(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil]);
}

- (instancetype)initWithAppExtensionBundle:(NSBundle *)bundle error:(NSError **)error
{
    return [self _initWithAppExtensionBundle:bundle error:error];
}

- (instancetype)_initWithAppExtensionBundle:(NSBundle *)bundle error:(NSError **)error
{
    if (error)
        *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
    return nil;
}

- (instancetype)initWithResourceBaseURL:(NSURL *)resourceBaseURL error:(NSError **)error
{
    return [self _initWithResourceBaseURL:resourceBaseURL error:error];
}

- (instancetype)_initWithResourceBaseURL:(NSURL *)resourceBaseURL error:(NSError **)error
{
    if (error)
        *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
    return nil;
}

- (instancetype)_initWithManifestDictionary:(NSDictionary<NSString *, id> *)manifest
{
    return [self _initWithManifestDictionary:manifest resources:nil];
}

- (instancetype)_initWithManifestDictionary:(NSDictionary<NSString *, id> *)manifest resources:(NSDictionary<NSString *, id> *)resources
{
    return nil;
}

- (instancetype)_initWithResources:(NSDictionary<NSString *, id> *)resources
{
    return nil;
}

- (NSDictionary<NSString *, id> *)manifest
{
    return nil;
}

- (double)manifestVersion
{
    return 0;
}

- (BOOL)supportsManifestVersion:(double)version
{
    return NO;
}

- (NSLocale *)defaultLocale
{
    return nil;
}

- (NSString *)displayName
{
    return nil;
}

- (NSString *)displayShortName
{
    return nil;
}

- (NSString *)displayVersion
{
    return nil;
}

- (NSString *)displayDescription
{
    return nil;
}

- (NSString *)displayActionLabel
{
    return nil;
}

- (NSString *)version
{
    return nil;
}

- (CocoaImage *)iconForSize:(CGSize)size
{
    return nil;
}

- (CocoaImage *)actionIconForSize:(CGSize)size
{
    return nil;
}

- (NSSet<WKWebExtensionPermission> *)requestedPermissions
{
    return nil;
}

- (NSSet<WKWebExtensionPermission> *)optionalPermissions
{
    return nil;
}

- (NSSet<WKWebExtensionMatchPattern *> *)requestedPermissionMatchPatterns
{
    return nil;
}

- (NSSet<WKWebExtensionMatchPattern *> *)optionalPermissionMatchPatterns
{
    return nil;
}

- (NSSet<WKWebExtensionMatchPattern *> *)allRequestedMatchPatterns
{
    return nil;
}

- (NSArray<NSError *> *)errors
{
    return nil;
}

- (BOOL)hasBackgroundContent
{
    return NO;
}

- (BOOL)hasPersistentBackgroundContent
{
    return NO;
}

- (BOOL)hasInjectedContent
{
    return NO;
}

- (BOOL)hasOptionsPage
{
    return NO;
}

- (BOOL)hasOverrideNewTabPage
{
    return NO;
}

- (BOOL)hasCommands
{
    return NO;
}

- (BOOL)hasContentModificationRules
{
    return NO;
}

- (BOOL)_hasServiceWorkerBackgroundContent
{
    return NO;
}

- (BOOL)_hasModularBackgroundContent
{
    return NO;
}

- (BOOL)_hasSidebar
{
    return NO;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
