/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
#include "CryptoAlgorithmRSA_OAEP.h"

#include "CryptoAlgorithmRsaOaepParams.h"
#include "CryptoKeyRSA.h"
#include "ExceptionOr.h"
#include "OpenSSLUtilities.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformEncrypt(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& plainText)
{
#if defined(EVP_PKEY_CTX_set_rsa_oaep_md) && defined(EVP_PKEY_CTX_set_rsa_mgf1_md) && defined(EVP_PKEY_CTX_set0_rsa_oaep_label)
    const EVP_MD* md = digestAlgorithm(key.hashAlgorithmIdentifier());
    if (!md)
        return Exception { ExceptionCode::NotSupportedError };

    auto ctx = EvpPKeyCtxPtr(EVP_PKEY_CTX_new(key.platformKey(), nullptr));
    if (!ctx)
        return Exception { ExceptionCode::OperationError };

    if (EVP_PKEY_encrypt_init(ctx.get()) <= 0)
        return Exception { ExceptionCode::OperationError };

    if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) <= 0)
        return Exception { ExceptionCode::OperationError };

    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), md) <= 0)
        return Exception { ExceptionCode::OperationError };

    if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx.get(), md) <= 0)
        return Exception { ExceptionCode::OperationError };

    if (!parameters.labelVector().isEmpty()) {
        size_t labelSize = parameters.labelVector().size();
        // The library takes ownership of the label so the caller should not free the original memory pointed to by label.
        auto label = static_cast<uint8_t*>(OPENSSL_malloc(labelSize));
        memcpySpan(std::span(label, labelSize), parameters.labelVector().span());
        if (EVP_PKEY_CTX_set0_rsa_oaep_label(ctx.get(), label, labelSize) <= 0) {
            OPENSSL_free(label);
            return Exception { ExceptionCode::OperationError };
        }
    }

    size_t cipherTextLen;
    if (EVP_PKEY_encrypt(ctx.get(), nullptr, &cipherTextLen, plainText.span().data(), plainText.size()) <= 0)
        return Exception { ExceptionCode::OperationError };

    Vector<uint8_t> cipherText(cipherTextLen);
    if (EVP_PKEY_encrypt(ctx.get(), cipherText.mutableSpan().data(), &cipherTextLen, plainText.span().data(), plainText.size()) <= 0)
        return Exception { ExceptionCode::OperationError };
    cipherText.shrink(cipherTextLen);

    return cipherText;
#else
    return Exception { ExceptionCode::NotSupportedError };
#endif
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformDecrypt(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& cipherText)
{
#if defined(EVP_PKEY_CTX_set_rsa_oaep_md) && defined(EVP_PKEY_CTX_set_rsa_mgf1_md) && defined(EVP_PKEY_CTX_set0_rsa_oaep_label)
    const EVP_MD* md = digestAlgorithm(key.hashAlgorithmIdentifier());
    if (!md)
        return Exception { ExceptionCode::NotSupportedError };

    auto ctx = EvpPKeyCtxPtr(EVP_PKEY_CTX_new(key.platformKey(), nullptr));
    if (!ctx)
        return Exception { ExceptionCode::OperationError };

    if (EVP_PKEY_decrypt_init(ctx.get()) <= 0)
        return Exception { ExceptionCode::OperationError };

    if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) <= 0)
        return Exception { ExceptionCode::OperationError };

    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), md) <= 0)
        return Exception { ExceptionCode::OperationError };

    if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx.get(), md) <= 0)
        return Exception { ExceptionCode::OperationError };

    if (!parameters.labelVector().isEmpty()) {
        size_t labelSize = parameters.labelVector().size();
        // The library takes ownership of the label so the caller should not free the original memory pointed to by label.
        auto label = static_cast<uint8_t*>(OPENSSL_malloc(labelSize));
        memcpySpan(std::span(label, labelSize), parameters.labelVector().span());
        if (EVP_PKEY_CTX_set0_rsa_oaep_label(ctx.get(), label, labelSize) <= 0) {
            OPENSSL_free(label);
            return Exception { ExceptionCode::OperationError };
        }
    }

    size_t plainTextLen;
    if (EVP_PKEY_decrypt(ctx.get(), nullptr, &plainTextLen, cipherText.span().data(), cipherText.size()) <= 0)
        return Exception { ExceptionCode::OperationError };

    Vector<uint8_t> plainText(plainTextLen);
    if (EVP_PKEY_decrypt(ctx.get(), plainText.mutableSpan().data(), &plainTextLen, cipherText.span().data(), cipherText.size()) <= 0)
        return Exception { ExceptionCode::OperationError };
    plainText.shrink(plainTextLen);

    return plainText;
#else
    return Exception { ExceptionCode::NotSupportedError };
#endif
}

} // namespace WebCore
