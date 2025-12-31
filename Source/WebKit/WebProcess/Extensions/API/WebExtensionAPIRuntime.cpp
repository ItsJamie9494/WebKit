/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebExtensionAPIRuntime.h"

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

bool WebExtensionAPIRuntime::isPropertyAllowed(const ASCIILiteral& name, WebPage*)
{
    Ref extensionContext = this->extensionContext();
    if (extensionContext->isUnsupportedAPI(propertyPath(), name)) [[unlikely]]
        return false;

    if (name == "connectNative"_s || name == "sendNativeMessage"_s)
        return extensionContext->hasPermission("nativeMessaging"_s);

    ASSERT_NOT_REACHED();
    return false;
}

void WebExtensionAPIRuntime::getPlatformInfo(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/getPlatformInfo

#if PLATFORM(MAC)
    static constexpr auto osValue = "mac"_s;
#elif PLATFORM(IOS_FAMILY)
    static constexpr auto osValue = "ios"_s;
#else
    static constexpr auto osValue = "unknown"_s;
#endif

#if CPU(X86_64)
    static constexpr auto archValue = "x86-64"_s;
#elif CPU(ARM) || CPU(ARM64)
    static constexpr auto archValue = "arm"_s;
#else
    static constexpr auto archValue = "unknown"_s;
#endif

    auto globalContext = callback->globalContext();
    callback->call(fromObject(callback->globalContext(), {
        { "os"_s, JSValueMakeString(globalContext, toJSString(osValue).get()) },
        { "arch"_s, JSValueMakeString(globalContext, toJSString(archValue).get()) }
    }));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
