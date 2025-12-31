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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebKitWebExtensionManager.h"
#include <WebKit/WKBase.h>

// Private API required by the unit tests

typedef struct _WebKitWebExtensionManager WebKitWebExtensionManager;

WK_EXPORT WebKitWebExtensionManager *webkitWebExtensionManagerNewWithTemporaryConfiguration(void);
WK_EXPORT gboolean webkitWebExtensionManagerGetIsTemporary(WebKitWebExtensionManager*);
WK_EXPORT const gchar *webkitWebExtensionManagerGetStorageDirectoryPath(WebKitWebExtensionManager*);

WK_EXPORT void webkitWebExtensionManagerSetTestingMode(WebKitWebExtensionManager*, bool);
WK_EXPORT void webkitWebExtensionManagerSetPrivateDelegate(WebKitWebExtensionManager*, WebExtensionManagerDelegate*);
WK_EXPORT WebExtensionManagerDelegate* webkitWebExtensionManagerGetPrivateDelegate(WebKitWebExtensionManager*);

#endif // ENABLE(WK_WEB_EXTENSIONS)
