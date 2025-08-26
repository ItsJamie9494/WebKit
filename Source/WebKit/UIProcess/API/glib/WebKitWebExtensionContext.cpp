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

#include "WebKitWebExtensionMatchPatternPrivate.h"

#include "WebKitError.h"
#include "WebKitPrivate.h"
#include "WebExtensionContext.h"
#include "WebKitWebExtensionPrivate.h"
#include <wtf/URLParser.h>
#include <WebCore/platform/LegacySchemeRegistry.h>

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
    CString optionsPageURI;
    CString overrideNewTabPageURI;
    GRefPtr<GPtrArray> unsupportedAPIs;
    GRefPtr<GPtrArray> currentPermissions;
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

#if ENABLE(WK_WEB_EXTENSIONS)

/**
 * webkit_web_extension_context_new_for_extension:
 * @extension: a #WebKitWebExtension
 * @error: return location for error or %NULL to ignore
 *
 * Create a new #WebKitWebExtensionContext for the provided #WebKitWebExtension
 * 
 * Returns: the newly created #WebKitWebExtensionContext
 * 
 * Since: 2.52
 */
WebKitWebExtensionContext* webkit_web_extension_context_new_for_extension(WebKitWebExtension* extension, GError** error)
{
    auto webExtension = webkitWebExtensionGetInternalExtension(extension);
    Ref context = WebKit::WebExtensionContext::create(*webExtension);

    if (!context->errors().isEmpty()) {
        Ref internalError = context->errors().last();
        g_set_error(error, webkit_web_extension_context_error_quark(),
            toWebKitWebExtensionContextError(internalError->errorCode()), internalError->localizedDescription().utf8().data(), nullptr);
    }

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
 * Returns: (nullable) (transfer none): a #WebKitWebExtension, or %NULL if no web extension is available.
 * 
 * Since: 2.52
 */
WebKitWebExtension* webkit_web_extension_context_get_web_extension(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);

    return context->priv->extension.get();
}

/**
 * webkit_web_extension_context_get_base_uri:
 * @context: a #WebKitWebExtensionContext
 *
 * Get the base URI this context uses for loading extension resources or injecting content into webpages.
 * The default value is a unique URI using the `webkit-extension` scheme.
 * 
 * Returns: the base URI
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_context_get_base_uri(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto baseURI = priv->context->baseURL();
    return baseURI.string().utf8().data();
}

/**
 * webkit_web_extension_context_set_base_uri:
 * @context: a #WebKitWebExtensionContext
 * @base_uri: The base URI to use for this context.
 *
 * Sets the base URI this context uses for loading extension resources or injecting content into webpages.
 * 
 * The base URI can be set to any URI, but only the scheme and host will be used. The scheme cannot be a scheme that is
 * already supported by #WebKitWebView (e.g. http, https, etc.) Setting is only allowed when the context is not loaded.
 * 
 * Since: 2.52
 */
void webkit_web_extension_context_set_base_uri(WebKitWebExtensionContext* context, const gchar* base_uri)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(base_uri);
    auto baseURI = URL { String::fromUTF8(base_uri) };
    g_return_if_fail(WTF::URLParser::maybeCanonicalizeScheme(baseURI.protocol()));
    g_return_if_fail(WebKit::WebExtensionMatchPattern::extensionSchemes().contains(baseURI.protocol().toStringWithoutCopying()));
    g_return_if_fail(WebCore::LegacySchemeRegistry::isBuiltinScheme(baseURI.protocol().toStringWithoutCopying()));
    g_return_if_fail(baseURI.path().length() || g_strcmp0(baseURI.path().utf8().data(), "/"));

    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setBaseURL(WTFMove(baseURI));
}

/**
 * webkit_web_extension_context_get_unique_identifier:
 * @context: a #WebKitWebExtensionContext
 *
 * Get the unique identifier used to distinguish the extension from other extensions and target it for messages.
 * 
 * The default value is a unique value that matches the host in the default base URI. The identifier can be any
 * value that is unique. This value is accessible by the extension via `browser.runtime.id` and is used for
 * messaging the extension via `browser.runtime.sendMessage()`.
 * 
 * Returns: the unique identifier
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_context_get_unique_identifier(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto uniqueIdentifier = priv->context->uniqueIdentifier();
    return uniqueIdentifier.utf8().data();
}

/**
 * webkit_web_extension_context_set_unique_identifier:
 * @context: a #WebKitWebExtensionContext
 * @unique_identifier: The unique identifier to use for this context.
 *
 * Sets the unique identifier used to distinguish the extension from other extensions and target it for messages.
 * 
 * The identifier can be any value that is unique. Setting is only allowed when the context is not loaded.
 * 
 * Since: 2.52
 */
void webkit_web_extension_context_set_unique_identifier(WebKitWebExtensionContext* context, const gchar* unique_identifier)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(unique_identifier);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setUniqueIdentifier(String::fromUTF8(unique_identifier));
}

/**
 * webkit_web_extension_context_get_is_inspectable:
 * @context: a #WebKitWebExtensionContext
 *
 * Gets whether Web Inspector can inspect the #WebKitWebView instances for this context.
 * 
 * A context can control multiple #WebKitWebView instances, from the background content, to the popover.
 * You should set this to `TRUE` when needed for debugging purposes. The default value is `FALSE`.
 * 
 * Returns: `TRUE` if the Web Inspector can inspect the web views.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_context_get_is_inspectable(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->isInspectable();
}

/**
 * webkit_web_extension_context_set_is_inspectable:
 * @context: a #WebKitWebExtensionContext
 * @is_inspectable: Whether the Web Inspector can inspect the #WebKitWebView instances
 *
 * Sets whether Web Inspector can inspect the #WebKitWebView instances for this context.
 * 
 * A context can control multiple #WebKitWebView instances, from the background content, to the popover.
 * You should set this to `TRUE` when needed for debugging purposes. The default value is `FALSE`.
 * 
 * Since: 2.52
 */
void webkit_web_extension_context_set_is_inspectable(WebKitWebExtensionContext* context, gboolean is_inspectable)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setInspectable(is_inspectable);
}

/**
 * webkit_web_extension_context_get_inspection_name:
 * @context: a #WebKitWebExtensionContext
 *
 * Get the name shown when inspecting the background web view.
 * 
 * Returns: (nullable): the name shown, or %NULL if there is no name set.
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_context_get_inspection_name(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    // WebKitWebExtensionContextPrivate* priv = context->priv;
    // FIXME: BROKEN
    return "";
    // auto inspectionName = priv->context->backgroundWebViewInspectionName();
    // return inspectionName.utf8().data();
}

/**
 * webkit_web_extension_context_set_inspection_name:
 * @context: a #WebKitWebExtensionContext
 * @inspection_name: The name to show when inspecting the background web view
 *
 * Sets the name shown when inspecting the background web view.
 * 
 * Since: 2.52
 */
void webkit_web_extension_context_set_inspection_name(WebKitWebExtensionContext* context, const gchar* inspection_name)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(inspection_name);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto inspectionName = String::fromUTF8(inspection_name);
    priv->context->setBackgroundWebViewInspectionName(inspectionName);
}

/**
 * webkit_web_extension_context_get_unsupported_apis:
 * @context: a #WebKitWebExtensionContext
 *
 * Get the unsupported APIs for this extension.
 * 
 * This allows the app to specify a subset of web extension APIs that it chooses not to support, effectively making
 * these APIs `undefined` within the extension's JavaScript contexts. This enables extensions to employ feature detection techniques
 * for unsupported APIs, allowing them to adapt their behavior based on the APIs actually supported by the app.
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer none): a %NULL-terminiated array of strings representing
 * APIs that are currently marked as unsupported, and therefore `undefined` in this extension's JavaScript contexts,
 * or %NULL otherwise. This array and its contents are owned by WebKit and should not be modified or freed.
 * 
 * Since: 2.52
 */
const gchar* const * webkit_web_extension_context_get_unsupported_apis(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto unsupportedAPIs = priv->context->unsupportedAPIs();
    if (!unsupportedAPIs.size())
        return nullptr;

    priv->unsupportedAPIs = adoptGRef(g_ptr_array_new_with_free_func(g_free));
    for (auto api : unsupportedAPIs)
        g_ptr_array_add(priv->unsupportedAPIs.get(), g_strdup(api.utf8().data()));
    g_ptr_array_add(priv->unsupportedAPIs.get(), nullptr);

    return reinterpret_cast<gchar**>(priv->unsupportedAPIs->pdata);
}

/**
 * webkit_web_extension_context_set_unsupported_apis:
 * @context: a #WebKitWebExtensionContext
 * @unsupported_apis: (allow-none) (array zero-terminated=1) (element-type utf8) (transfer none): A %NULL-terminated array of stringsrepresenting APIs that should be marked as unsupported,
 * and therefore `undefined` in this extension's JavaScript contexts.
 *
 * Specify unsupported APIs for this extension, making them `undefined` in JavaScript.
 * 
 * This allows the app to specify a subset of web extension APIs that it chooses not to support, effectively making
 * these APIs `undefined` within the extension's JavaScript contexts. This enables extensions to employ feature detection techniques
 * for unsupported APIs, allowing them to adapt their behavior based on the APIs actually supported by the app. Setting is only allowed when
 * the context is not loaded. Only certain APIs can be specified here, particularly those within the `browser` namespace and other dynamic
 * functions and properties, anything else will be silently ignored.
 * 
 * For example, specifying `"browser.windows.create"` and `"browser.storage"` in this set will result in the
 * `browser.windows.create()` function and `browser.storage` property being `undefined`.
 * 
 * Since: 2.52
 */
void webkit_web_extension_context_set_unsupported_apis(WebKitWebExtensionContext* context, const gchar* const* unsupported_apis)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(unsupported_apis);

    // FIXME: This code, if it works, will be USEFUL in the internal impl
    WebKitWebExtensionContextPrivate* priv = context->priv;
    HashSet<String> unsupportedAPIs;
    for (const char* api : span(const_cast<char**>(unsupported_apis)))
        unsupportedAPIs.add(String::fromUTF8(api));

    priv->context->setUnsupportedAPIs(WTFMove(unsupportedAPIs));
}

/**
 * webkit_web_extension_context_get_options_page_uri:
 * @context: a #WebKitWebExtensionContext
 *
 * Get the URI of the extension's options page, if the extension has one.
 * 
 * Provides the URI for the dedicated options page, if provided by the extension; otherwise %NULL if no page is defined.
 * The app should provide access to this page through a user interface element. Navigation to the options page is only
 * possible after this extension has been loaded.
 * 
 * Returns: (nullable): the URI of the extension's options page, or %NULL if the extension does not have one.
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_context_get_options_page_uri(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (!priv->optionsPageURI.isNull())
        return priv->optionsPageURI.data();

    auto optionsPageURI = priv->context->optionsPageURL();
    if (optionsPageURI.isEmpty())
        return nullptr;

    priv->optionsPageURI = optionsPageURI.string().utf8();
    return priv->optionsPageURI.data();
}

/**
 * webkit_web_extension_context_get_override_new_tab_page_uri:
 * @context: a #WebKitWebExtensionContext
 *
 * Get the URI to use as an alternative to the default new tab page, if the extension has one.
 * 
 * Provides the URI for a new tab page, if provided by the extension; otherwise %NULL if no page is defined.
 * The app should prompt the user for permission to use the extension's new tab page as the default.
 * Navigation to the override new tab page is only possible after this extension has been loaded.
 * 
 * Returns: (nullable): the URI to use as an alternative to the default new tab page, or %NULL if the extension
 * does not have one.
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_context_get_override_new_tab_page_uri(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (!priv->overrideNewTabPageURI.isNull())
        return priv->overrideNewTabPageURI.data();

    auto overrideNewTabPageURI = priv->context->overrideNewTabPageURL();
    if (overrideNewTabPageURI.isEmpty())
        return nullptr;

    priv->overrideNewTabPageURI = overrideNewTabPageURI.string().utf8();
    return priv->overrideNewTabPageURI.data();
}

/**
 * webkit_web_extension_context_get_has_requested_optional_access_to_all_hosts:
 * @context: a #WebKitWebExtensionContext
 *
 * Get whether the extension has requested optional access to all hosts.
 * 
 * If this property is `TRUE`, the extension has asked for access to all hosts in a call to `browser.runtime.permissions.request()`,
 * and future permission checks will present discrete hosts for approval as being implicitly requested.
 * 
 * This value should be saved and restored as needed by the app via
 * webkit_web_extension_context_set_has_requested_optional_access_to_all_hosts().
 * 
 * Returns: %TRUE if the extension has requested access to all hosts.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_context_get_has_requested_optional_access_to_all_hosts(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->requestedOptionalAccessToAllHosts();
}

/**
 * webkit_web_extension_context_set_has_requested_optional_access_to_all_hosts:
 * @context: a #WebKitWebExtensionContext
 * @requested_access_to_all_hosts: Whether to request optional access to all hosts
 *
 * Set whether the extension has requested optional access to all hosts.
 * 
 * If this property is `TRUE`, the extension has asked for access to all hosts in a call to `browser.runtime.permissions.request()`,
 * and future permission checks will present discrete hosts for approval as being implicitly requested.
 * 
 * Since: 2.52
 */
void webkit_web_extension_context_set_has_requested_optional_access_to_all_hosts(WebKitWebExtensionContext* context, gboolean requested_access_to_all_hosts)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));

    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setRequestedOptionalAccessToAllHosts(requested_access_to_all_hosts);

    // FIXME: notify parametre change?
}

/**
 * webkit_web_extension_context_get_has_access_to_private_data:
 * @context: a #WebKitWebExtensionContext
 *
 * Get whether the extension has access to private data.
 * 
 * If this property is `TRUE`, the extension is granted permission to interact with private windows, tabs, and cookies. Access to private data
 * should be explicitly allowed by the user before setting this property.
 * 
 * This value should be saved and restored as needed by the app via
 * webkit_web_extension_context_set_has_access_to_private_data().
 * 
 * Returns: %TRUE is the extension has been granted permission to access private data.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_context_get_has_access_to_private_data(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasAccessToPrivateData();
}

/**
 * webkit_web_extension_context_set_has_access_to_private_data:
 * @context: a #WebKitWebExtensionContext
 * @has_access_to_private_data: Whether the extension should have access to private data.
 *
 * Sets whether the extension has access to private data.
 * 
 * If this property is `TRUE`, the extension is granted permission to interact with private windows, tabs, and cookies. Access to private data
 * should be explicitly allowed by the user before setting this property.
 * 
 * To ensure proper isolation between private and non-private data, web views associated with private data must use a
 * different #WebKitUserContentManager. Likewise, to be identified as a private web view and to ensure that cookies and other
 * website data is not shared, private web views must be configured to use a non-persistent #WebKitWebsiteDataManager.
 * 
 * Since: 2.52
 */
void webkit_web_extension_context_set_has_access_to_private_data(WebKitWebExtensionContext* context, gboolean has_access_to_private_data)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));

    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setHasAccessToPrivateData(has_access_to_private_data);

    // FIXME: notify parametre change?
}

/**
 * webkit_web_extension_context_get_current_permissions:
 * @context: a #WebKitWebExtensionContext
 *
 * Get the currently granted permissions that have not expired.
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer none): A %NULL-terminiated list of permissions that have
 * been granted to the extension and have not expired, or %NULL otherwise.
 * 
 * Since: 2.52
 */
const gchar* const * webkit_web_extension_context_get_current_permissions(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto currentPermissions = priv->context->currentPermissions();
    if (!currentPermissions.size())
        return nullptr;

    priv->currentPermissions = adoptGRef(g_ptr_array_new_with_free_func(g_free));
    for (auto permission : currentPermissions)
        g_ptr_array_add(priv->currentPermissions.get(), g_strdup(permission.utf8().data()));
    g_ptr_array_add(priv->currentPermissions.get(), nullptr);

    return reinterpret_cast<gchar**>(priv->currentPermissions->pdata);
}

/**
 * webkit_web_extension_context_get_current_permission_match_patterns:
 * @context: a #WebKitWebExtensionContext
 *
 * Get the currently granted permission match patterns that have not expired.
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer none): A %NULL-terminiated list of match patterns that have
 * been granted to the extension and have not expired, or %NULL otherwise.
 * 
 * Since: 2.52
 */
WebKitWebExtensionMatchPattern** webkit_web_extension_context_get_current_permission_match_patterns(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto matchPatterns = priv->context->currentPermissionMatchPatterns();
    if (!matchPatterns.size())
        return nullptr;

    GPtrArray* currentPermissionMatchPatterns = g_ptr_array_new_with_free_func(g_free);
    for (Ref pattern : matchPatterns)
        g_ptr_array_add(currentPermissionMatchPatterns, webkitWebExtensionMatchPatternCreate(pattern));
    g_ptr_array_add(currentPermissionMatchPatterns, nullptr);

    return reinterpret_cast<WebKitWebExtensionMatchPattern**>(g_ptr_array_free(currentPermissionMatchPatterns, FALSE));
}

/**
 * webkit_web_extension_context_has_permission:
 * @context: a #WebKitWebExtensionContext
 * @permission: The permission for which to return the status
 *
 * Checks the specified permission against the currently granted permissions.
 * 
 * Returns: %TRUE if the extension has been granted the specified permission.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_context_has_permission(WebKitWebExtensionContext* context, const gchar* permission)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(permission, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasPermission(String::fromUTF8(permission), nullptr);
}

/**
 * webkit_web_extension_context_has_access_to_uri:
 * @context: a #WebKitWebExtensionContext
 * @uri: The URI for which to return the status
 *
 * Checks the specified URI against the currently granted permission match patterns.
 * 
 * Returns: %TRUE if the URI is accessible by the extension.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_context_has_access_to_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(uri, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasPermission(URL { String::fromUTF8(uri) }, nullptr);
}

/**
 * webkit_web_extension_context_get_has_access_to_all_uris:
 * @context: a #WebKitWebExtensionContext
 *
 * Get whether the currently granted permission match patterns set contains the `<all_urls>` pattern.
 * 
 * This does not check for any `*` host patterns. In most cases you should use the broader
 * webkit_web_extension_context_get_has_access_to_all_hosts().
 * 
 * Returns: %TRUE if the `<all_urls>` pattern is present.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_context_get_has_access_to_all_uris(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasAccessToAllURLs();
}

/**
 * webkit_web_extension_context_get_has_access_to_all_hosts:
 * @context: a #WebKitWebExtensionContext
 *
 * Get  whether the currently granted permission match patterns set contains the `<all_urls>` pattern or any `*` host patterns.
 * 
 * Returns: %TRUE if the `<all_urls>` pattern or any `*` host patterns are present.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_context_get_has_access_to_all_hosts(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasAccessToAllHosts();
}

/**
 * webkit_web_extension_context_get_has_injected_content:
 * @context: a #WebKitWebExtensionContext
 *
 * Get whether the extension has script or stylesheet content that can be injected into webpages.
 * 
 * If this property is %TRUE, the extension has content that can be injected by matching against the extension's requested match patterns.
 * 
 * Returns: %TRUE if the extension contains content that can be injected into webpages.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_context_get_has_injected_content(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasInjectedContent();
}

/**
 * webkit_web_extension_context_has_injected_content_for_uri:
 * @context: a #WebKitWebExtensionContext
 * @uri: The webpage URI to check
 *
 * Checks if the extension has script or stylesheet content that can be injected into the specified URL.
 * 
 * The extension context will still need to be loaded and have granted website permissions for its content to actually be injected.
 * 
 * Returns: %TRUE if the extension has content that can be injected by matching the URL against the extension's requested match patterns.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_context_has_injected_content_for_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(uri, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasInjectedContentForURL(URL { String::fromUTF8(uri) });
}

/**
 * webkit_web_extension_context_get_has_content_modification_rules:
 * @context: a #WebKitWebExtensionContext
 *
 * Get whether the extension includes rules used for content modification or blocking.
 * 
 * This includes both static rules available in the extension's manifest and dynamic rules applied during a browsing session.
 * 
 * Returns: %TRUE if the extension includes rules used for content modification or blocking.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_context_get_has_content_modification_rules(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasContentModificationRules();
}

static inline WebKitWebExtensionContextPermissionStatus toAPI(WebKit::WebExtensionContext::PermissionState status)
{
    switch (status) {
    case WebKit::WebExtensionContext::PermissionState::DeniedExplicitly:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY;
    case WebKit::WebExtensionContext::PermissionState::DeniedImplicitly:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_IMPLICITLY;
    case WebKit::WebExtensionContext::PermissionState::RequestedImplicitly:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY;
    case WebKit::WebExtensionContext::PermissionState::Unknown:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN;
    case WebKit::WebExtensionContext::PermissionState::RequestedExplicitly:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY;
    case WebKit::WebExtensionContext::PermissionState::GrantedImplicitly:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_IMPLICITLY;
    case WebKit::WebExtensionContext::PermissionState::GrantedExplicitly:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY;
    }
    return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN;
}

static inline WebKit::WebExtensionContext::PermissionState toImpl(WebKitWebExtensionContextPermissionStatus status)
{
    switch (status) {
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY:
        return WebKit::WebExtensionContext::PermissionState::DeniedExplicitly;
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_IMPLICITLY:
        return WebKit::WebExtensionContext::PermissionState::DeniedImplicitly;
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY:
        return WebKit::WebExtensionContext::PermissionState::RequestedImplicitly;
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN:
        return WebKit::WebExtensionContext::PermissionState::Unknown;
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY:
        return WebKit::WebExtensionContext::PermissionState::RequestedExplicitly;
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_IMPLICITLY:
        return WebKit::WebExtensionContext::PermissionState::GrantedImplicitly;
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY:
        return WebKit::WebExtensionContext::PermissionState::GrantedExplicitly;
    }
    return WebKit::WebExtensionContext::PermissionState::Unknown;
}

/**
 * webkit_web_extension_context_permission_status_for_permission:
 * @context: a #WebKitWebExtensionContext
 * @permission: The permission for which to return the status.
 *
 * Checks the specified permission against the currently denied, granted, and requested permissions.
 * 
 * Returns: the status of the requested permission
 * 
 * Since: 2.52
 */
WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_permission_status_for_permission(WebKitWebExtensionContext* context, const gchar* permission)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(permission, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return toAPI(priv->context->permissionState(String::fromUTF8(permission), nullptr));
}

/**
 * webkit_web_extension_context_permission_status_for_uri:
 * @context: a #WebKitWebExtensionContext
 * @uri: The URI for which to return the status.
 *
 * Checks the specified URL against the currently denied, granted, and requested permission match patterns.
 * 
 * Returns: the permission status of the requested URI
 * 
 * Since: 2.52
 */
WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_permission_status_for_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(uri, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return toAPI(priv->context->permissionState(URL { String::fromUTF8(uri) }, nullptr));
}

/**
 * webkit_web_extension_context_permission_status_for_match_pattern:
 * @context: a #WebKitWebExtensionContext
 * @pattern: The pattern for which to return the status.
 *
 * Checks the specified match pattern against the currently denied, granted, and requested permission match patterns.
 * 
 * Returns: the permission status of the requested match pattern
 * 
 * Since: 2.52
 */
WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_permission_status_for_match_pattern(WebKitWebExtensionContext* context, WebKitWebExtensionMatchPattern* pattern)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(pattern, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return toAPI(priv->context->permissionState(*webkitWebExtensionMatchPatternToImpl(pattern), nullptr));
}

/**
 * webkit_web_extension_context_set_permission_status_for_permission:
 * @context: a #WebKitWebExtensionContext
 * @permission: The permission for which to set the status
 * @status: The new permission status to set for the given permission.
 *
 * Sets the status of a permission with a distant future expiration date.
 * 
 * This method will update ``grantedPermissions`` and ``deniedPermissions``. Use this method for changing a single permission's status.
 * Only #WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY, #WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN,
 * and #WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY states are allowed to be set using this method.
 * 
 * Since: 2.52
 */
void webkit_web_extension_context_set_permission_status_for_permission(WebKitWebExtensionContext* context, const gchar* permission, WebKitWebExtensionContextPermissionStatus status)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(permission);
    g_return_if_fail(status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setPermissionState(toImpl(status), String::fromUTF8(permission));
}

/**
 * webkit_web_extension_context_set_permission_status_for_uri:
 * @context: a #WebKitWebExtensionContext
 * @uri: The URI for which to set the status
 * @status: The new permission status to set for the given permission.
 *
 * Sets the permission status of a URL with a distant future expiration date.
 * 
 * The URL is converted into a match pattern and will update ``grantedPermissionMatchPatterns`` and ``deniedPermissionMatchPatterns``.
 * Use this method for changing a single URL's status.
 * Only #WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY, #WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN,
 * and #WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY states are allowed to be set using this method.
 * 
 * Since: 2.52
 */
void webkit_web_extension_context_set_permission_status_for_uri(WebKitWebExtensionContext* context, const gchar* uri, WebKitWebExtensionContextPermissionStatus status)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(uri);
    g_return_if_fail(status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setPermissionState(toImpl(status), URL { String::fromUTF8(uri) });
}

/**
 * webkit_web_extension_context_set_permission_status_for_match_pattern:
 * @context: a #WebKitWebExtensionContext
 * @pattern: The match pattern for which to set the status
 * @status: The new permission status to set for the given permission.
 *
 * Sets the status of a match pattern with a distant future expiration date.
 * 
 * This method will update ``grantedPermissionMatchPatterns`` and ``deniedPermissionMatchPatterns``.
 * Use this method for changing a single match pattern's status.
 * Only #WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY, #WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN,
 * and #WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY states are allowed to be set using this method.
 * 
 * Since: 2.52
 */
void webkit_web_extension_context_set_permission_status_for_match_pattern(WebKitWebExtensionContext* context, WebKitWebExtensionMatchPattern* pattern, WebKitWebExtensionContextPermissionStatus status)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(pattern);
    g_return_if_fail(status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setPermissionState(toImpl(status), *webkitWebExtensionMatchPatternToImpl(pattern));
}

/**
 * webkit_web_extension_context_load_background_content:
 * @context: a #WebKitWebExtensionContext
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously loads the background content if needed for the extension.
 * 
 * This method forces the loading of the background content for the extension that will otherwise be loaded on-demand during specific events.
 * It is useful when the app requires the background content to be loaded for other reasons.
 * 
 * When the operation is finished, or if the background content is already loaded, @callback will be called. 
 * 
 * You can then call webkit_web_extension_context_load_background_content_finish() to get the result of the operation.
 * 
 * Since: 2.52
 */
void webkit_web_extension_context_load_background_content(WebKitWebExtensionContext* context, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));

    GRefPtr<GTask> task = adoptGRef(g_task_new(context, cancellable, callback, user_data));
    context->priv->context->loadBackgroundContent([task = WTFMove(task)](RefPtr<API::Error> error) {
        if (error)
            g_task_return_new_error(task.get(), webkit_web_extension_context_error_quark(),
                toWebKitWebExtensionContextError(error->errorCode()), error->localizedDescription().utf8().data(), nullptr);
        else
            g_task_return_boolean(task.get(), TRUE);
    });
}

/**
 * webkit_web_extension_context_load_background_content_finish:
 * @context: a #WebKitWebExtensionContext
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_extension_context_load_background_content_finish().
 * 
 * An error will occur if the extension does not have any background content to load or loading fails.
 * 
 * Returns: %TRUE if the background content was loaded or %FALSE in case of error.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_context_load_background_content_finish(WebKitWebExtensionContext* context, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(g_task_is_valid(result, context), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

#else // ENABLE(WK_WEB_EXTENSIONS)

WebKitWebExtensionContext* webkit_web_extension_context_new_for_extension(WebKitWebExtension* extension, GError** error)
{
    return nullptr;
}

WebKitWebExtension* webkit_web_extension_context_get_web_extension(WebKitWebExtensionContext* context)
{
    return nullptr;
}

const gchar* webkit_web_extension_context_get_base_uri(WebKitWebExtensionContext* context)
{
    return "";
}

void webkit_web_extension_context_set_base_uri(WebKitWebExtensionContext* context, const gchar* base_uri)
{
    return;
}

const gchar* webkit_web_extension_context_get_unique_identifier(WebKitWebExtensionContext* context)
{
    return "";
}

void webkit_web_extension_context_set_unique_identifier(WebKitWebExtensionContext* context, const gchar* unique_identifier)
{
    return;
}

gboolean webkit_web_extension_context_get_is_inspectable(WebKitWebExtensionContext* context)
{
    return 0;
}

void webkit_web_extension_context_set_is_inspectable(WebKitWebExtensionContext* context, gboolean is_inspectable)
{
    return;
}

const gchar* webkit_web_extension_context_get_inspection_name(WebKitWebExtensionContext* context)
{
    return "";
}

void webkit_web_extension_context_set_inspection_name(WebKitWebExtensionContext* context, const gchar* inspection_name)
{
    return;
}

const gchar* const * webkit_web_extension_context_get_unsupported_apis(WebKitWebExtensionContext* context)
{
    return nullptr;
}

void webkit_web_extension_context_set_unsupported_apis(WebKitWebExtensionContext* context, const gchar* const* unsupported_apis)
{
    return;
}

const gchar* webkit_web_extension_context_get_options_page_uri(WebKitWebExtensionContext* context)
{
    return "";
}

const gchar* webkit_web_extension_context_get_override_new_tab_page_uri(WebKitWebExtensionContext* context)
{
    return "";
}

gboolean webkit_web_extension_context_get_has_requested_optional_access_to_all_hosts(WebKitWebExtensionContext* context)
{
    return 0;
}

void webkit_web_extension_context_set_has_requested_optional_access_to_all_hosts(WebKitWebExtensionContext* context, gboolean requested_optional_access_to_all_hosts)
{
    return;
}

gboolean webkit_web_extension_context_get_has_access_to_private_data(WebKitWebExtensionContext* context)
{
    return 0;
}

void webkit_web_extension_context_set_has_access_to_private_data(WebKitWebExtensionContext* context, gboolean has_access_to_private_data)
{
    return;
}

const gchar* const * webkit_web_extension_context_get_current_permissions(WebKitWebExtensionContext* context)
{
    return nullptr;
}

WebKitWebExtensionMatchPattern** webkit_web_extension_context_get_current_permission_match_patterns(WebKitWebExtensionContext* context)
{
    return nullptr;
}

gboolean webkit_web_extension_context_has_permission(WebKitWebExtensionContext* context, const gchar* permission)
{
    return 0;
}

gboolean webkit_web_extension_context_has_access_to_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    return 0;
}

gboolean webkit_web_extension_context_get_has_access_to_all_uris(WebKitWebExtensionContext* context)
{
    return 0;
}

gboolean webkit_web_extension_context_get_has_access_to_all_hosts(WebKitWebExtensionContext* context)
{
    return 0;
}

gboolean webkit_web_extension_context_get_has_injected_content(WebKitWebExtensionContext* context)
{
    return 0;
}

gboolean webkit_web_extension_context_has_injected_content_for_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    return 0;
}

gboolean webkit_web_extension_context_get_has_content_modification_rules(WebKitWebExtensionContext* context)
{
    return 0;
}

WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_permission_status_for_permission(WebKitWebExtensionContext* context, const gchar* permission)
{
    return 0;
}

WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_permission_status_for_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    return 0;
}

WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_permission_status_for_match_pattern(WebKitWebExtensionContext* context, WebKitWebExtensionMatchPattern* match_pattern)
{
    return 0;
}

void webkit_web_extension_context_set_permission_status_for_permission(WebKitWebExtensionContext* context, const gchar* permission, WebKitWebExtensionContextPermissionStatus permission_status)
{
    return;
}

void webkit_web_extension_context_set_permission_status_for_uri(WebKitWebExtensionContext* context, const gchar* uri, WebKitWebExtensionContextPermissionStatus permission_status)
{
    return;
}

void webkit_web_extension_context_set_permission_status_for_match_pattern(WebKitWebExtensionContext* context, WebKitWebExtensionMatchPattern* match_pattern, WebKitWebExtensionContextPermissionStatus permission_status)
{
    return;
}

void webkit_web_extension_context_load_background_content(WebKitWebExtensionContext* context, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    return;
}

gboolean webkit_web_extension_context_load_background_content_finish(WebKitWebExtensionContext* context, GAsyncResult* result, GError** error)
{
    return 0;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

#endif // ENABLE(2022_GLIB_API)
