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
#include "WebExtensionAPITest.h"

#include "WebExtensionControllerMessages.h"
#include "WebExtensionControllerProxy.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <jsc/JSCContextPrivate.h>
#include <jsc/JSCValuePrivate.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

JSValueRef WebExtensionAPITest::assertRejects(JSContextRef context, JSValueRef promiseRef, JSValueRef expectedError, const String& message)
{
    GRefPtr<JSCContext> globalContext = jscContextGetOrCreate(JSContextGetGlobalContext(context));

    GRefPtr<JSCValue> resolveCallback;
    GRefPtr<JSCValue> rejectCallback;

    auto resultPromiseHandler = Function<void(JSCValue*, JSCValue*)> { [&resolveCallback, &rejectCallback](JSCValue* resolve, JSCValue* reject) {
        resolveCallback = GRefPtr(resolve);
        rejectCallback = GRefPtr(reject);
    } };

    GRefPtr<JSCValue> resultPromise = jsc_value_new_promise(globalContext.get(), +[](JSCValue *result, JSCValue* reject, gpointer userData) {
        auto& handler = *reinterpret_cast<Function<void(JSCValue*, JSCValue*)>*>(userData);

        handler(result, reject);
    }, &resultPromiseHandler);

    // Wrap in a native promise for consistency.
    GRefPtr<JSCValue> promise = jsc_value_new_promise(globalContext.get(), +[](JSCValue* resolve, JSCValue*, gpointer userData) {
        g_object_unref(jsc_value_function_call(resolve, JSC_TYPE_VALUE, static_cast<JSCValue*>(userData), G_TYPE_NONE));
    }, jscValueCreate(globalContext.get(), promiseRef));

    if (!isThenable(context, jscValueGetJSValue(promise.get())))
        return JSValueMakeNull(context);

    struct CallbackData {
        Ref<WebExtensionAPITest> self;
        GRefPtr<JSCContext> context;
        GRefPtr<JSCValue> resolveCallback;
        GRefPtr<JSCValue> rejectCallback;
        JSValueRef expectedError;
        String message;
    };

    CallbackData callbackData = { .self = Ref { *this }, .context = globalContext, .resolveCallback = resolveCallback, .rejectCallback = rejectCallback, .expectedError = expectedError, .message = message };
    GRefPtr<JSCValue> resultFunction = adoptGRef(jsc_value_new_function (globalContext.get(), "result", G_CALLBACK(+[](JSCValue *result, gpointer userData) {
        auto& data = *reinterpret_cast<CallbackData*>(userData);
        auto context = data.context.get();

        String falseException = nullString();
        data.self->assertEquals(jscContextGetJSContext(context), false, data.expectedError ? data.self->debugString(jscContextGetJSContext(context), data.expectedError) : "(any error)"_s, result ? data.self->debugString(jscContextGetJSContext(context), jscValueGetJSValue(result)) : "(no error)"_s, data.self->combineMessages(data.message, "Promise did not reject with an error"_s), falseException);
        callObjectWithArguments<0>(jscValueGetJSValue(data.rejectCallback.get()), jscContextGetJSContext(context), { });
    }), &callbackData, nullptr, G_TYPE_NONE, 1, JSC_TYPE_VALUE));
    GRefPtr<JSCValue> errorFunction = adoptGRef(jsc_value_new_function (globalContext.get(), "error", G_CALLBACK(+[](JSCValue *error, gpointer userData) {
        auto& data = *reinterpret_cast<CallbackData*>(userData);
        auto context = data.context.get();

        if (!error) {
            String falseException = nullString();
            data.self->assertEquals(jscContextGetJSContext(context), false, data.expectedError ? data.self->debugString(jscContextGetJSContext(context), data.expectedError) : "(any error)"_s, "(no error)"_s, data.self->combineMessages(data.message, "Promise did not reject with an error"_s), falseException);
            callObjectWithArguments<0>(jscValueGetJSValue(data.rejectCallback.get()), jscContextGetJSContext(context), { });
            return;
        }

        JSCValue* errorMessageValue = jsc_value_is_object(error) && jsc_value_object_has_property(error, "message") ? jsc_value_object_get_property(error, "message") : error;

        // By default, JSValueRef is set to an undefined value in the JS implementation.
        // It should not be possible to get a null value here.
        if (JSValueIsUndefined(jscContextGetJSContext(context), data.expectedError) || !data.expectedError) {
            String falseException = nullString();
            data.self->assertEquals(jscContextGetJSContext(context), true, "(any error)"_s, data.self->debugString(jscContextGetJSContext(context), jscValueGetJSValue(errorMessageValue)), data.self->combineMessages(data.message, "Promise rejected with an error"_s), falseException);
            callObjectWithArguments<0>(jscValueGetJSValue(data.resolveCallback.get()), jscContextGetJSContext(context), { });
            return;
        }

        if (isRegularExpression(jscContextGetJSContext(context), data.expectedError)) {
            String falseException = nullString();
            JSValueRef testResult = data.self->invokeMethod<1>(jscContextGetJSContext(context), data.expectedError, "test"_s, { jscValueGetJSValue(errorMessageValue) });
            data.self->assertEquals(jscContextGetJSContext(context), JSValueToBoolean(jscContextGetJSContext(context), testResult), data.self->debugString(jscContextGetJSContext(context), data.expectedError), data.self->debugString(jscContextGetJSContext(context), jscValueGetJSValue(errorMessageValue)), data.self->combineMessages(data.message, "Promise rejected with an error that didn't match the regular expression"_s), falseException);
            callObjectWithArguments<0>(jscValueGetJSValue(data.resolveCallback.get()), jscContextGetJSContext(context), { });
            return;
        }

        String falseException = nullString();
        data.self->assertEquals(jscContextGetJSContext(context), JSValueIsEqual(jscContextGetJSContext(context), data.expectedError, jscValueGetJSValue(errorMessageValue), nullptr), data.self->debugString(jscContextGetJSContext(context), data.expectedError), data.self->debugString(jscContextGetJSContext(context), jscValueGetJSValue(errorMessageValue)), data.self->combineMessages(data.message, "Promise rejected with an error that didn't equal"_s), falseException);
        callObjectWithArguments<0>(jscValueGetJSValue(data.resolveCallback.get()), jscContextGetJSContext(context), { });
    }), &callbackData, nullptr, G_TYPE_NONE, 1, JSC_TYPE_VALUE));

    g_object_unref(jsc_value_object_invoke_method(promise.get(), "then",
        JSC_TYPE_VALUE,
        resultFunction.get(),

        JSC_TYPE_VALUE,
        errorFunction.get(),

        G_TYPE_NONE
    ));

    return jscValueGetJSValue(resultPromise.get());
}

JSValueRef WebExtensionAPITest::assertResolves(JSContextRef context, JSValueRef promiseRef, const String& message)
{
    GRefPtr<JSCContext> globalContext = jscContextGetOrCreate(JSContextGetGlobalContext(context));

    GRefPtr<JSCValue> resolveCallback;

    auto resultPromiseHandler = Function<void(JSCValue*)> { [&resolveCallback](JSCValue* resolve) {
        resolveCallback = GRefPtr(resolve);
    } };

    GRefPtr<JSCValue> resultPromise = jsc_value_new_promise(globalContext.get(), +[](JSCValue *result, JSCValue*, gpointer userData) {
        auto& handler = *reinterpret_cast<Function<void(JSCValue*)>*>(userData);

        handler(result);
    }, &resultPromiseHandler);

    // Wrap in a native promise for consistency.
    GRefPtr<JSCValue> promise = jsc_value_new_promise(globalContext.get(), +[](JSCValue* resolve, JSCValue*, gpointer userData) {
        g_object_unref(jsc_value_function_call(resolve, JSC_TYPE_VALUE, static_cast<JSCValue*>(userData), G_TYPE_NONE));
    }, jscValueCreate(globalContext.get(), promiseRef));

    if (!isThenable(context, promiseRef))
        return JSValueMakeNull(context);

    struct ResultCallbackData {
        Ref<WebExtensionAPITest> self;
        GRefPtr<JSCContext> context;
        GRefPtr<JSCValue> resolveCallback;
        String message;
    };

    ResultCallbackData resultData = { .self = Ref { *this }, .context = globalContext, .resolveCallback = resolveCallback, .message = message };
    GRefPtr<JSCValue> resultFunction = adoptGRef(jsc_value_new_function (globalContext.get(), "result", G_CALLBACK(+[](JSCValue *result, gpointer userData) {
        auto& data = *reinterpret_cast<ResultCallbackData*>(userData);
        GRefPtr<JSCContext> context = data.context;

        data.self->succeed(jscContextGetJSContext(context.get()), "Promise resolved without an error"_s);
        callObjectWithArguments<1>(jscValueGetJSValue(data.resolveCallback.get()), jscContextGetJSContext(context.get()), { jscValueGetJSValue(result) });
    }), &resultData, nullptr, G_TYPE_NONE, 1, JSC_TYPE_VALUE));
    GRefPtr<JSCValue> errorFunction = adoptGRef(jsc_value_new_function (globalContext.get(), "error", G_CALLBACK(+[](JSCValue *error, gpointer userData) {
        auto& data = *reinterpret_cast<ResultCallbackData*>(userData);
        auto context = data.context.get();

        JSCValue* errorMessageValue = jsc_value_is_object(error) && jsc_value_object_has_property(error, "message") ? jsc_value_object_get_property(error, "message") : error;
        data.self->fail(jscContextGetJSContext(context), data.self->combineMessages(data.message, makeString("Promise rejected with an error: "_s, data.self->debugString(jscContextGetJSContext(context), jscValueGetJSValue(errorMessageValue)))));
        callObjectWithArguments<0>(jscValueGetJSValue(data.resolveCallback.get()), jscContextGetJSContext(context), { });
    }), &resultData, nullptr, G_TYPE_NONE, 1, JSC_TYPE_VALUE));

    g_object_unref(jsc_value_object_invoke_method(promise.get(), "then",
        JSC_TYPE_VALUE,
        resultFunction.get(),

        JSC_TYPE_VALUE,
        errorFunction.get(),

        G_TYPE_NONE
    ));

    return jscValueGetJSValue(resultPromise.get());
}

static JSValueRef createValueWithNewPromiseRejected(JSContextRef context, String reason)
{
    GRefPtr<JSCContext> globalContext = jscContextGetOrCreate(JSContextGetGlobalContext(context));

    GRefPtr<JSCValue> promise = jsc_value_new_promise(globalContext.get(), +[](JSCValue*, JSCValue* reject, gpointer userData) {
        auto reason = static_cast<const char*>(userData);
        g_object_unref(jsc_value_function_call(reject, JSC_TYPE_VALUE, jsc_value_new_string(jsc_value_get_context(reject), reason), G_TYPE_NONE));
    }, g_strdup(reason.utf8().data()));

    auto result = jscValueGetJSValue(promise.get());
    JSValueProtect(context, result);
    return result;
}

JSValueRef WebExtensionAPITest::addTest(JSContextRef context, JSValueRef testFunctionRef, String callingAPIName)
{
    if (!JSValueIsObject(context, testFunctionRef))
        return createValueWithNewPromiseRejected(context, toErrorString(callingAPIName, nullString(), "Error creating a new test."_s));

    JSObjectRef testObject = JSValueToObject(context, testFunctionRef, nullptr);
    if (!testObject)
        return nullptr;
    JSValueRef testName = JSObjectGetProperty(context, testObject, toJSString("name"_s).get(), nullptr);
    if (!testName || toString(context, testName).isEmpty())
        return createValueWithNewPromiseRejected(context, toErrorString(callingAPIName, nullString(), "The supplied test function must be named."_s));

    RefPtr page = toWebPage(context);
    if (!page)
        return createValueWithNewPromiseRejected(context, toErrorString(callingAPIName, nullString(), "Error creating a new test."_s));
    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return createValueWithNewPromiseRejected(context, toErrorString(callingAPIName, nullString(), "Error creating a new test."_s));

    GRefPtr<JSCValue> resolveCallback;
    GRefPtr<JSCValue> rejectCallback;

    auto resultPromiseHandler = Function<void(JSCValue*, JSCValue*)> { [&resolveCallback, &rejectCallback](JSCValue* resolve, JSCValue* reject) {
        resolveCallback = GRefPtr(resolve);
        rejectCallback = GRefPtr(reject);
    } };

    GRefPtr<JSCContext> globalContext = jscContextGetOrCreate(JSContextGetGlobalContext(context));

    GRefPtr<JSCValue> resultPromise = jsc_value_new_promise(globalContext.get(), [](JSCValue *result, JSCValue* reject, gpointer userData) {
        auto& handler = *reinterpret_cast<Function<void(JSCValue*, JSCValue*)>*>(userData);
        handler(result, reject);
    }, &resultPromiseHandler);

    auto location = scriptLocation(context);
    auto webExtensionControllerIdentifier = webExtensionControllerProxy->identifier();

    m_testQueue.append({
        toString(context, testName),
        location,
        webExtensionControllerIdentifier,
        testFunctionRef,
        jscValueGetJSValue(resolveCallback.get()),
        jscValueGetJSValue(rejectCallback.get()),
        context
    });

    WebProcess::singleton().send(Messages::WebExtensionController::TestAdded(toString(context, testName), location.first, location.second), webExtensionControllerIdentifier);

    if (!m_runningTest) {
        m_runningTest = true;

        WorkQueue::mainSingleton().dispatch([this, protectedThis = Ref { *this }] {
            startNextTest();
        });
    }

    return jscValueGetJSValue(resultPromise.get());
}

void WebExtensionAPITest::startNextTest()
{
    auto test = *m_testQueue.begin();

    WebProcess::singleton().send(Messages::WebExtensionController::TestStarted(test.testName, test.location.first, test.location.second), test.webExtensionControllerIdentifier);

    JSValueRef exception = nullptr;
    JSValueRef result = callObjectWithArguments<0>(test.testFunction, test.context, { }, &exception);

    GRefPtr<JSCContext> globalContext = jscContextGetOrCreate(JSContextGetGlobalContext(test.context));

    if (isThenable(test.context, result)) {
        GRefPtr<JSCValue> resolveFunction = adoptGRef(jsc_value_new_function (globalContext.get(), "resolve", G_CALLBACK(+[](JSCValue *result, gpointer userData) {
            auto& self = *static_cast<WebExtensionAPITest*>(userData);
            auto test = self.m_testQueue.takeFirst();
            WebProcess::singleton().send(Messages::WebExtensionController::TestFinished(test.testName, true, "Promise resolved without an error."_s, test.location.first, test.location.second), test.webExtensionControllerIdentifier);
            callObjectWithArguments<1>(test.resolveCallback, test.context, { jscValueGetJSValue(result) });

            self.m_hitAssertion = false;

            if (!self.m_testQueue.isEmpty())
                self.startNextTest();
            else
                self.m_runningTest = false;
        }), this, nullptr, G_TYPE_NONE, 1, JSC_TYPE_VALUE));
        GRefPtr<JSCValue> rejectFunction = adoptGRef(jsc_value_new_function (globalContext.get(), "reject", G_CALLBACK(+[](JSCValue *error, gpointer userData) {
            auto& self = *static_cast<WebExtensionAPITest*>(userData);
            auto test = self.m_testQueue.takeFirst();

            if (error || self.m_hitAssertion) {
                String errorMessage;
                if (error) {
                    JSCValue* errorMessageValue = jsc_value_is_object(error) && jsc_value_object_has_property(error, "message") ? jsc_value_object_get_property(error, "message") : error;
                    errorMessage = self.debugString(test.context, jscValueGetJSValue(errorMessageValue));
                } else if (!self.m_assertionMessage.isNull())
                    errorMessage = self.m_assertionMessage;

                errorMessage = !errorMessage.isEmpty() ? self.combineMessages("Promise rejected with an error: "_s, errorMessage) : "Promise rejected without an error"_s;

                WebProcess::singleton().send(Messages::WebExtensionController::TestFinished(test.testName, false, errorMessage, test.location.first, test.location.second), test.webExtensionControllerIdentifier);

                callObjectWithArguments<0>(test.rejectCallback, test.context, { });
            }

            self.m_hitAssertion = false;

            if (!self.m_testQueue.isEmpty())
                self.startNextTest();
            else
                self.m_runningTest = false;
        }), this, nullptr, G_TYPE_NONE, 1, JSC_TYPE_VALUE));

        g_object_unref(jsc_value_object_invoke_method(jscValueCreate(globalContext.get(), result), "then",
            JSC_TYPE_VALUE,
            resolveFunction.get(),

            JSC_TYPE_VALUE,
            rejectFunction.get(),

            G_TYPE_NONE
        ));
    } else {
        JSCValue* error = jscValueCreate(globalContext.get(), exception);
        if (error || m_hitAssertion) {
            String errorMessage;
            if (error) {
                JSCValue* errorMessageValue = jsc_value_is_object(error) && jsc_value_object_has_property(error, "message") ? jsc_value_object_get_property(error, "message") : error;
                errorMessage = debugString(test.context, jscValueGetJSValue(errorMessageValue));
            } else if (!m_assertionMessage.isNull())
                errorMessage = m_assertionMessage;

            errorMessage = !errorMessage.isEmpty() ? combineMessages("Promise rejected with an error: "_s, errorMessage) : "Promise rejected without an error"_s;

            WebProcess::singleton().send(Messages::WebExtensionController::TestFinished(test.testName, false, errorMessage, test.location.first, test.location.second), test.webExtensionControllerIdentifier);

            callObjectWithArguments<0>(test.rejectCallback, test.context, { });
        } else {
            WebProcess::singleton().send(Messages::WebExtensionController::TestFinished(test.testName, true, "Promise resolved without an error."_s, test.location.first, test.location.second), test.webExtensionControllerIdentifier);
            callObjectWithArguments<1>(test.resolveCallback, test.context, { jscValueGetJSValue(jscValueCreate(globalContext.get(), result)) });
        }

        m_hitAssertion = false;

        if (!m_testQueue.isEmpty())
            startNextTest();
        else
            m_runningTest = false;
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
