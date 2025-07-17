#include <memory>
#include <string>
#include <string_view>
#include <sstream>
#include <stdexcept>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/decoder.h>
#include <openssl/core_names.h>

#include "base64.h"

namespace tateyama::authentication {

template<typename T, typename D>
std::unique_ptr<T, D> make_handle(T* handle, D deleter)
{
    return std::unique_ptr<T, D>{handle, deleter};
}

class rsa_encrypter {
public:
    explicit rsa_encrypter(std::string_view public_key_text) : public_key_(public_key_text) {
        evp_public_key_ = make_handle(get_public_key(), EVP_PKEY_free);
        if (!evp_public_key_) {
            throw std::runtime_error("Get public key failed.");
        }
    }

    void encrypt(std::string_view in, std::string &out) {
        size_t buf_len = 0;
        OSSL_PARAM params[5];  // NOLINT

        auto ctx = make_handle(EVP_PKEY_CTX_new_from_pkey(nullptr, evp_public_key_.get(), nullptr), EVP_PKEY_CTX_free);
        if (!ctx) {
            throw std::runtime_error("fail to EVP_PKEY_CTX_new_from_pkey");
        }
        set_optional_params(params);  // NOLINT
        if (EVP_PKEY_encrypt_init_ex(ctx.get(), params) <= 0) {  // NOLINT
            throw std::runtime_error("fail to EVP_PKEY_encrypt_init_ex");
        }
        if (EVP_PKEY_encrypt(ctx.get(), nullptr, &buf_len, reinterpret_cast<const unsigned char*>(in.data()), in.length()) <= 0) {  // NOLINT
            throw std::runtime_error("fail to EVP_PKEY_encrypt first");
        }
        out.resize(buf_len);
        if (EVP_PKEY_encrypt(ctx.get(), reinterpret_cast<unsigned char*>(out.data()), &buf_len, reinterpret_cast<const unsigned char*>(in.data()), in.length()) <= 0) {  // NOLINT
            throw std::runtime_error("fail to EVP_PKEY_encrypt final");
        }
    }

private:
    std::string public_key_{};

    std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY*)> evp_public_key_{nullptr, nullptr};

    EVP_PKEY *get_public_key() {
        EVP_PKEY *pkey = nullptr;
        int selection{EVP_PKEY_PUBLIC_KEY};
        const unsigned char *data{};
        size_t data_len{};

        data = reinterpret_cast<const unsigned char*>(public_key_.data());  // NOLINT
        data_len = public_key_.length();
        auto dctx = make_handle(OSSL_DECODER_CTX_new_for_pkey(&pkey, "PEM", nullptr, "RSA",
                                                              selection, nullptr, nullptr), OSSL_DECODER_CTX_free);
        if (OSSL_DECODER_from_data(dctx.get(), &data, &data_len)  <= 0) {
            throw std::runtime_error("fail to handle public key");
        }
        return pkey;
    }

    void set_optional_params(OSSL_PARAM *p) {
        /* "pkcs1" is used by default if the padding mode is not set */
        *p++ = OSSL_PARAM_construct_utf8_string(OSSL_ASYM_CIPHER_PARAM_PAD_MODE,     // NOLINT
                                                OSSL_PKEY_RSA_PAD_MODE_PKCSV15, 0);  // NOLINT
        *p = OSSL_PARAM_construct_end();
    }
};

}  // tateyama::authentication
