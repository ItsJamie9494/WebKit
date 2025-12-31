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
#include "WebExtensionControllerConfiguration.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionDataRecord.h"
#include <wtf/CallbackAggregator.h>
#include <wtf/glib/Application.h>

namespace WebKit {

String WebExtensionControllerConfiguration::createStorageDirectoryPath(std::optional<WTF::UUID> identifier)
{
    String dataPath = m_defaultWebsiteDataStore->defaultBaseDataDirectory();
    RELEASE_ASSERT(!dataPath.isEmpty());

    String identifierPath = identifier ? identifier->toString().convertToASCIIUppercase() : "Default"_s;

    String appDirectoryName = String::fromUTF8(WTF::applicationID().data());
    return FileSystem::pathByAppendingComponents(dataPath, std::initializer_list<StringView>({ "WebKit"_s, appDirectoryName, "WebExtensions"_s, identifierPath }));
}

String WebExtensionControllerConfiguration::createTemporaryStorageDirectoryPath()
{
    return FileSystem::createTemporaryDirectory("WebExtensions"_s);
}

Ref<WebExtensionControllerConfiguration> WebExtensionControllerConfiguration::copy() const
{
    RefPtr<WebExtensionControllerConfiguration> result;

    if (m_identifier)
        result = create(m_identifier.value());
    else if (storageIsTemporary())
        result = createTemporary();
    else if (storageIsPersistent())
        result = createDefault();
    else
        result = createNonPersistent();

    result->setStorageDirectory(storageDirectory());
    result->setWebViewConfiguration(m_webViewConfiguration.get());
    result->setDefaultWebsiteDataStore(m_defaultWebsiteDataStore.get());

    return result.releaseNonNull();
}

WebKitSettings* WebExtensionControllerConfiguration::webViewConfiguration()
{
    if (!m_webViewConfiguration)
        m_webViewConfiguration = webkit_settings_new();
    return m_webViewConfiguration.get();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
