/*
 * Copyright (C) 2024 Igalia S.L.
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

#include "WebExtensionLocalization.h"
#include "Logging.h"

#include <wtf/Language.h>
#include <wtf/text/Base64.h>

namespace WebKit {

static constexpr auto defaultLocaleManifestKey = "default_locale"_s;

static constexpr auto generatedBackgroundPageFilename = "_generated_background_page.html"_s;
static constexpr auto generatedBackgroundServiceWorkerFilename = "_generated_service_worker.js"_s;

WebExtension::WebExtension(const JSON::Value& manifest, Resources&& resources)
    : m_manifestJSON(manifest)
    , m_resources(WTFMove(resources))
{
    auto manifestData = manifest.toJSONString();
    RELEASE_ASSERT(manifestData);

    m_resources.set("manifest.json"_s, API::Data::create(manifestData.utf8().span()));
}

bool WebExtension::manifestParsedSuccessfully()
{
    if (m_parsedManifest)
        return !!m_manifestJSON->asObject();

    m_parsedManifest = true;

    RefPtr<API::Error> error;
    RefPtr manifestData = resourceDataForPath("manifest.json"_s, error);
    if (!manifestData || error) {
        recordErrorIfNeeded(error);
        return false;
    }

    auto manifestJSON = JSON::Value::parseJSON(String(manifestData->span()));

    if (!manifestJSON) {
        recordError(createError(Error::InvalidManifest));
        return false;
    }

    if (auto manifestObject = manifestJSON->asObject()) {
        auto defaultLocale = manifestObject->getString(defaultLocaleManifestKey);
        if (!defaultLocale.isEmpty()) {
            auto parsedLocale = parseLocale(defaultLocale);
            if (!parsedLocale.languageCode.isEmpty()) {
                if (supportedLocales().contains(String(defaultLocale)))
                    m_defaultLocale = defaultLocale;
                else
                    recordError(createError(Error::InvalidDefaultLocale, WEB_UI_STRING("Unable to find `default_locale` in “_locales” folder.", "WKWebExtensionErrorInvalidManifestEntry description for missing default_locale")));
            } else
                recordError(createError(Error::InvalidDefaultLocale));
        }
    }
    
    auto localization = WebExtensionLocalization(*this);

    // RefPtr localizedManifestJSON = localization.localizedJSONforJSON(manifestJSON->asObject());

    m_manifestJSON = *manifestJSON;

    if (!m_manifestJSON || !m_manifestJSON->asObject()) {
        m_manifestJSON = JSON::Value::null();
        recordError(createError(Error::InvalidManifest));
        return false;
    }

    return true;
}

RefPtr<API::Data> WebExtension::resourceDataForPath(const String& originalPath, RefPtr<API::Error>& outError, CacheResult cacheResult, SuppressNotFoundErrors suppressErrors)
{
    ASSERT(originalPath);

    outError = nullptr;

    String path = originalPath;

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if (path.startsWith('/'))
        path = path.substring(1);

    if (path.startsWith("data:"_s)) {
        if (auto base64Position = path.find(";base64,"_s); base64Position != notFound) {
            auto base64String = base64DecodeToString(path.substring(base64Position));
            return API::Data::create(base64String.utf8().span());
        }

        if (auto commaPosition = path.find(','); commaPosition != notFound) {
            auto urlEncodedString = path.substring(commaPosition);
            // FIXME: WTF::Url is not documented very well, check that parsing a URL actually handles percent encoding
            auto decodedString = URL(urlEncodedString).string();
            return API::Data::create(decodedString.utf8().span());
        }

        ASSERT(path == "data:"_s);
        return API::Data::create(std::span<const uint8_t> { });
    }

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if (path.startsWith('/'))
        path = path.substring(1);
    
    if (RefPtr cachedObject = m_resources.get(path))
        return cachedObject;

    if (path == generatedBackgroundPageFilename || path  == generatedBackgroundServiceWorkerFilename)
        return API::Data::create(generatedBackgroundContent().utf8().span());
    
    auto resourceURL = resourceFileURLForPath(path);
    if (!resourceURL.isEmpty()) {
        if (suppressErrors == SuppressNotFoundErrors::No)
            outError = createError(Error::ResourceNotFound, WEB_UI_FORMAT_STRING("Unable to find \"%s\" in the extension’s resources. It is an invalid path.", "WKWebExtensionErrorResourceNotFound description with invalid file path", path.utf8().data()));
        return nullptr;
    }

    auto rawData = FileSystem::readEntireFile((resourceURL).fileSystemPath());
    if (!rawData.has_value()) {
        if (suppressErrors == SuppressNotFoundErrors::No)
            outError = createError(Error::ResourceNotFound, WEB_UI_FORMAT_STRING("Unable to find \"%s\" in the extension’s resources.", "WKWebExtensionErrorResourceNotFound description with file name", path.utf8().data()));
        return nullptr;
    }

    auto data = API::Data::create(*rawData);

    if (cacheResult == CacheResult::Yes)
        m_resources.set(path, data);

    return data;
}

void WebExtension::recordError(Ref<API::Error> error)
{
    RELEASE_LOG_ERROR(Extensions, "Error recorded: %s", error->platformError());

    // Only the first occurrence of each error is recorded in the array. This prevents duplicate errors,
    // such as repeated "resource not found" errors, from being included multiple times.
    if (m_errors.contains(error))
        return;

    // FIXME: properties
    // [wrapper() willChangeValueForKey:@"errors"];
    m_errors.append(error);
    // [wrapper() didChangeValueForKey:@"errors"];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
