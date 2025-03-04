#include "TLSWrapper.hpp"
#include "debug.hpp"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/debug.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "testMacros.hpp"
#include "mbedtls/platform.h"
#include "esp_heap_caps.h"

TLSWrapper::TLSWrapper() {
    // Initialize net context
    mbedtls_net_init(&this->net_ctx);
    while (this->net_ctx.fd != -1) {
        mbedtls_net_free(&this->net_ctx);
        mbedtls_net_init(&this->net_ctx);
        DEBUG("Failed to initialize net context");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Initialize SSL context
    mbedtls_ssl_init(&this->ssl);

    // Initialize SSL configuration
    mbedtls_ssl_config_init(&this->ssl_conf);

    // Initialize CTR-DRBG (Random number generator)
    mbedtls_ctr_drbg_init(&this->ctr_drbg);

    // Initialize entropy context
    mbedtls_entropy_init(&this->entropy);

    // Initialize CA certificate context
    mbedtls_x509_crt_init(&this->ca_cert);

#ifdef TLS_DEBUG
    DEBUG("Enabling TLS debug");
    mbedtls_debug_set_threshold(4);
    mbedtls_ssl_conf_dbg(&ssl_conf, mbedtls_debug_cb, NULL);
#endif

    // Seed the RNG
    int ret = mbedtls_ctr_drbg_seed(&this->ctr_drbg, mbedtls_entropy_func, &this->entropy, nullptr, 0);
    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        DEBUG("Failed to initialize RNG, error code: ", ret, " - ", error_buf);
    }
}

TLSWrapper::~TLSWrapper() {
    close();
    mbedtls_ssl_free(&this->ssl);
    mbedtls_ssl_config_free(&this->ssl_conf);
    mbedtls_ctr_drbg_free(&this->ctr_drbg);
    mbedtls_entropy_free(&this->entropy);
    mbedtls_x509_crt_free(&this->ca_cert);
}

bool TLSWrapper::is_net_ctx_valid() {
    if (this->net_ctx.fd != -1) {
        mbedtls_net_free(&this->net_ctx);
        mbedtls_net_init(&this->net_ctx);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return (this->net_ctx.fd == -1); // Check if the socket is valid
}

bool TLSWrapper::connect(const char *host, const char *port, const char *root_cert) {
    if (!host || !port || !root_cert) {
        DEBUG("Invalid arguments: host, port, or root_cert is null");
        return false;
    }

    if (strlen(host) == 0 || strlen(port) == 0) {
        DEBUG("Invalid arguments: host or port is empty");
        return false;
    }

    int ret;
    size_t cert_len = strlen(root_cert) + 1;

    if (!is_net_ctx_valid()) {
        DEBUG("Invalid net_ctx");
        return false;
    }

    ret = mbedtls_net_connect(&this->net_ctx, host, port, MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        DEBUG("TCP connection failed, error code: ", ret, " - ", error_buf);
        return false;
    }
    DEBUG("Connection done");

    int conf_ret = mbedtls_ssl_config_defaults(&this->ssl_conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                               MBEDTLS_SSL_PRESET_DEFAULT);
    if (conf_ret != 0) {
        DEBUG("SSL config failed, error code: ", conf_ret);
        return false;
    }
    DEBUG("SSL config done");

#ifdef DISABLE_CERTIFICATE_VERIFICATION
    DEBUG("Disabling certificate verification");
    mbedtls_ssl_conf_authmode(&this->ssl_conf, MBEDTLS_SSL_VERIFY_NONE);
#else
    mbedtls_ssl_conf_authmode(&this->ssl_conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    DEBUG("Enabling certificate verification");

    if (this->ca_cert.next != nullptr) {
        mbedtls_x509_crt_free(&this->ca_cert);
    }
    mbedtls_x509_crt_init(&this->ca_cert);
    DEBUG("Parsing CA certificate");

    ret = mbedtls_x509_crt_parse(&this->ca_cert, (const unsigned char *)root_cert, cert_len);
    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        DEBUG("Failed to parse CA certificate, error code: %d - %s", ret, error_buf);
        return false;
    }
    DEBUG("CA certificate parsed");

    mbedtls_ssl_conf_ca_chain(&this->ssl_conf, &this->ca_cert, NULL);
    DEBUG("CA certificate chain set");
#endif

    mbedtls_ssl_conf_rng(&this->ssl_conf, mbedtls_ctr_drbg_random, &this->ctr_drbg);
    ret = mbedtls_ssl_setup(&this->ssl, &this->ssl_conf);
    if (ret != 0) {
        DEBUG("mbedtls_ssl_setup failed, error code: ", ret);
        return false;
    }
    DEBUG("SSL setup done");

    mbedtls_ssl_set_bio(&this->ssl, &this->net_ctx, mbedtls_net_send, mbedtls_net_recv, nullptr);
    if (mbedtls_ssl_set_hostname(&this->ssl, "stargazer") != 0) {
        DEBUG("Setting hostname failed");
        return false;
    }
    DEBUG("Hostname set");

    ret = mbedtls_ssl_handshake(&this->ssl);
    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        DEBUG("TLS handshake failed, error code: ", ret, " - ", error_buf);
        return false;
    }

    DEBUG("TLS handshake successful");
    return true;
}

int TLSWrapper::send(const char *data, size_t len) {
    return mbedtls_ssl_write(&this->ssl, (const unsigned char *)data, len);
}

int TLSWrapper::receive(char *buffer, size_t maxLen) {
    return mbedtls_ssl_read(&this->ssl, (unsigned char *)buffer, maxLen);
}

void TLSWrapper::close() {
    mbedtls_ssl_close_notify(&this->ssl);
    mbedtls_net_free(&this->net_ctx);
}

#ifdef TLS_DEBUG
void mbedtls_debug_cb(void *ctx, int level, const char *file, int line, const char *str) {
    DEBUG("mbedtls debug ", level, " : ", file, " : ", line, " : ", str);
}
#endif
