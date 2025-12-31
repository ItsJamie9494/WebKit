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
#import "WebExtensionController.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "ContextMenuContextData.h"
#import "Logging.h"
#import "SandboxUtilities.h"
#import "WKFeature.h"
#import "WKPreferences.h"
#import "WKPreferencesPrivate.h"
#import "WKWebViewConfigurationPrivate.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WebExtensionContext.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextParameters.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionControllerMessages.h"
#import "WebExtensionControllerProxyMessages.h"
#import "WebExtensionDataRecord.h"
#import "WebExtensionEventListenerType.h"
#import "WebExtensionStorageSQLiteStore.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "_WKFeatureInternal.h"
#import <WebCore/ContentRuleListResults.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/FileSystem.h>
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import "FoundationSPI.h"
#endif

#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
#import <WebCore/ContentRuleListMatchedRule.h>
#endif

static constexpr Seconds purgeMatchedRulesInterval = 5_min;

static NSString * const WebExtensionUniqueIdentifierKey = @"uniqueIdentifier";
static NSString * const WebExtensionLocalStorageWasDeletedNotification = @"WebExtensionLocalStorageWasDeleted";

using namespace WebKit;

@interface _WKWebExtensionControllerHelper : NSObject {
    WeakPtr<WebKit::WebExtensionController> _webExtensionController;
}

- (instancetype)initWithWebExtensionController:(WebKit::WebExtensionController&)controller;

@end

@implementation _WKWebExtensionControllerHelper

- (instancetype)initWithWebExtensionController:(WebKit::WebExtensionController&)controller
{
    if (!(self = [super init]))
        return nil;

    _webExtensionController = controller;

    [NSDistributedNotificationCenter.defaultCenter addObserver:self selector:@selector(_didDeleteLocalStorage:) name:WebExtensionLocalStorageWasDeletedNotification object:nil];

    return self;
}

- (void)_didDeleteLocalStorage:(NSNotification *)notification
{
    NSString *uniqueIdentifier = objectForKey<NSString>(notification.userInfo, WebExtensionUniqueIdentifierKey);
    if (!uniqueIdentifier)
        return;

    RefPtr webExtensionController = _webExtensionController.get();
    if (!webExtensionController)
        return;

    if (RefPtr context = webExtensionController->extensionContext(uniqueIdentifier))
        context->invalidateStorage();
}

@end

namespace WebKit {

void WebExtensionController::initializePlatform()
{
    ASSERT(!m_webExtensionControllerHelper);
    m_webExtensionControllerHelper = [[_WKWebExtensionControllerHelper alloc] initWithWebExtensionController:*this];
}

void WebExtensionController::removeData(OptionSet<WebExtensionDataType> dataTypes, const Vector<Ref<WebExtensionDataRecord>>& records, CompletionHandler<void()>&& completionHandler)
{
    if (!m_configuration->storageIsPersistent() || dataTypes.isEmpty() || records.isEmpty()) {
        completionHandler();
        return;
    }

    Ref aggregator = MainRunLoopCallbackAggregator::create([completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });

    for (Ref record : records) {
        auto uniqueIdentifier = record.get().uniqueIdentifier();
        for (auto dataType : dataTypes) {
            RefPtr extensionContext = this->extensionContext(uniqueIdentifier);
            RefPtr storage = sqliteStore(storageDirectory(uniqueIdentifier), dataType, extensionContext);
            if (!storage) {
                RELEASE_LOG_ERROR(Extensions, "Failed to create sqlite store for extension: %{private}@", uniqueIdentifier.createNSString().get());
                record->addError(@"Unable to delete extension storage", dataType);
                continue;
            }

            removeStorage(storage, dataType, makeBlockPtr([aggregator, uniqueIdentifier, dataType, record = Ref { record }](Expected<void, WebExtensionError>&& result) mutable {
                if (!result)
                    record->addError(result.error().createNSString().get(), dataType);
                else
                    [NSDistributedNotificationCenter.defaultCenter postNotificationName:WebExtensionLocalStorageWasDeletedNotification object:nil userInfo:@{ WebExtensionUniqueIdentifierKey: uniqueIdentifier.createNSString().get() }];
            }));
        }
    }
}

void WebExtensionController::cookiesDidChange(API::HTTPCookieStore& cookieStore)
{
    // FIXME: <https://webkit.org/b/267514> Add support for changeInfo.

    for (Ref context : m_extensionContexts)
        context->cookiesDidChange(cookieStore);
}

bool WebExtensionController::isFeatureEnabled(const String& featureName) const
{
    WKPreferences *preferences = protectedConfiguration()->webViewConfiguration().preferences;

    auto *cocoaFeatureName = featureName.createNSString().get();
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:cocoaFeatureName])
            return [preferences _isEnabledForFeature:feature];
    }

    return false;
}

#if PLATFORM(MAC)
void WebExtensionController::addItemsToContextMenu(WebPageProxy& page, const ContextMenuContextData& contextData, NSMenu *menu)
{
    [menu addItem:NSMenuItem.separatorItem];

    for (Ref context : m_extensionContexts)
        context->addItemsToContextMenu(page, contextData, menu);
}
#endif

// MARK: declarativeNetRequest

void WebExtensionController::handleContentRuleListNotification(WebPageProxyIdentifier pageID, URL& url, WebCore::ContentRuleListResults& results)
{
    bool savedMatchedRule = false;

    for (const auto& result : results.results) {
        auto contentRuleListIdentifier = result.first;
        for (Ref context : m_extensionContexts) {
            if (context->uniqueIdentifier() != contentRuleListIdentifier)
                continue;

            RefPtr tab = context->getTab(pageID);
            if (!tab)
                break;

            savedMatchedRule |= context->handleContentRuleListNotificationForTab(*tab, url, result.second);

            break;
        }
    }

    if (!savedMatchedRule || m_purgeOldMatchedRulesTimer)
        return;

    m_purgeOldMatchedRulesTimer = makeUnique<RunLoop::Timer>(RunLoop::mainSingleton(), "WebExtensionController::PurgeOldMatchedRulesTimer"_s, this, &WebExtensionController::purgeOldMatchedRules);
    m_purgeOldMatchedRulesTimer->startRepeating(purgeMatchedRulesInterval);
}

#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
void WebExtensionController::handleContentRuleListMatchedRule(WebPageProxyIdentifier pageID, WebCore::ContentRuleListMatchedRule& matchedRule)
{
    auto contentRuleListIdentifier = matchedRule.rule.extensionId;
    if (!contentRuleListIdentifier.has_value())
        return;

    for (Ref context : m_extensionContexts) {
        if (context->uniqueIdentifier() != contentRuleListIdentifier.value())
            continue;

        RefPtr tab = context->getTab(pageID);
        if (!tab)
            break;

        // FIXME: <rdar://99141106> Implement declarativeNetRequest.testMatchOutcome; until then, extensionId should be null
        matchedRule.rule.extensionId = std::nullopt;
        matchedRule.request.tabId = toWebAPI(tab->identifier());
        context->handleContentRuleListMatchedRule(*tab, matchedRule);

        break;
    }
}
#endif

// MARK: Inspector

#if ENABLE(INSPECTOR_EXTENSIONS)
void WebExtensionController::inspectorWillOpen(WebInspectorUIProxy& inspector, WebPageProxy& inspectedPage)
{
    for (Ref context : m_extensionContexts)
        context->inspectorWillOpen(inspector, inspectedPage);
}

void WebExtensionController::inspectorWillClose(WebInspectorUIProxy& inspector, WebPageProxy& inspectedPage)
{
    for (Ref context : m_extensionContexts)
        context->inspectorWillClose(inspector, inspectedPage);
}
#endif // ENABLE(INSPECTOR_EXTENSIONS)

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
