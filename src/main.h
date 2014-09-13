#ifndef __MAYTRICS_MAIN_H__
# define __MAYTRICS_MAIN_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <evhtp.h>

#include <hiredis/hiredis.h>

#include <jansson.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

struct maytrics {
    redisContext *              redis;

    regex_t *                   metrics_regex;

    const char *                host;
    int                         port;

    const char *                db_path;

    evbase_t *                  evbase;

    SSL_CTX *                   ssl_ctx;
    SSL *                       ssl;

    const char *                allowed_origin;
};

enum {
    LOG_FATAL,
    LOG_ERROR,
    LOG_INFO,
    LOG_DEBUG
};

extern int log_level;

#define clean_errno() (errno == 0 ? "None" : strerror(errno))
#define log_fatal(M, ...) if (log_level >= LOG_FATAL) {        \
        fprintf(stdout, "[FATAL] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__); \
    }

#define log_error(M, ...) if (log_level >= LOG_ERROR) {                 \
        fprintf(stdout, "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__); \
    }

#define log_info(M, ...) if (log_level >= LOG_DEBUG) {          \
        fprintf(stdout, "[INFO] " M "\n", ##__VA_ARGS__);       \
    }

#define log_debug(M, ...) if (log_level >= LOG_DEBUG) {                 \
        fprintf(stdout, "[DEBUG] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    }

#endif /* !__MAYTRICS_MAIN_H__ */

