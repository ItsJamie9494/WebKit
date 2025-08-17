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

#if ENABLE(2022_GLIB_API)

#include "config.h"
#include "WebKitWebExtensionContext.h"

#include "WebKitError.h"
#include "WebKitPrivate.h"
#include "WebExtensionContext.h"
#include "WebKitWebExtensionPrivate.h"

using namespace WebKit;

/**
 * WebKitWebExtensionContext:
 *
 * Represents the runtime environment for a [WebExtension](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions).
 * 
 * A #WebKitWebExtensionContext object provides methods for managing the extension's permissions, allowing it to inject content,
 * run background logic, show popovers, and display other web-based UI to the user.
 *
 * Since: 2.52
 */
struct _WebKitWebExtensionContextPrivate {
#if ENABLE(WK_WEB_EXTENSIONS)
    RefPtr<WebExtensionContext> context;
    GRefPtr<WebKitWebExtension> extension;
#endif
};

WEBKIT_DEFINE_FINAL_TYPE(WebKitWebExtensionContext, webkit_web_extension_context, G_TYPE_OBJECT, GObject)

enum {
    PROP_0,
    PROP_WEB_EXTENSION,
    N_PROPERTIES,
};

static std::array<GParamSpec*, N_PROPERTIES> properties;

static void webkitWebExtensionContextGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebExtensionContext* context = WEBKIT_WEB_EXTENSION_CONTEXT(object);

    switch (propId) {
    case PROP_WEB_EXTENSION:
        g_value_set_object(value, webkit_web_extension_context_get_web_extension(context));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_web_extension_context_class_init(WebKitWebExtensionContextClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->get_property = webkitWebExtensionContextGetProperty;

    /**
     * WebKitWebExtensionContext:web-extension:
     * 
     * The #WebKitWebExtension this #WebKitWebExtensionContext represents.
     * See webkit_web_extension_context_get_web_extension() for more details.
     *
     * Since: 2.52
     */
    properties[PROP_WEB_EXTENSION] =
        g_param_spec_object(
            "web-extension",
            nullptr, nullptr,
            WEBKIT_TYPE_WEB_EXTENSION,
            WEBKIT_PARAM_READABLE
        );
    
    g_object_class_install_properties(objectClass, properties.size(), properties.data());
}

WebKitWebExtensionContext* webkit_web_extension_context_new_for_extension(WebKitWebExtension* extension, GError** error)
{
    auto webExtension = webkitWebExtensionGetInternalExtension(extension);
    Ref context = WebKit::WebExtensionContext::create(*webExtension);

    // if (!context->errors().isEmpty()) {
    //     Ref internalError = context->errors().last();
    //     g_set_error(error, webkit_web_extension_context_error_quark(),
    //         toWebKitWebExtensionContextError(internalError->errorCode()), internalError->localizedDescription().utf8().data(), nullptr);
    // }

    WebKitWebExtensionContext* object = WEBKIT_WEB_EXTENSION_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_EXTENSION_CONTEXT, nullptr));
    object->priv->context = WTFMove(context);
    object->priv->extension = extension;
    return object;
}

/**
 * webkit_web_extension_context_get_web_extension:
 * @context: a #WebKitWebExtensionContext
 *
 * Get the #WebKitWebExtension this context represents.
 * 
 * Returns: (nullable): a #WebKitWebExtension, or %NULL if no web extension is available.
 * 
 * Since: 2.52
 */
WebKitWebExtension* webkit_web_extension_context_get_web_extension(WebKitWebExtensionContext *context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(context), nullptr);

    return context->priv->extension.get();
}

#endif // ENABLE(2022_GLIB_API)
