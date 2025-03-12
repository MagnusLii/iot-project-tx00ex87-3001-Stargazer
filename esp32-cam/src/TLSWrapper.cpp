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

/**
 * @brief Initializes the TLSWrapper class, setting up necessary contexts for network communication and TLS operations.
 * 
 * This constructor initializes various components required for TLS communication:
 * - Network context (`net_ctx`) for socket-based communication.
 * - SSL context (`ssl`) for establishing secure connections.
 * - SSL configuration (`ssl_conf`) for configuring the SSL connection.
 * - CTR-DRBG (random number generator) context (`ctr_drbg`) for secure random number generation.
 * - Entropy context (`entropy`) for gathering entropy data.
 * - CA certificate context (`ca_cert`) for validating remote certificates.
 * 
 * Additionally, it attempts to seed the random number generator (RNG) using entropy data, and provides debug capabilities if TLS debugging is enabled.
 * 
 * @note If any initialization fails, the constructor will log the error and attempt re-initialization for the network context.
 */
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

/**
 * @brief Destructor for the TLSWrapper class.
 * 
 * This destructor cleans up and frees the resources allocated for TLS communication:
 * - Closes any active connections and cleans up the network context.
 * - Frees the SSL context (`ssl`) and configuration (`ssl_conf`).
 * - Releases the CTR-DRBG (random number generator) context (`ctr_drbg`).
 * - Frees the entropy context (`entropy`) used for random number generation.
 * - Frees the CA certificate context (`ca_cert`) used for certificate validation.
 * 
 * It ensures that all memory and resources allocated during the lifetime of the `TLSWrapper` object are properly released.
 */
TLSWrapper::~TLSWrapper() {
    close();
    mbedtls_ssl_free(&this->ssl);
    mbedtls_ssl_config_free(&this->ssl_conf);
    mbedtls_ctr_drbg_free(&this->ctr_drbg);
    mbedtls_entropy_free(&this->entropy);
    mbedtls_x509_crt_free(&this->ca_cert);
}

/**
 * @brief Checks if the network context (net_ctx) is valid.
 * 
 * This function checks the validity of the network context by verifying if the file descriptor (`fd`) is not equal to `-1`.
 * If the file descriptor is invalid, the function attempts to reinitialize the network context by freeing and initializing it again.
 * It then checks whether the network context's socket file descriptor is still invalid, returning `true` if it is invalid, or `false` if it is valid.
 * 
 * @return `true` if the network context's file descriptor is invalid; `false` if it is valid.
 */
bool TLSWrapper::is_net_ctx_valid() {
    if (this->net_ctx.fd != -1) {
        mbedtls_net_free(&this->net_ctx);
        mbedtls_net_init(&this->net_ctx);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return (this->net_ctx.fd == -1); // Check if the socket is valid
}

/**
 * @brief Establishes a TCP connection and performs a TLS handshake with a specified host and port.
 * 
 * This function first validates the input arguments (`host`, `port`, and `root_cert`) to ensure they are not null or empty. It then checks the validity of the network context. 
 * A TCP connection is established with the specified host and port using the `mbedtls_net_connect` function. 
 * Afterward, it sets up SSL configuration with default values and enables certificate verification (unless explicitly disabled). 
 * If certificate verification is enabled, the provided CA certificate is parsed and set up for use in the TLS connection. 
 * The SSL setup is completed, and a TLS handshake is initiated. 
 * If any step fails, an appropriate error message is logged, and the function returns `false`. If the connection and handshake succeed, it returns `true`.
 * 
 * @param host The hostname or IP address of the server to connect to.
 * @param port The port to use for the connection.
 * @param root_cert The root certificate to use for verifying the server's certificate.
 * 
 * @return `true` if the connection and TLS handshake are successful; `false` otherwise.
 */
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

/**
 * @brief Sends data over an established TLS connection.
 * 
 * This function uses the `mbedtls_ssl_write` function to send the provided data through the TLS connection.
 * The data is written to the connection using the `ssl` context. The function returns the result of the `mbedtls_ssl_write` call, 
 * which indicates the number of bytes written or an error code.
 * 
 * @param data The data to be sent over the connection.
 * @param len The length of the data to be sent.
 * 
 * @return The number of bytes written on success, or a negative error code on failure.
 */
int TLSWrapper::send(const char *data, size_t len) {
    return mbedtls_ssl_write(&this->ssl, (const unsigned char *)data, len);
}

/**
 * @brief Receives data from an established TLS connection.
 * 
 * This function uses the `mbedtls_ssl_read` function to receive data from the TLS connection and store it in the provided buffer.
 * The data is read from the connection using the `ssl` context. The function returns the number of bytes read, 
 * or a negative error code if an error occurs during the read operation.
 * 
 * @param buffer The buffer where the received data will be stored.
 * @param maxLen The maximum number of bytes to read into the buffer.
 * 
 * @return The number of bytes read on success, or a negative error code on failure.
 */
int TLSWrapper::receive(char *buffer, size_t maxLen) {
    return mbedtls_ssl_read(&this->ssl, (unsigned char *)buffer, maxLen);
}

/**
 * @brief Closes the TLS connection and frees associated resources.
 * 
 * This function sends a close notification to the peer via `mbedtls_ssl_close_notify`, 
 * then frees the network context (`net_ctx`) to release any allocated resources. 
 * It effectively terminates the TLS session and prepares the context for reuse or cleanup.
 * 
 * @return void
 */
void TLSWrapper::close() {
    mbedtls_ssl_close_notify(&this->ssl);
    mbedtls_net_free(&this->net_ctx);
}

#ifdef TLS_DEBUG
/**
 * @brief Callback function for mbedtls debugging output.
 * 
 * This function is used as a callback for mbedtls debug messages. It prints debug
 * information, including the debug level, file, line number, and the message string,
 * to the debug output.
 * 
 * @param ctx   A pointer to the context (unused in this implementation).
 * @param level The mbedtls debug level.
 * @param file  The source file where the debug message originated.
 * @param line  The line number in the source file where the debug message originated.
 * @param str   The debug message string.
 * 
 * @return void
 */
void mbedtls_debug_cb(void *ctx, int level, const char *file, int line, const char *str) {
    DEBUG("mbedtls debug ", level, " : ", file, " : ", line, " : ", str);
}
#endif
