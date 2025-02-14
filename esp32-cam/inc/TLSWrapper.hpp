#ifndef TLSWRAPPER_H
#define TLSWRAPPER_H

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include <cstring>

class TLSWrapper {
public:
    TLSWrapper();
    ~TLSWrapper();

    bool connect(const char *host, const char *port);
    int send(const char *data, size_t len);
    int receive(char *buffer, size_t maxLen);
    void close();

private:
    mbedtls_net_context net_ctx;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config ssl_conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
};

#endif // TLSWRAPPER_H
