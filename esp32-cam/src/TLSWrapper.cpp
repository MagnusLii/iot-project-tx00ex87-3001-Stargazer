#include "TLSWrapper.hpp"
#include "mbedtls/debug.h"
#include "debug.hpp"

TLSWrapper::TLSWrapper() {
    mbedtls_net_init(&net_ctx);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&ssl_conf);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

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
    if (mbedtls_net_connect(&net_ctx, host, port, MBEDTLS_NET_PROTO_TCP) != 0) {
        DEBUG("TCP connection failed");
        return false;
    }

    if (mbedtls_ssl_config_defaults(&ssl_conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        DEBUG("Failed to configure SSL");
        return false;
    }

    mbedtls_ssl_conf_rng(&ssl_conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_setup(&ssl, &ssl_conf);
    mbedtls_ssl_set_bio(&ssl, &net_ctx, mbedtls_net_send, mbedtls_net_recv, nullptr);

    if (mbedtls_ssl_handshake(&ssl) != 0) {
        DEBUG("TLS handshake failed");
        return false;
    }

    DEBUG("TLS handshake successful");
    return true;
}

int TLSWrapper::send(const char *data, size_t len) {
    return mbedtls_ssl_write(&ssl, (const unsigned char *)data, len);
}

int TLSWrapper::receive(char *buffer, size_t maxLen) {
    return mbedtls_ssl_read(&ssl, (unsigned char *)buffer, maxLen);
}

void TLSWrapper::close() {
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&net_ctx);
}
