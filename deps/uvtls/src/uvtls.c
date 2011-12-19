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

#include "uvtls.h"

int uv_tls_init(uv_loop_t* loop, uv_tls_t* tls, SSL *pssl) {
  /* TODO: figure out better handle type? */
  uv__stream_init(loop, (uv_stream_t*)tls, UV_TCP);
  return 0;
}
