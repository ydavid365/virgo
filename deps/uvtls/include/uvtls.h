/*
 *  Copyright 2011 Rackspace
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#ifndef _UVTLS_H_
#define _UVTLS_H_

#define UV_EXPORT_PRIVATE

#include "uv.h"
#include <openssl/ssl.h>

/**
 * We hard code a check here for the version of OpenSSL we bundle inside deps, because it is
 * too easily to accidently pull in an older version of OpenSSL on random platforms with
 * weird include paths.
 */
#if OPENSSL_VERSION_NUMBER != 0x1000005fL
#error Invalid OpenSSL version number. Busted Include Paths?
#endif

typedef struct uv_tls_s uv_tls_t;

struct uv_tls_s {
  UV_HANDLE_FIELDS
  UV_STREAM_FIELDS
  uv_tcp_t *tcp;
  SSL *ssl;
  BIO *bio_read;
  BIO *bio_write;
};

/* initialize a TLS handle, taking resposibility for the SSL object's lifetime */
UV_EXTERN int uv_tls_init(uv_loop_t* loop, uv_tls_t* handle, SSL *pssl);

/* Connect TLS stream to a TCP stream */
UV_EXTERN int uv_tls_start(uv_tls_t* handle, uv_tcp_t *tcp, int is_server);


#endif