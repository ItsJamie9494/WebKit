/*
 * Copyright (C) 2024 Igalia, S.L. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "TestMain.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>
#include <WebKitWebExtensionInternal.h>

static void testDisplayStringParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return webkitWebExtensionCreate({{ "manifest.json"_s, String::fromLatin1(manifestString) }}, &error.outPtr());
    };

    auto extension = parse("{ \"manifest_version\": 2 }");

    g_assert_null(webkit_web_extension_get_display_name(extension));
    g_assert_null(webkit_web_extension_get_display_short_name(extension));
    g_assert_null(webkit_web_extension_get_display_version(extension));
    g_assert_null(webkit_web_extension_get_display_description(extension));
    g_assert_null(webkit_web_extension_get_version(extension));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");

    g_assert_cmpstr(webkit_web_extension_get_display_name(extension), ==, "Test");
    g_assert_cmpstr(webkit_web_extension_get_display_short_name(extension), ==,  "Test");
    g_assert_cmpstr(webkit_web_extension_get_display_version(extension), ==,  "1.0");
    g_assert_cmpstr(webkit_web_extension_get_display_description(extension), ==,  "Test description");
    g_assert_cmpstr(webkit_web_extension_get_version(extension), ==,  "1.0");
    g_assert_cmpint(webkit_web_extension_get_manifest_version(extension), ==, 2);
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 3, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");

    g_assert_cmpstr(webkit_web_extension_get_display_name(extension), ==, "Test");
    g_assert_cmpstr(webkit_web_extension_get_display_short_name(extension), ==,  "Test");
    g_assert_cmpstr(webkit_web_extension_get_display_version(extension), ==,  "1.0");
    g_assert_cmpstr(webkit_web_extension_get_display_description(extension), ==,  "Test description");
    g_assert_cmpstr(webkit_web_extension_get_version(extension), ==,  "1.0");
    g_assert_cmpint(webkit_web_extension_get_manifest_version(extension), ==, 3);
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"name\": \"Test\", \"short_name\": \"Tst\", \"version\": \"1.0\", \"version_name\": \"1.0 Final\", \"description\": \"Test description\" }");

    g_assert_cmpstr(webkit_web_extension_get_display_name(extension), ==, "Test");
    g_assert_cmpstr(webkit_web_extension_get_display_short_name(extension), ==,  "Tst");
    g_assert_cmpstr(webkit_web_extension_get_display_version(extension), ==,  "1.0 Final");
    g_assert_cmpstr(webkit_web_extension_get_display_description(extension), ==,  "Test description");
    g_assert_cmpstr(webkit_web_extension_get_version(extension), ==,  "1.0");
    g_assert_no_error(error.get());
}

static void testDefaultLocaleParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString, const gchar* localeFile) {
        return webkitWebExtensionCreate({{ "manifest.json"_s, String::fromLatin1(manifestString) }, { String::fromLatin1(localeFile), "{}"_s }}, &error.outPtr());
    };

    // Test no default locale
    auto extension = webkitWebExtensionCreate({{ "manifest.json"_s, String::fromLatin1("{ \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }") }}, &error.outPtr());
    g_assert_no_error(error.get());
    g_assert_null(webkit_web_extension_get_default_locale(extension));

    // Test with language locale file existing
    extension = parse("{ \"manifest_version\": 2, \"default_locale\": \"en\", \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", "_locales/en/messages.json");
    auto *defaultLocale = webkit_web_extension_get_default_locale(extension);
    g_assert_no_error(error.get());
    g_assert_cmpstr(webkit_web_extension_get_default_locale(extension), ==, "en");

    // Test with language locale file existing
    extension = parse("{ \"manifest_version\": 2, \"default_locale\": \"en_US\", \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", "_locales/en_US/messages.json");
    defaultLocale = webkit_web_extension_get_default_locale(extension);
    g_assert_no_error(error.get());
    g_assert_cmpstr(webkit_web_extension_get_default_locale(extension), ==, "en_US");

    // Test with less specific locale file existing
    extension = parse("{ \"manifest_version\": 2, \"default_locale\": \"en_US\", \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", "_locales/en/messages.json");
    defaultLocale = webkit_web_extension_get_default_locale(extension);
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_null(defaultLocale);

    // Test with wrong locale file existing
    extension = parse("{ \"manifest_version\": 2, \"default_locale\": \"en_US\", \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", "_locales/zh_CN/messages.json");
    defaultLocale = webkit_web_extension_get_default_locale(extension);
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_null(defaultLocale);

    // Test with no locale file existing
    extension = parse("{ \"manifest_version\": 2, \"default_locale\": \"en_US\", \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", "");
    defaultLocale = webkit_web_extension_get_default_locale(extension);
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_null(defaultLocale);
}

static void testDisplayStringParsingWithLocalization(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;

    const gchar* manifest =
        "{"
        "\"manifest_version\": 2,"
        "\"default_locale\": \"en_US\","
        "\"name\": \"__MSG_default_name__\","
        "\"short_name\": \"__MSG_regional_name__\","
        "\"version\": \"1.0\","
        "\"description\": \"__MSG_default_description__\""
        "}";
    
    const gchar* defaultMessages =
        "{"
        "\"default_name\": {"
        "\"message\": \"Default String\","
        "\"description\": \"The test name.\""
        "},"
        "\"default_description\": {"
        "\"message\": \"Default Description\","
        "\"description\": \"The test description.\""
        "}"
        "}";
    
    const gchar* regionalMessages =
        "{"
        "\"regional_name\": {"
        "\"message\": \"Regional String\","
        "\"description\": \"The regional name.\""
        "}"
        "}";
    
    HashMap<String, String> resources = {
        { "manifest.json"_s, String::fromLatin1(manifest)},
        { "_locales/en/messages.json"_s, String::fromLatin1(defaultMessages) },
        { "_locales/en_US/messages.json"_s, String::fromLatin1(regionalMessages) }
    };

    auto extension = webkitWebExtensionCreate(resources, &error.outPtr());

    g_assert_cmpstr(webkit_web_extension_get_display_name(extension), ==, "Default String");
    g_assert_cmpstr(webkit_web_extension_get_display_short_name(extension), ==,  "Regional String");
    g_assert_cmpstr(webkit_web_extension_get_display_version(extension), ==,  "1.0");
    g_assert_cmpstr(webkit_web_extension_get_display_description(extension), ==,  "Default Description");
    g_assert_cmpstr(webkit_web_extension_get_version(extension), ==,  "1.0");
    g_assert_no_error(error.get());

    manifest =
        "{"
        "\"manifest_version\": 2,"
        "\"default_locale\": \"en_US\","
        "\"name\": \"__MSG_default_name__\","
        "\"short_name\": \"__MSG_default_name__\","
        "\"version\": \"1.0\","
        "\"description\": \"__MSG_default_description__\""
        "}";
    resources = {
        { "manifest.json"_s, String::fromLatin1(manifest)},
        { "_locales/en/messages.json"_s, String::fromLatin1(defaultMessages) },
        { "_locales/en_US/messages.json"_s, String::fromLatin1(regionalMessages) }
    };

    extension = webkitWebExtensionCreate(resources, &error.outPtr());

    g_assert_cmpstr(webkit_web_extension_get_display_short_name(extension), ==,  "Default String");
    g_assert_no_error(error.get());
}

static void testContentScriptsParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        StringBuilder builder;
        builder.append("{"_s);
        builder.append("\"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\","_s);
        builder.append(ASCIILiteral::fromLiteralUnsafe(manifestString));
        builder.append("}"_s);
        return webkitWebExtensionCreate({{ "manifest.json"_s, builder.toString() }}, &error.outPtr());
    };

    auto *extension = parse("\"content_scripts\": [{ \"js\": [\"\test.js\", 1, \"\"], \"css\": [false, \"test.css\", \"\"], \"matches\": [\"*://*/\"] }]");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_injected_content(extension));

    extension = parse("\"content_scripts\": [{ \"js\": [\"\test.js\", 1, \"\"], \"css\": [false, \"test.css\", \"\"], \"matches\": [\"*://*/\"], \"exclude_matches\": [\"*://*.example.com/\"] }]");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_injected_content(extension));

    extension = parse("\"content_scripts\": [{ \"js\": [\"\test.js\", 1, \"\"], \"css\": [false, \"test.css\", \"\"], \"matches\": [\"*://*.example.com/\"] }]");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_injected_content(extension));

    extension = parse("\"content_scripts\": [{ \"js\": [\"\test.js\"], \"matches\": [\"*://*.example.com/\"], \"world\": \"MAIN\" }]");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_injected_content(extension));

    extension = parse("\"content_scripts\": [{ \"css\": [false, \"test.css\", \"\"], \"matches\": [\"*://*.example.com/\"], \"css_origin\": \"user\" }]");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_injected_content(extension));

    extension = parse("\"content_scripts\": [{ \"css\": [false, \"test.css\", \"\"], \"matches\": [\"*://*.example.com/\"], \"css_origin\": \"author\" }]");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_injected_content(extension));

    // Invalid cases

    extension = parse("\"content_scripts\": []");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_injected_content(extension));

    extension = parse("\"content_scripts\": { \"invalid\": true }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_injected_content(extension));

    extension = parse("\"content_scripts\": [{ \"js\": [ \"test.js\" ], \"matches\": [] }]");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_injected_content(extension));

    // Non-critical invalid cases

    extension = parse("\"content_scripts\": [{ \"js\": [ \"test.js\" ], \"matches\": [\"*://*.example.com/\"], \"run_at\": \"invalid\" }]");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_true(webkit_web_extension_get_has_injected_content(extension));

    extension = parse("\"content_scripts\": [{ \"js\": [ \"test.js\" ], \"matches\": [\"*://*.example.com/\"], \"world\": \"INVALID\" }]");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_true(webkit_web_extension_get_has_injected_content(extension));

    extension = parse("\"content_scripts\": [{ \"css\": [false, \"test.css\", \"\"], \"matches\": [\"*://*.example.com/\"], \"css_origin\": \"bad\" }]");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_true(webkit_web_extension_get_has_injected_content(extension));
}

static void testPermissionsParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return webkitWebExtensionCreate({{ "manifest.json"_s, String::fromLatin1(manifestString) }}, &error.outPtr());
    };

    // Failure Testing

    // Neither of the "permissions" and "optional_permissions" keys are defined.

    auto *extension = parse("{ \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    const gchar* const* requestedPermissions = webkit_web_extension_get_requested_permissions(extension);
    const gchar* const* optionalPermissions = webkit_web_extension_get_optional_permissions(extension);
    g_assert_null(requestedPermissions);
    g_assert_null(optionalPermissions);

    // The "permissions" key alone is defined and empty

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension);
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension);
    g_assert_null(requestedPermissions);
    g_assert_null(optionalPermissions);

    // The "optional_permissions" key alone is defined and empty

    extension = parse("{ \"manifest_version\": 2, \"optional_permissions\": [], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension);
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension);
    g_assert_null(requestedPermissions);
    g_assert_null(optionalPermissions);

    // The "permissions" and "optional_permissions" keys are defined as invalid types

    extension = parse("{ \"manifest_version\": 2, \"permissions\": 2, \"optional_permissions\": \"foo\", \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension);
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension);
    g_assert_null(requestedPermissions);
    g_assert_null(optionalPermissions);

    // The "permissions" keys is defined with an invalid permission.

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [ \"invalid\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension);
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension);
    g_assert_null(requestedPermissions);
    g_assert_null(optionalPermissions);

    // Pass Testing

    // The "permissions" key is defined with a valid permission

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [ \"tabs\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension);
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension);
    g_assert_nonnull(requestedPermissions);
    g_assert_null(optionalPermissions);
    g_assert_cmpstr(requestedPermissions[0], ==, "tabs");
    g_assert_null(requestedPermissions[1]);

    // The "permissions" key is defined with a valid  & invalid permission

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [ \"tabs\", \"invalid\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension);
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension);
    g_assert_nonnull(requestedPermissions);
    g_assert_null(optionalPermissions);
    g_assert_cmpstr(requestedPermissions[0], ==, "tabs");
    g_assert_null(requestedPermissions[1]);

    // The "optional_permissions" keys is defined with an invalid permission.

    extension = parse("{ \"manifest_version\": 2, \"optional_permissions\": [ \"invalid\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension);
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension);
    g_assert_null(optionalPermissions);
    g_assert_null(requestedPermissions);

    // The "optional_permissions" key is defined with a valid permission

    extension = parse("{ \"manifest_version\": 2, \"optional_permissions\": [ \"tabs\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension);
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension);
    g_assert_nonnull(optionalPermissions);
    g_assert_null(requestedPermissions);
    g_assert_cmpstr(optionalPermissions[0], ==, "tabs");
    g_assert_null(optionalPermissions[1]);

    // The "optional_permissions" key is defined with a valid  & invalid permission

    extension = parse("{ \"manifest_version\": 2, \"optional_permissions\": [ \"tabs\", \"invalid\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension);
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension);
    g_assert_nonnull(optionalPermissions);
    g_assert_null(requestedPermissions);
    g_assert_cmpstr(optionalPermissions[0], ==, "tabs");
    g_assert_null(optionalPermissions[1]);

    // The "optional_permissions" key is defined with a valid & forbidden invalid permission

    extension = parse("{ \"manifest_version\": 2, \"optional_permissions\": [ \"tabs\", \"geolocation\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension);
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension);
    g_assert_nonnull(optionalPermissions);
    g_assert_null(requestedPermissions);
    g_assert_cmpstr(optionalPermissions[0], ==, "tabs");
    g_assert_null(optionalPermissions[1]);

    // The "optional_permissions" key is defined in "permissions"

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [ \"tabs\", \"geolocation\" ], \"optional_permissions\": [ \"tabs\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension);
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension);
    g_assert_nonnull(requestedPermissions);
    g_assert_null(optionalPermissions);
    g_assert_cmpstr(requestedPermissions[0], ==, "tabs");
    g_assert_null(requestedPermissions[1]);
}

static void testBackgroundParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return webkitWebExtensionCreate({{ "manifest.json"_s, String::fromLatin1(manifestString) }}, &error.outPtr());
    };

    auto *extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [ \"test.js\" ] }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension));
    g_assert_true(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_no_error(error.get());
    // FIXME: Cococa tests modual and service worker here, should we include those?

    extension = parse("{\"manifest_version\":2,\"background\":{\"page\":\"test.html\",\"persistent\":false},\"name\":\"Test\",\"version\":\"1.0\",\"description\":\"Test\"}");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [ \"test-1.js\", \"\", \"test-2.js\" ], \"persistent\": true }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension));
    g_assert_true(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"service_worker\": \"test.js\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [ \"test-1.js\", \"test-2.js\" ], \"service_worker\": \"test.js\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"page\": \"test.html\", \"service_worker\": \"test.js\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [ \"test-1.js\", \"test-2.js\" ], \"page\": \"test.html\", \"service_worker\": \"test.js\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"service_worker\": \"test.js\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"service_worker\": \"test.js\", \"type\": \"module\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [ \"test-1.js\", \"test-2.js\" ], \"type\": \"module\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_no_error(error.get());

    // Invalid cases

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"page\": \"test.html\", \"persistent\": true }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_BACKGROUND_PERSISTENCE);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"service_worker\": \"test.js\", \"persistent\": true }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_BACKGROUND_PERSISTENCE);

    extension = parse("{ \"manifest_version\": 2, \"background\": { }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": [ \"invalid\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [], \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"page\": \"\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"page\": [ \"test.html\" ], \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [ [ \"test.js\" ] ], \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"service_worker\": \"\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"service_worker\": [ \"test.js\" ], \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
}

static void testOptionsPageParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return webkitWebExtensionCreate({{ "manifest.json"_s, String::fromLatin1(manifestString) }}, &error.outPtr());
    };

    auto *extension = parse("{ \"manifest_version\": 3, \"options_page\": \"options.html\", \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_options_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"options_page\": \"\", \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_options_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"options_page\": 123, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_options_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"options_ui\": { \"page\": \"options.html\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_options_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"options_ui\": { \"bad\": \"options.html\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_options_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"options_ui\": { \"page\": 123 }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_options_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"options_ui\": { }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_options_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"options_ui\": { \"page\": \"\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_options_page(extension));
}

static void testURLOverridesParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return webkitWebExtensionCreate({{ "manifest.json"_s, String::fromLatin1(manifestString) }}, &error.outPtr());
    };

    auto *extension = parse("{ \"manifest_version\": 3, \"browser_url_overrides\": { \"newtab\": \"newtab.html\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_override_new_tab_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"browser_url_overrides\": { \"bad\": \"newtab.html\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"browser_url_overrides\": { \"newtab\": 123 }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"browser_url_overrides\": { }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"browser_url_overrides\": { \"newtab\": \"\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"chrome_url_overrides\": { \"newtab\": \"newtab.html\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_override_new_tab_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"chrome_url_overrides\": { \"bad\": \"newtab.html\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"chrome_url_overrides\": { \"newtab\": 123 }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"chrome_url_overrides\": { }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension));

    extension = parse("{ \"manifest_version\": 3, \"chrome_url_overrides\": { \"newtab\": \"\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension));
}

static void testContentSecurityPolicyParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return webkitWebExtensionCreate({{ "manifest.json"_s, String::fromLatin1(manifestString) }}, &error.outPtr());
    };

    // Manifest V3
    parse("{ \"manifest_version\": 3, \"content_security_policy\": { \"extension_pages\": \"script-src 'self'; object-src 'self'\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());

    parse("{ \"manifest_version\": 3, \"content_security_policy\": { \"sandbox\": \"sandbox allow-scripts allow-forms allow-popups allow-modals; script-src 'self'\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());

    parse("{ \"manifest_version\": 3, \"content_security_policy\": { }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    parse("{ \"manifest_version\": 3, \"content_security_policy\": { \"extension_pages\": 123 }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    parse("{ \"manifest_version\": 3, \"content_security_policy\": \"script-src 'self'; object-src 'self'\", \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    // Manifest V2
    parse("{ \"manifest_version\": 2, \"content_security_policy\": \"script-src 'self'; object-src 'self'\", \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());

    parse("{ \"manifest_version\": 2, \"content_security_policy\": [ \"invalid\", \"type\" ], \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    parse("{ \"manifest_version\": 2, \"content_security_policy\": 123, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    parse("{ \"manifest_version\": 2, \"content_security_policy\": { \"extension_pages\": \"script-src 'self'; object-src 'self'\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
}

static void testWebAccessibleResourcesV2(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return webkitWebExtensionCreate({{ "manifest.json"_s, String::fromLatin1(manifestString) }}, &error.outPtr());
    };

    parse("{ \"manifest_version\": 2, \"web_accessible_resources\": [ \"images/*.png\", \"styles/*.css\" ], \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());

    parse("{ \"manifest_version\": 2, \"web_accessible_resources\": [ ], \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());

    parse("{ \"manifest_version\": 2, \"web_accessible_resources\": \"bad\", \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    parse("{ \"manifest_version\": 2, \"web_accessible_resources\": { }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
}

void beforeAll()
{
    Test::add("WebKitWebExtension", "display-string-parsing", testDisplayStringParsing);
    Test::add("WebKitWebExtension", "default-locale-parsing", testDefaultLocaleParsing);
    // Test::add("WebKitWebExtension", "display-string-parsing-with-localization", testDisplayStringParsingWithLocalization);
    Test::add("WebKitWebExtension", "content-scripts-parsing", testContentScriptsParsing);
    Test::add("WebKitWebExtension", "permissions-parsing", testPermissionsParsing);
    Test::add("WebKitWebExtension", "background-parsing", testBackgroundParsing);
    Test::add("WebKitWebExtension", "options-page-parsing", testOptionsPageParsing);
    Test::add("WebKitWebExtension", "url-overrides-parsing", testURLOverridesParsing);
    Test::add("WebKitWebExtension", "content-security-policy-parsing", testContentSecurityPolicyParsing);
    Test::add("WebKitWebExtension", "web-accessible-resources-v2", testWebAccessibleResourcesV2);
}

void afterAll()
{
    
}

#endif // ENABLE(WK_WEB_EXTENSIONS)
