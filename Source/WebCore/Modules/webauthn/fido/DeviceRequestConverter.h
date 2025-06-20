// Copyright 2017 The Chromium Authors. All rights reserved.
// Copyright (C) 2018 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#if ENABLE(WEB_AUTHN)

#include "AuthenticatorSupportedOptions.h"
#include "FidoConstants.h"
#include <wtf/Forward.h>

namespace WebCore {
struct PublicKeyCredentialCreationOptions;
struct PublicKeyCredentialRequestOptions;
struct PublicKeyCredentialDescriptor;
struct PublicKeyCredentialParameters;
}

namespace fido {

struct PinParameters {
    int protocol;
    Vector<uint8_t> auth;
};

// Serializes MakeCredential request parameter into CBOR encoded map with
// integer keys and CBOR encoded values as defined by the CTAP spec.
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html#authenticatorMakeCredential
WEBCORE_EXPORT Vector<uint8_t> encodeMakeCredentialRequestAsCBOR(const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialCreationOptions&, AuthenticatorSupportedOptions::UserVerificationAvailability, AuthenticatorSupportedOptions::ResidentKeyAvailability, const Vector<String>& authenticatorSupportedExtensions, std::optional<PinParameters> = std::nullopt, const std::optional<Vector<WebCore::PublicKeyCredentialParameters>>& = std::nullopt, std::optional<Vector<WebCore::PublicKeyCredentialDescriptor>>&& = std::nullopt);

// Serializes GetAssertion request parameter into CBOR encoded map with
// integer keys and CBOR encoded values as defined by the CTAP spec.
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html#authenticatorGetAssertion
WEBCORE_EXPORT Vector<uint8_t> encodeGetAssertionRequestAsCBOR(const Vector<uint8_t>& hash, const WebCore::PublicKeyCredentialRequestOptions&, AuthenticatorSupportedOptions::UserVerificationAvailability, const Vector<String>& authenticatorSupportedExtensions, std::optional<PinParameters> = std::nullopt, std::optional<Vector<WebCore::PublicKeyCredentialDescriptor>>&& = std::nullopt);

WEBCORE_EXPORT Vector<uint8_t> encodeBogusRequestForAuthenticatorSelection();
WEBCORE_EXPORT Vector<uint8_t> encodeSilentGetAssertion(const String& rpId, const Vector<uint8_t>& hash, const Vector<WebCore::PublicKeyCredentialDescriptor>& credentials, std::optional<PinParameters> = std::nullopt);

// Represents CTAP requests with empty parameters, including
// AuthenticatorGetInfo, AuthenticatorReset and AuthenticatorGetNextAssertion commands.
WEBCORE_EXPORT Vector<uint8_t> encodeEmptyAuthenticatorRequest(CtapRequestCommand);

} // namespace fido

#endif // ENABLE(WEB_AUTHN)
