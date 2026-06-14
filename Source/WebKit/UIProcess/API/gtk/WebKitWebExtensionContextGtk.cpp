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
#include "WebKitWebExtensionContext.h"

#if ENABLE(2022_GLIB_API)

#if ENABLE(WK_WEB_EXTENSIONS)

/**
 * webkit_web_extension_context_new_for_extension:
 * @extension: (transfer none): a [class@WebExtension]
 * @error: return location for error or %NULL to ignore
 *
 * Create a new Context for the provided [class@WebExtension].
 * 
 * Returns: the newly created context
 * 
 * Since: 2.52
 */
WebKitWebExtensionContext* webkit_web_extension_context_new_for_extension(WebKitWebExtension* extension, GError** error)
{
    if (auto object = g_initable_new (WEBKIT_TYPE_WEB_EXTENSION_CONTEXT, nullptr, error, "web-extension", extension, nullptr))
        return WEBKIT_WEB_EXTENSION_CONTEXT (object);
    return nullptr;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

WebKitWebExtensionContext* webkit_web_extension_context_new_for_extension(WebKitWebExtension* extension, GError** error)
{
    return nullptr;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

#endif // ENABLE(2022_GLIB_API)
