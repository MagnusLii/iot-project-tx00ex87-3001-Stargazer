#include "TLSWrapper.hpp"
#include "debug.hpp"
#include "mbedtls/debug.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "testMacros.hpp"

// #define DISABLE_CERTIFICATE_VERIFICATION
// #define TLS_DEBUG

TLSWrapper::TLSWrapper() {
    mbedtls_net_init(&net_ctx);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&ssl_conf);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

#ifdef TLS_DEBUG
    // Does not work with tls1.3 enabled
    DEBUG("Enabling TLS debug");
    mbedtls_debug_set_threshold(4);                          // Set debug level to show detailed logs
    mbedtls_ssl_conf_dbg(&ssl_conf, mbedtls_debug_cb, NULL); // Enable debug
#endif                                                       // TLS_DEBUG

    // Initialize entropy and random number generator
    if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, nullptr, 0) != 0) {
        DEBUG("Failed to initialize RNG");
    }
}

TLSWrapper::~TLSWrapper() {
    close();
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&ssl_conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

bool TLSWrapper::connect(const char *host, const char *port) {
    int ret;

    if (mbedtls_net_connect(&net_ctx, host, port, MBEDTLS_NET_PROTO_TCP) != 0) {
        DEBUG("TCP connection failed");
        return false;
    }

    int conf_ret = mbedtls_ssl_config_defaults(&ssl_conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                               MBEDTLS_SSL_PRESET_DEFAULT);
    if (conf_ret != 0) {
        DEBUG("SSL config failed, error code: ", conf_ret);
        return false;
    }

#ifdef DISABLE_CERTIFICATE_VERIFICATION
    DEBUG("disabling certificate verification");
    mbedtls_ssl_conf_authmode(&ssl_conf, MBEDTLS_SSL_VERIFY_NONE);
#else
    mbedtls_ssl_conf_authmode(&ssl_conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    static const char *root_cert = TEST_CERTIFICATE; // TODO: read cert from sdcard
    mbedtls_x509_crt ca_cert;
    mbedtls_x509_crt_init(&ca_cert);
    ret = mbedtls_x509_crt_parse(&ca_cert, (const unsigned char *)root_cert, strlen(root_cert) + 1);
    if (ret != 0) {
        DEBUG("Failed to parse CA certificate, error code: ", ret);
        return false;
    }

    mbedtls_ssl_conf_ca_chain(&ssl_conf, &ca_cert, NULL); // Set CA chain
#endif // DISABLE_CERTIFICATE_VERIFICATION

    mbedtls_ssl_conf_rng(&ssl_conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_setup(&ssl, &ssl_conf);
    mbedtls_ssl_set_bio(&ssl, &net_ctx, mbedtls_net_send, mbedtls_net_recv, nullptr);

    const char *hostname = "stargazer.local";
    if (mbedtls_ssl_set_hostname(&ssl, hostname) != 0) {
        DEBUG("Setting hostname failed");
        return false;
    }

    ret = mbedtls_ssl_handshake(&ssl);
    if (ret != 0) {
        DEBUG("TLS handshake failed, error code: ", ret);
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        DEBUG("TLS handshake error: ", error_buf);
        return false;
    }

    DEBUG("TLS handshake successful");
    return true;
}

int TLSWrapper::send(const char *data, size_t len) { return mbedtls_ssl_write(&ssl, (const unsigned char *)data, len); }

int TLSWrapper::receive(char *buffer, size_t maxLen) { return mbedtls_ssl_read(&ssl, (unsigned char *)buffer, maxLen); }

void TLSWrapper::close() {
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&net_ctx);
}

#ifdef TLS_DEBUG
void mbedtls_debug_cb(void *ctx, int level, const char *file, int line, const char *str) {
    DEBUG("mbedtls debug ", level, " : ", file, " : ", line, " : ", str);
}
#endif // TLS_DEBUG