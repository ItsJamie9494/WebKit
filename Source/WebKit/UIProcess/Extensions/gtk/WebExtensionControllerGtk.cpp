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
#include "WebExtensionController.h"

#include "WebExtensionDataRecord.h"
#include "WebKitFeature.h"
#include "WebKitSettings.h"
#include <wtf/CallbackAggregator.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

void WebExtensionController::initializePlatform()
{
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
                RELEASE_LOG_ERROR(Extensions, "Failed to create sqlite store for extension: " PRIVATE_LOG_STRING, uniqueIdentifier.utf8().data());
                continue;
            }

            removeStorage(storage, dataType, [aggregator, uniqueIdentifier, dataType, record = Ref { record }, extensionContext = RefPtr { extensionContext }](Expected<void, WebExtensionError>&& result) mutable {
                if (extensionContext)
                    extensionContext->invalidateStorage();
            });
        }
    }
}

bool WebExtensionController::isFeatureEnabled(const String& featureName) const
{
    WebKitSettings* settings = protectedConfiguration()->webViewConfiguration();

    g_autoptr(WebKitFeatureList) list = webkit_settings_get_all_features();
    for (gsize i = 0; i < webkit_feature_list_get_length(list); i++) {
        WebKitFeature* feature = webkit_feature_list_get(list, i);
        if (String::fromUTF8(webkit_feature_get_name(feature)) == featureName)
            return webkit_settings_get_feature_enabled(settings, feature);
    }

    return false;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
