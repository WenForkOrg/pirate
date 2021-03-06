/*
 * This work was authored by Two Six Labs, LLC and is sponsored by a subcontract
 * agreement with Galois, Inc.  This material is based upon work supported by
 * the Defense Advanced Research Projects Agency (DARPA) under Contract No.
 * HR0011-19-C-0103.
 *
 * The Government has unlimited rights to use, modify, reproduce, release,
 * perform, display, or disclose computer software or computer software
 * documentation marked with this legend. Any reproduction of technical data,
 * computer software, or portions thereof marked with this legend must also
 * reproduce this marking.
 *
 * Copyright 2019 Two Six Labs, LLC.  All rights reserved.
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <pthread.h>
#include <openssl/sha.h>
#include "libpirate.h"

#ifdef GAPS_ENABLE
#ifndef __GAPS__
#error "gaps compiler must be used"
#endif
#define PIRATE_ENCLAVE_MAIN(e)  __attribute__((pirate_enclave_main(e)))
#else
#define PIRATE_ENCLAVE_MAIN(e)
#endif

#define MAX_APP_THREADS         3
#define MAX_APP_GAPS_CHANNELS   4
typedef struct {
    void *(*func) (void *);
    void *arg;
    const char *name;
    pthread_t tid;
} thread_ctx_t;

#define THREAD_ADD(f, a, n) {.func = f, .arg = a, .name = n, .tid = 0 }
#define THREAD_END          THREAD_ADD(NULL, NULL, NULL)

typedef struct {
    int num;
    int flags;
    const char *conf;
    const char* desc;
} gaps_channel_ctx_t;

typedef struct {
    gaps_channel_ctx_t ch[MAX_APP_GAPS_CHANNELS];
    thread_ctx_t threads[MAX_APP_THREADS];
    int signal_fd;
    void (*on_shutdown) (void);
} gaps_app_t;
#define GAPS_CHANNEL(n, f, c, d)    \
{                                   \
    .num   = n,                     \
    .flags = f,                     \
    .conf  = c,                     \
    .desc  = d                      \
}
#define GAPS_CHANNEL_END        GAPS_CHANNEL(-1, 0, NULL, NULL)

#define DEFAULT_GAPS_CHANNEL    "pipe"

int gaps_app_run(gaps_app_t *ctx);
int gaps_app_wait_exit(gaps_app_t *ctx);
void gaps_terminate();
int gaps_running();

/* GAPS channel assignments */
typedef enum {
    CLIENT_TO_PROXY = 0,
    PROXY_TO_CLIENT = 1,
    PROXY_TO_SIGNER = 2,
    SIGNER_TO_PROXY = 3
} demo_channel_t;

#define PROXY_TO_SIGNER_WR "serial,/dev/tty_P_TO_S_WR"
#define PROXY_TO_SIGNER_RD "serial,/dev/tty_P_TO_S_RD"
#define SIGNER_TO_PROXY_WR "serial,/dev/tty_S_TO_P_WR"
#define SIGNER_TO_PROXY_RD "serial,/dev/tty_S_TO_P_RD"

typedef enum {
    OK,
    BUSY,
    ERR,
    UNKNOWN
} ts_status_t;

const char *ts_status_str(ts_status_t sts);

/* Verbosity levels */
typedef enum {
    VERBOSITY_NONE = 0,
    VERBOSITY_MIN  = 1,
    VERBOSITY_MAX  = 2
} verbosity_t;

/* GAPS channel type used for the demo */
#define GAPS_CH_TYPE PIPE

#define NONCE_LENGTH    8
#define MAX_REQ_LEN     128
#define TS_PATH_MAX     128
#define MAX_TS_LEN      (8 << 10)

#define IMAGE_WIDTH 640
#define IMAGE_HEIGHT 480
#define DEFAULT_REQUEST_DELAY_MS 2000
#define DEFAULT_VIDEO_DEVICE "/dev/video0"

/* Structure passed from the client to the proxy */
typedef struct {
    unsigned char digest[SHA256_DIGEST_LENGTH];
} proxy_request_t;
#define PROXY_REQUEST_INIT { .digest = { 0 } }

/* Structure passed from the proxy to the signing service */
typedef struct {
    uint32_t len;
    unsigned char req[MAX_REQ_LEN];
} tsa_request_t;
#define TSA_REQUEST_INIT { .len = 0, .req = { 0 }}

/* Timestamp response */
typedef struct {
    ts_status_t status;
    uint32_t len;
} tsa_response_header;

typedef struct {
    tsa_response_header hdr;
    unsigned char ts[MAX_TS_LEN];
} tsa_response_t;
#define TSA_RESPONSE_INIT { .hdr.status = UNKNOWN, .hdr.len = 0, .ts = { 0 } }


#define _BLACK       "30"
#define _RED         "31"
#define _GREEN       "32"
#define _YELLOW      "33"
#define _BLUE        "34"
#define _MAGENTA     "35"
#define _CYAN        "36"
#define _WHITE       "37"
#define CLR(C, str)  "\x1b["_##C"m" str "\x1b[0m"
#define BCLR(C, str) "\033[1m\033["_##C"m" str "\x1b[0m"

typedef enum {
    INFO,
    WARN,
    ERROR
} log_level_t;

void ts_log(log_level_t l, const char *fmt, ...);
void log_proxy_req(verbosity_t v, const char* msg, const proxy_request_t *req);
void log_tsa_req(verbosity_t v, const char* msg, const tsa_request_t *req);
void log_tsa_rsp(verbosity_t v, const char* msg, const tsa_response_t* rsp);

#endif /* _COMMON_H_ */
