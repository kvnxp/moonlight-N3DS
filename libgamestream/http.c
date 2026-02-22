/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "http.h"
#include "errors.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#ifdef __3DS__
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#else
#include <curl/curl.h>
#endif

static bool debug;
#ifdef __3DS__
/* no external curl library available on 3DS; we implement a tiny HTTP GET
   routine using BSD sockets.  Only plain HTTP is supported, and TLS/HTTPS is
   ignored (fallbacks will also try HTTP). */
#else
static CURL *curl;
#endif

static size_t _write_curl(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  PHTTP_DATA mem = (PHTTP_DATA)userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL)
    return 0;

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

int http_init(const char* keyDirectory, int logLevel) {
  debug = logLevel >= 2;
#ifndef __3DS__
  curl = curl_easy_init();
  if (!curl)
    return GS_FAILED;

  char certificateFilePath[4096];
  snprintf(certificateFilePath, sizeof(certificateFilePath), "%s/%s", keyDirectory, CERTIFICATE_FILE_NAME);

  char keyFilePath[4096];
  snprintf(keyFilePath, sizeof(keyFilePath), "%s/%s", keyDirectory, KEY_FILE_NAME);

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_SSLENGINE_DEFAULT, 1L);
  curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE,"PEM");
  curl_easy_setopt(curl, CURLOPT_SSLCERT, certificateFilePath);
  curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "PEM");
  curl_easy_setopt(curl, CURLOPT_SSLKEY, keyFilePath);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _write_curl);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_SESSIONID_CACHE, 0L);
#endif
  return GS_OK;
}

int http_request(char* url, PHTTP_DATA data) {
#ifndef __3DS__
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
  curl_easy_setopt(curl, CURLOPT_URL, url);
#ifdef __FreeBSD__
  curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);
#endif
  if (debug) {
    printf("Request %s\n", url);
  }

  if (data->size > 0) {
    free(data->memory);
    data->memory = malloc(1);
    if(data->memory == NULL)
      return GS_OUT_OF_MEMORY;

    data->size = 0;
  }
  CURLcode res = curl_easy_perform(curl);

  if(res != CURLE_OK) {
    gs_error = curl_easy_strerror(res);
    return GS_FAILED;
  } else if (data->memory == NULL) {
    return GS_OUT_OF_MEMORY;
  }

  /* print additional information when running in debug mode */
  if (debug) {
    printf("[libgamestream] http_request: url=%s body_size=%zu\n",
           url, data->size);
    printf("Response:\n%s\n\n", data->memory);
  }

  /* treat an empty body as invalid; this is what triggered the
     ambiguous "no element found" message in xml parser. */
  if (data->size == 0) {
    gs_error = "empty HTTP response";
    return GS_IO_ERROR;
  }

  return GS_OK;
#else
  // Minimal HTTP GET implementation
  char scheme[16] = {0}, host[256] = {0}, path[2048] = "/";
  int port = 0;
  char *p, *rest;
  // split scheme
  p = strstr(url, "://");
  if (!p) {
    gs_error = "malformed URL";
    return GS_FAILED;
  }
  size_t schemelen = p - url;
  if (schemelen >= sizeof(scheme)) schemelen = sizeof(scheme) - 1;
  memcpy(scheme, url, schemelen);
  rest = p + 3;
  // host and optional port
  char *slash = strchr(rest, '/');
  char hostport[512];
  if (slash) {
    size_t hplen = slash - rest;
    if (hplen >= sizeof(hostport)) hplen = sizeof(hostport) - 1;
    memcpy(hostport, rest, hplen);
    hostport[hplen] = '\0';
    strncpy(path, slash, sizeof(path)-1);
  } else {
    strncpy(hostport, rest, sizeof(hostport)-1);
  }
  char *colon = strchr(hostport, ':');
  if (colon) {
    *colon = '\0';
    port = atoi(colon + 1);
  }
  strncpy(host, hostport, sizeof(host)-1);
  if (port == 0) {
    port = (strcmp(scheme, "https") == 0) ? 47984 : 47989;
  }

  if (debug)
    printf("[http] GET %s://%s:%d%s\n", scheme, host, port, path);

  struct addrinfo hints = {0}, *res = NULL;
  char portstr[6];
  snprintf(portstr, sizeof(portstr), "%d", port);
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) {
    gs_error = "DNS lookup failed";
    return GS_IO_ERROR;
  }

  int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sock < 0) {
    gs_error = strerror(errno);
    freeaddrinfo(res);
    return GS_IO_ERROR;
  }

  if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
    gs_error = strerror(errno);
    close(sock);
    freeaddrinfo(res);
    return GS_IO_ERROR;
  }
  freeaddrinfo(res);

  // send request
  char req[4096];
  snprintf(req, sizeof(req),
           "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
           path, host);
  send(sock, req, strlen(req), 0);

  // read response
  data->memory = malloc(1);
  data->size = 0;
  char buf[512];
  int n;
  bool header_done = false;
  while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
    if (!header_done) {
      char *hpos = strstr(buf, "\r\n\r\n");
      if (hpos) {
        header_done = true;
        int hdrlen = (hpos - buf) + 4;
        n -= hdrlen;
        memmove(buf, buf + hdrlen, n);
      } else {
        continue; // skip headers
      }
    }
    data->memory = realloc(data->memory, data->size + n + 1);
    memcpy(data->memory + data->size, buf, n);
    data->size += n;
    data->memory[data->size] = 0;
  }
  close(sock);

  if (debug)
    printf("[http] received %zu bytes\n", data->size);

  if (data->size == 0) {
    gs_error = "empty HTTP response";
    return GS_IO_ERROR;
  }
  return GS_OK;
#endif
}

void http_cleanup() {
#ifndef __3DS__
  curl_easy_cleanup(curl);
#endif
}

PHTTP_DATA http_create_data() {
  PHTTP_DATA data = malloc(sizeof(HTTP_DATA));
  if (data == NULL)
    return NULL;

  data->memory = malloc(1);
  if(data->memory == NULL) {
    free(data);
    return NULL;
  }
  data->size = 0;

  return data;
}

void http_free_data(PHTTP_DATA data) {
  if (data != NULL) {
    if (data->memory != NULL)
      free(data->memory);

    free(data);
  }
}
