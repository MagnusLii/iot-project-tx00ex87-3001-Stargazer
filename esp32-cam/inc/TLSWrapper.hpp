#ifndef TLSWRAPPER_H
#define TLSWRAPPER_H

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include <cstring>

// #define DISABLE_CERTIFICATE_VERIFICATION
#define TLS_DEBUG

class TLSWrapper {
  public:
    TLSWrapper();
    ~TLSWrapper();

    bool connect(const char *host, const char *port, const char *root_cert);
    int send(const char *data, size_t len);
    int receive(char *buffer, size_t maxLen);
    void close();

  private:
    bool is_net_ctx_valid();

    mbedtls_net_context net_ctx;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config ssl_conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_x509_crt ca_cert;
};

#ifdef TLS_DEBUG
void mbedtls_debug_cb(void *ctx, int level, const char *file, int line, const char *str);
#endif // TLS_DEBUG

#endif // TLSWRAPPER_H
