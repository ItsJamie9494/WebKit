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
#include "WebKitWebExtensionManager.h"

#include "WebExtensionController.h"
#include "WebKitError.h"
#include "WebKitPrivate.h"
#include "WebKitWebExtensionContextPrivate.h"
#include "WebKitWebExtensionManagerInternal.h"
#include "WebKitWebExtensionPrivate.h"
#include "WebKitWebsiteDataManagerPrivate.h"
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

/**
 * WebKitWebExtensionManager:
 * 
 * Manages a set of loaded extension contexts.
 * 
 * You can have one or more extension controller instances, allowing different parts of the app to use different sets of extensions.
 * 
 * A manager must be attached to a [class@WebView] via the [property@WebView:web-extension-manager] property.
 * 
 * Since: 2.52
 */
struct _WebKitWebExtensionManagerPrivate {
#if ENABLE(WK_WEB_EXTENSIONS)
    RefPtr<WebExtensionController> controller;
    CString identifier;
    CString storageDirectory;
    GRefPtr<WebKitWebsiteDataManager> websiteDataManager;
    WebExtensionManagerDelegate* delegate;
#endif
};

WEBKIT_DEFINE_FINAL_TYPE(WebKitWebExtensionManager, webkit_web_extension_manager, G_TYPE_OBJECT, GObject)

enum {
    PROP_0,
    PROP_PERSISTENT,
    PROP_IDENTIFIER,
    PROP_SETTINGS,
    PROP_WEBSITE_DATA_MANAGER,
    N_PROPERTIES
};

static std::array<GParamSpec*, N_PROPERTIES> properties;

static void webkitWebExtensionManagerGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebExtensionManager* manager = WEBKIT_WEB_EXTENSION_MANAGER(object);

    switch (propId) {
    case PROP_PERSISTENT:
        g_value_set_boolean(value, webkit_web_extension_manager_get_persistent(manager));
        break;
    case PROP_IDENTIFIER:
        g_value_set_string(value, webkit_web_extension_manager_get_identifier(manager));
        break;
    case PROP_SETTINGS:
        g_value_set_object(value, webkit_web_extension_manager_get_settings(manager));
        break;
    case PROP_WEBSITE_DATA_MANAGER:
        g_value_set_object(value, webkit_web_extension_manager_get_website_data_manager(manager));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebExtensionManagerSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebExtensionManager* manager = WEBKIT_WEB_EXTENSION_MANAGER(object);

    switch (propId) {
    case PROP_SETTINGS:
        webkit_web_extension_manager_set_settings(manager, WEBKIT_SETTINGS(g_value_get_object(value)));
        break;
    case PROP_WEBSITE_DATA_MANAGER:
        webkit_web_extension_manager_set_website_data_manager(manager, WEBKIT_WEBSITE_DATA_MANAGER(g_value_get_object(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_web_extension_manager_class_init(WebKitWebExtensionManagerClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->get_property = webkitWebExtensionManagerGetProperty;
    objectClass->set_property = webkitWebExtensionManagerSetProperty;

    /**
     * WebKitWebExtensionManager:persistent:
     * 
     * Whether this [class@WebExtensionManager] should save data to the file system.
     * See [method@WebExtensionManager.get_persistent] for more details.
     *
     * Since: 2.52
     */
    properties[PROP_PERSISTENT] =
        g_param_spec_boolean(
            "persistent",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE
        );

    /**
     * WebKitWebExtensionManager:identifier:
     * 
     * A unique identifier to save data to the file system under.
     * See [method@WebExtensionManager.get_identifier] for more details.
     *
     * Since: 2.52
     */
    properties[PROP_IDENTIFIER] =
        g_param_spec_string(
            "identifier",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READABLE
        );

    /**
     * WebKitWebExtensionManager:settings:
     * 
     * A [class@Settings] to use for any web views created by extensions
     * connected to this [class@WebExtensionManager].
     * See [method@WebExtensionManager.get_settings] for more details.
     *
     * Since: 2.52
     */
    properties[PROP_SETTINGS] =
        g_param_spec_object(
            "settings",
            nullptr, nullptr,
            WEBKIT_TYPE_SETTINGS,
            WEBKIT_PARAM_READWRITE
        );

    /**
     * WebKitWebExtensionManager:website-data-manager:
     * 
     * A [class@WebsiteDataManager] to use for any web views created by extensions
     * connected to this [class@WebExtensionManager].
     * See [method@WebExtensionManager.get_website_data_manager] for more details.
     *
     * Since: 2.52
     */
    properties[PROP_WEBSITE_DATA_MANAGER] =
        g_param_spec_object(
            "website-data-manager",
            nullptr, nullptr,
            WEBKIT_TYPE_WEBSITE_DATA_MANAGER,
            WEBKIT_PARAM_READWRITE
        );

    g_object_class_install_properties(objectClass, properties.size(), properties.data());
}

#if ENABLE(WK_WEB_EXTENSIONS)

WebKitWebExtensionManager* webkitWebExtensionManagerNewWithTemporaryConfiguration()
{
    WebKitWebExtensionManager* object = WEBKIT_WEB_EXTENSION_MANAGER(g_object_new(WEBKIT_TYPE_WEB_EXTENSION_MANAGER, nullptr));
    Ref controller = WebKit::WebExtensionController::create(WebKit::WebExtensionControllerConfiguration::createTemporary());

    object->priv->controller = WTFMove(controller);
    object->priv->controller->setWrapper(object);
    return object;
}

gboolean webkitWebExtensionManagerGetIsTemporary(WebKitWebExtensionManager *manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), false);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    return priv->controller->protectedConfiguration()->storageIsTemporary();
}

const gchar* webkitWebExtensionManagerGetStorageDirectoryPath(WebKitWebExtensionManager *manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    if (!priv->storageDirectory.isNull())
        return priv->storageDirectory.data();

    auto directory = priv->controller->protectedConfiguration()->storageDirectory();
    if (directory.isEmpty())
        return nullptr;

    priv->storageDirectory = directory.utf8();
    return priv->storageDirectory.data();
}

RefPtr<WebKit::WebExtensionController> webkitWebExtensionManagerToImpl(WebKitWebExtensionManager *manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;

    return priv->controller;
}

void webkitWebExtensionManagerSetTestingMode(WebKitWebExtensionManager *manager, bool enabled)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager));

    manager->priv->controller->setTestingMode(enabled);
}


void webkitWebExtensionManagerSetPrivateDelegate(WebKitWebExtensionManager *manager, WebExtensionManagerDelegate *delegate)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager));

    manager->priv->delegate = delegate;
}

WebExtensionManagerDelegate* webkitWebExtensionManagerGetPrivateDelegate(WebKitWebExtensionManager *manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);

    return manager->priv->delegate;
}

/**
 * webkit_web_extension_manager_new:
 *
 * Creates a new [class@WebExtensionManager] with the default settings.
 * 
 * The default settings are to be persistent and not unique.
 * This manager, therefore, will not have a unique identifier, and data will be written
 * to the file system in a common location. When using multiple extension managers,
 * each manager should use a unique identifier to avoid conflicts.
 * 
 * Returns: the newly created [class@WebExtensionManager].
 * 
 * Since: 2.52
 */
WebKitWebExtensionManager* webkit_web_extension_manager_new()
{
    WebKitWebExtensionManager* object = WEBKIT_WEB_EXTENSION_MANAGER(g_object_new(WEBKIT_TYPE_WEB_EXTENSION_MANAGER, nullptr));
    Ref controller = WebKit::WebExtensionController::create(WebKit::WebExtensionControllerConfiguration::createDefault());

    object->priv->controller = WTFMove(controller);
    object->priv->controller->setWrapper(object);
    return object;
}

/**
 * webkit_web_extension_manager_new_with_identifier:
 * @identifier: A valid UUID
 *
 * Creates a new [class@WebExtensionManager] that is persistent and unique.
 * 
 * Data will be written to the file system in a unique location based on the specified identifier.
 * See also [ctor@WebExtensionManager.new].
 * 
 * Returns: (nullable): the newly created [class@WebExtensionManager], or %NULL if the identifier is not a valid UUID.
 * 
 * Since: 2.52
 */
WebKitWebExtensionManager* webkit_web_extension_manager_new_with_identifier(gchar* identifierUUID)
{
    g_return_val_if_fail(g_uuid_string_is_valid(identifierUUID), nullptr);

    WebKitWebExtensionManager* object = WEBKIT_WEB_EXTENSION_MANAGER(g_object_new(WEBKIT_TYPE_WEB_EXTENSION_MANAGER, nullptr));
    Ref controller = WebKit::WebExtensionController::create(WebKit::WebExtensionControllerConfiguration::create(WTF::UUID::parse(String::fromUTF8(identifierUUID)).value()));

    object->priv->controller = WTFMove(controller);
    object->priv->controller->setWrapper(object);
    return object;
}

/**
 * webkit_web_extension_manager_new_with_non_persistent_settings:
 *
 * Creates a new [class@WebExtensionManager] with non-persisent settings.
 * 
 * When not configured to be persistent, no data will be written to the file system. This is
 * useful for extensions in "private browsing" situations.
 * 
 * Returns: the newly created [class@WebExtensionManager].
 * 
 * Since: 2.52
 */
WebKitWebExtensionManager* webkit_web_extension_manager_new_with_non_persistent_settings()
{
    WebKitWebExtensionManager* object = WEBKIT_WEB_EXTENSION_MANAGER(g_object_new(WEBKIT_TYPE_WEB_EXTENSION_MANAGER, nullptr));
    Ref controller = WebKit::WebExtensionController::create(WebKit::WebExtensionControllerConfiguration::createNonPersistent());

    object->priv->controller = WTFMove(controller);
    object->priv->controller->setWrapper(object);
    return object;
}

/**
 * webkit_web_extension_manager_get_persistent:
 * @manager: A [class@WebExtensionManager]
 *
 * Gets whether this [class@WebExtensionManager] will write data to the file system.
 * 
 * If this is %TRUE, then any connected [class@WebExtensionContext] will write extension data to the file system,
 * such as extension storage and settings. If this is %FALSE, the data will not be saved to the file system,
 * and will only be stored in memory.
 * 
 * Returns: %TRUE if this [class@WebExtensionManager] will write data to the file system.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_manager_get_persistent(WebKitWebExtensionManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), false);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    return priv->controller->storageIsPersistent();
}

/**
 * webkit_web_extension_manager_get_identifier:
 * @manager: A [class@WebExtensionManager]
 *
 * Gets the unique identifier used for persistent storage.
 * 
 * When not configured to be persistent, or the identifier is not unique, this value will be %NULL.
 * See [method@WebExtensionManager.get_persistent] to check whether this [class@WebExtensionManager] is persistent.
 * 
 * Returns: (nullable): the unique identifier used for persistent storage, or %NULL if it is the default or not persistent.
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_manager_get_identifier(WebKitWebExtensionManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    if (!priv->identifier.isNull())
        return priv->identifier.data();

    auto identifier = priv->controller->protectedConfiguration()->identifier();
    if (!identifier)
        return nullptr;

    priv->identifier = identifier.value().toString().utf8();
    return priv->identifier.data();
}

/**
 * webkit_web_extension_manager_get_settings:
 * @manager: A [class@WebExtensionManager]
 *
 * Gets the [class@Settings] to be used as a basis for configuring web views in any connected contexts.
 * 
 * Returns: (transfer none): a [class@Settings] to be used for any [class@WebView] instances created by any connected contexts.
 * 
 * Since: 2.52
 */
WebKitSettings* webkit_web_extension_manager_get_settings(WebKitWebExtensionManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    return priv->controller->protectedConfiguration()->webViewConfiguration();
}

/**
 * webkit_web_extension_manager_set_settings:
 * @manager: A [class@WebExtensionManager]
 * @settings: (nullable): A [class@Settings] to set
 *
 * Sets the [class@Settings] to be used as a basis for configuring web views in any connected contexts.
 * 
 * When set to %NULL, any newly created [class@WebView] instances will use the default settings as set in [method@WebExtensionManager.Settings.new].
 * 
 * Since: 2.52
 */
void webkit_web_extension_manager_set_settings(WebKitWebExtensionManager* manager, WebKitSettings* settings)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager));
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    priv->controller->protectedConfiguration()->setWebViewConfiguration(settings);
}

/**
 * webkit_web_extension_manager_get_website_data_manager:
 * @manager: A [class@WebExtensionManager]
 *
 * Gets the [class@WebsiteDataManager] for website data and cookie access in connected extension contexts.
 * 
 * This sets the primary data manager for managing website data, including cookies, which extensions can access,
 * subject to the granted permissions within the extension contexts.
 * 
 * In addition to data stores created by this data manager, extensions can also access other data managers, such as non-persistent ones, for any open tabs.
 * 
 * Returns: (transfer none): a [class@WebsiteDataManager] used for any connected extensions
 * 
 * Since: 2.52
 */
WebKitWebsiteDataManager* webkit_web_extension_manager_get_website_data_manager(WebKitWebExtensionManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    if (priv->websiteDataManager)
        return priv->websiteDataManager.get();

    WebKitWebsiteDataManager* websiteDataManager = webkitWebsiteDataManagerCreate(priv->controller->protectedConfiguration()->protectedDefaultWebsiteDataStore());
    priv->websiteDataManager = adoptGRef(websiteDataManager);
    return websiteDataManager;
}

/**
 * webkit_web_extension_manager_set_website_data_manager:
 * @manager: A [class@WebExtensionManager]
 * @website_data_manager: (nullable) (transfer full): A [class@WebsiteDataManager]
 *
 * Sets the [class@WebsiteDataManager] for website data and cookie access in connected extension contexts.
 * 
 * When set to %NULL, any newly created [class@WebView] instances will use the default [class@WebsiteDataManager].
 * 
 * Since: 2.52
 */
void webkit_web_extension_manager_set_website_data_manager(WebKitWebExtensionManager* manager, WebKitWebsiteDataManager* websiteDataManager)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager));
    g_return_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(websiteDataManager));

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    priv->controller->protectedConfiguration()->setDefaultWebsiteDataStore(&webkitWebsiteDataManagerGetDataStore(websiteDataManager));
    priv->websiteDataManager = adoptGRef(websiteDataManager);
}

/**
 * webkit_web_extension_manager_get_extensions:
 * @manager: A [class@WebExtensionManager]
 *
 * Gets a list of [class@WebExtension] currently loaded by any connected [class@WebExtensionContext] instances.
 * To get the list of currently loaded extension contexts, see [method@WebExtensionManager.get_extension_contexts].
 * 
 * Returns: (element-type WebKitWebExtension) (transfer full): A [struct@GLib.List] of [class@WebExtension] instances
 * Since: 2.52
 */
GList* webkit_web_extension_manager_get_extensions(WebKitWebExtensionManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    GList* extensionList = 0;
    for (Ref extension : priv->controller->extensions())
        extensionList = g_list_prepend(extensionList, extension->wrapper());
    extensionList = g_list_reverse(extensionList);

    return extensionList;
}

/**
 * webkit_web_extension_manager_get_extension_contexts:
 * @manager: A [class@WebExtensionManager]
 *
 * Gets a list of [class@WebExtensionContext] currently loaded by this @manager.
 * To get the list of currently loaded extensions, see [method@WebExtensionManager.get_extensions].
 * 
 * Returns: (element-type WebKitWebExtensionContext) (transfer full): A [struct@GLib.List] of [class@WebExtensionContext] instances
 * Since: 2.52
 */
GList* webkit_web_extension_manager_get_extension_contexts(WebKitWebExtensionManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    GList* extensionList = 0;
    for (Ref context : priv->controller->extensionContexts())
        extensionList = g_list_prepend(extensionList, context->wrapper());
    extensionList = g_list_reverse(extensionList);

    return extensionList;
}

/**
 * webkit_web_extension_manager_load_extension_context:
 * @manager: A [class@WebExtensionManager]
 * @context: A [class@WebExtensionContext] to load
 * @error: return location for error or %NULL to ignore
 *
 * Loads the specified [class@WebExtensionContext]. This causes the context to start,
 * loading any background content, and injecting any content into relevant tabs.
 * 
 * Returns: %TRUE if the [class@WebExtensionContext] was loaded successfully or %FALSE in case of error.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_manager_load_extension_context(WebKitWebExtensionManager *manager, WebKitWebExtensionContext *context, GError **error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), false);
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), false);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    auto loadResult = priv->controller->load(*webkitWebExtensionContextGetInternalContext(context));
    if (!loadResult) {
        RefPtr internalError = loadResult.error();
        g_set_error(error, webkit_web_extension_context_error_quark(),
            toWebKitWebExtensionContextError(internalError->errorCode()), internalError->localizedDescription().utf8().data(), nullptr);
        return false;
    }

    return loadResult.value();
}

/**
 * webkit_web_extension_manager_unload_extension_context:
 * @manager: A [class@WebExtensionManager]
 * @context: A [class@WebExtensionContext] to unload
 * @error: return location for error or %NULL to ignore
 *
 * Unloads the specified [class@WebExtensionContext]. This causes the context to stop running.
 * 
 * Returns: %TRUE if the [class@WebExtensionContext] was unloaded successfully or %FALSE in case of error.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_manager_unload_extension_context(WebKitWebExtensionManager *manager, WebKitWebExtensionContext *context, GError **error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), false);
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), false);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    auto unloadResult = priv->controller->unload(*webkitWebExtensionContextGetInternalContext(context));
    if (!unloadResult) {
        RefPtr internalError = unloadResult.error();
        g_set_error(error, webkit_web_extension_context_error_quark(),
            toWebKitWebExtensionContextError(internalError->errorCode()), internalError->localizedDescription().utf8().data(), nullptr);
        return false;
    }

    return unloadResult.value();
}

/**
 * webkit_web_extension_manager_extension_context_for_extension:
 * @manager: A [class@WebExtensionManager]
 * @extension: A [class@WebExtension]
 *
 * Returns a loaded extension context for the specified extension.
 * 
 * Returns: (transfer none): a [class@WebExtensionContext] or %NULL if no match was found
 * 
 * Since: 2.52
 */
WebKitWebExtensionContext* webkit_web_extension_manager_extension_context_for_extension(WebKitWebExtensionManager *manager, WebKitWebExtension *extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    if (RefPtr extensionContext = priv->controller->extensionContext(*webkitWebExtensionToImpl(extension)))
        return extensionContext->wrapper();
    return nullptr;
}

/**
 * webkit_web_extension_manager_extension_context_for_URL:
 * @manager: A [class@WebExtensionManager]
 * @url: The URL to lookup.
 *
 * Returns a loaded extension context matching the specified URL.
 * 
 * This method is useful for determining the extension context to use when about to navigate to an extension URL. For example,
 * you could use this method to retrieve the appropriate extension context and then use its settings property to configure a
 * web view for loading that URL.
 * 
 * Returns: (transfer none): a [class@WebExtensionContext] or %NULL if no match was found
 * 
 * Since: 2.52
 */
WebKitWebExtensionContext* webkit_web_extension_manager_extension_context_for_URL(WebKitWebExtensionManager *manager, gchar *url)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    if (RefPtr extensionContext = priv->controller->extensionContext(URL { String::fromUTF8(url) }))
        return extensionContext->wrapper();
    return nullptr;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

WebKitWebExtensionManager* webkit_web_extension_manager_new()
{
    return nullptr;
}

WebKitWebExtensionManager* webkit_web_extension_manager_new_with_identifier(gchar* identifierUUID)
{
    return nullptr;
}

WebKitWebExtensionManager* webkit_web_extension_manager_new_with_non_persistent_settings()
{
    return nullptr;
}

gboolean webkit_web_extension_manager_get_persistent(WebKitWebExtensionManager* manager)
{
    return false;
}

const gchar* webkit_web_extension_manager_get_identifier(WebKitWebExtensionManager* manager)
{
    return ""
}

WebKitSettings* webkit_web_extension_manager_get_settings(WebKitWebExtensionManager* manager)
{
    return nullptr;
}

void webkit_web_extension_manager_set_settings(WebKitWebExtensionManager* manager, WebKitSettings* settings)
{
    return;
}

WebKitWebsiteDataManager* webkit_web_extension_manager_get_website_data_manager(WebKitWebExtensionManager* manager)
{
    return nullptr;
}

void webkit_web_extension_manager_set_website_data_manager(WebKitWebExtensionManager* manager, WebKitWebsiteDataManager* websiteDataManager)
{
    return;
}

GList* webkit_web_extension_manager_get_extensions(WebKitWebExtensionManager* manager)
{
    return nullptr;
}

GList* webkit_web_extension_manager_get_extension_contexts(WebKitWebExtensionManager* manager)
{
    return nullptr;
}

gboolean webkit_web_extension_manager_load_extension_context(WebKitWebExtensionManager *manager, WebKitWebExtensionContext *context, GError **error)
{
    return false;
}

gboolean webkit_web_extension_manager_unload_extension_context(WebKitWebExtensionManager *manager, WebKitWebExtensionContext *context, GError **error)
{
    return false;
}

WebKitWebExtensionContext* webkit_web_extension_manager_extension_context_for_extension(WebKitWebExtensionManager *manager, WebKitWebExtension *extension)
{
    return nullptr;
}

WebKitWebExtensionContext* webkit_web_extension_manager_extension_context_for_URL(WebKitWebExtensionManager *manager, gchar *url)
{
    return nullptr;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

#endif // ENABLE(2022_GLIB_API)
