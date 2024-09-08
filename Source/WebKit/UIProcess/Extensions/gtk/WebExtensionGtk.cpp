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

#include "Logging.h"

#include <wtf/text/Base64.h>

namespace WebKit {

static String const generatedBackgroundPageFilename = "_generated_background_page.html"_s;
static String const generatedBackgroundServiceWorkerFilename = "_generated_service_worker.js"_s;

std::span<const uint8_t> WebExtension::resourceDataForPath(String path, RefPtr<API::Error> *outError, CacheResult cacheResult, SuppressNotFoundErrors suppressErrors)
{
    ASSERT(path);

    if (outError)
        *outError = nullptr;

    if (path.startsWith("data:"_s)) {
        if (auto base64Position = path.find(";base64,"_s); base64Position != notFound) {
            auto base64String = base64DecodeToString(path.substring(base64Position));
            return base64String.span8();
        }

        if (auto commaPosition = path.find(','); commaPosition != notFound) {
            auto urlEncodedString = path.substring(commaPosition);
            // FIXME: WTF::Url is not documented very well, check that parsing a URL actually handles percent encoding
            auto decodedString = URL(urlEncodedString).string();
            return decodedString.span8();
        }

        ASSERT(path == "data:"_s);
        return std::span<const uint8_t>();
    }

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if (path.startsWith('/'))
        path = path.substring(1);
    
    if (auto cachedObject = m_resources.getOptional(path)) {
        if (auto cachedString = String::fromUTF8(*cachedObject); !cachedString.isEmpty())
            return cachedString.span8();
        
        ASSERT(!cachedObject.isEmpty());
        
        return *cachedObject;
    }

    bool isServiceWorker = backgroundContentIsServiceWorker();
    if (!isServiceWorker && (path == generatedBackgroundPageFilename))
        return generatedBackgroundContent().span8();
    
    if (isServiceWorker && (path == generatedBackgroundServiceWorkerFilename))
        return generatedBackgroundContent().span8();
    
    auto resourceURL = resourceFileURLForPath(path);
    if (!resourceURL) {
        if (suppressErrors == SuppressNotFoundErrors::No && outError)
            *outError = createError(Error::ResourceNotFound, WEB_UI_FORMAT_STRING("Unable to find \"%s\" in the extension’s resources. It is an invalid path.", "WKWebExtensionErrorResourceNotFound description with invalid file path", path.utf8().data()));
        return std::span<const uint8_t>();
    }

    auto data = FileSystem::readEntireFile((*resourceURL).fileSystemPath());
    if (!data.has_value()) {
        if (suppressErrors == SuppressNotFoundErrors::No && outError)
            *outError = createError(Error::ResourceNotFound, WEB_UI_FORMAT_STRING("Unable to find \"%s\" in the extension’s resources.", "WKWebExtensionErrorResourceNotFound description with file name", path.utf8().data()));
        return std::span<const uint8_t>();
    }

    if (cacheResult == CacheResult::Yes)
        m_resources.set(String(path), (*data).span());

    return *data;
}

void WebExtension::recordError(Ref<API::Error> error)
{
    if (!m_errors.isEmpty())
        m_errors = Vector<Ref<API::Error>>();
    
    RELEASE_LOG_ERROR(Extensions, "Error recorded: %s", error.get().localizedDescription().utf8().data());

    // Only the first occurrence of each error is recorded in the array. This prevents duplicate errors,
    // such as repeated "resource not found" errors, from being included multiple times.
    if (m_errors.contains(error))
        return;

    m_errors.append(error);

    // FIXME: dispatch on Linux
}

RefPtr<WebCore::Icon> WebExtension::imageForPath(String path, RefPtr<API::Error> *outError, WebCore::FloatSize sizeForResizing)
{
    ASSERT(path);

    auto imageData = resourceDataForPath(path, outError);
    if (!imageData.empty())
        return nullptr;

    // FIXME: Resize image

    auto imageBytes = adoptGRef(g_bytes_new (imageData.data(), imageData.size()));
    GIcon *image = g_bytes_icon_new (imageBytes.get());

    return WebCore::Icon::create(image);
}

RefPtr<WebCore::Icon> WebExtension::bestImageInIconsDictionary(const JSON::Value& value, WebCore::FloatSize idealSize, const Function<void(RefPtr<API::Error> *)>& reportError)
{
    if (!value.asObject()->size())
        return nullptr;

    float idealPointSize = idealSize.width() > idealSize.height() ? idealSize.width() : idealSize.height();
    if (auto standardIconPath = pathForBestImageInIconsDictionary(value, idealPointSize)) {
        RefPtr<API::Error> error;
        auto image = imageForPath(*standardIconPath, &error);

        if (!image && reportError && error)
            reportError(&error);
        else
            return image;
    }
    
    return nullptr;
}

} // namespace WebKit
