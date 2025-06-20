/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "config.h"

#import "PlatformUtilities.h"
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKWebView.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/NSURLExtras.h>

static bool didFinishTest;
static bool didFailProvisionalLoad;

@interface LoadInvalidURLWebFrameLoadDelegate : NSObject <WebFrameLoadDelegate> {
}
@end

@implementation LoadInvalidURLWebFrameLoadDelegate

- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame
{
    didFinishTest = true;
}

- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    EXPECT_WK_STREQ(error.domain, @"WebKitErrorDomain");
    EXPECT_EQ(error.code, WebKitErrorCannotShowURL);

    static char literal[] = "https://www.example.com$/";
    NSURL *failedURL = WTF::URLWithData([NSData dataWithBytes:literal length:strlen(literal)], nil);
    EXPECT_TRUE([error.userInfo[NSURLErrorFailingURLErrorKey] isEqual:failedURL]);

    didFailProvisionalLoad = true;
    didFinishTest = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, LoadInvalidURLRequest)
{
    @autoreleasepool {
        RetainPtr<WebView> webView = adoptNS([[WebView alloc] init]);

        RetainPtr<LoadInvalidURLWebFrameLoadDelegate> delegate = adoptNS([[LoadInvalidURLWebFrameLoadDelegate alloc] init]);
        [webView setFrameLoadDelegate:delegate.get()];

        NSURL *contentURL = [NSBundle.test_resourcesBundle URLForResource:@"LoadInvalidURLRequest" withExtension:@"html"];
        [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:contentURL]];

        didFinishTest = false;
        didFailProvisionalLoad = false;
        Util::run(&didFinishTest);

        EXPECT_FALSE(didFailProvisionalLoad);
    }
}

} // namespace TestWebKitAPI

