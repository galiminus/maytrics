#include "main.h"

int log_level;

int
init_maytrics (struct maytrics *  maytrics)
{
    const char *        port;
    const char *        log_level_string;

    maytrics->host = getenv ("HOST");
    if (maytrics->host == NULL) {
        maytrics->host = "127.0.0.1";
    }

    port = getenv ("PORT");
    if (port == NULL) {
        port = "8081";
    }
    maytrics->port = atoi (port);

    maytrics->metrics_regex = (regex_t *)malloc (sizeof (regex_t));
    if (maytrics->metrics_regex == NULL) {
        log_fatal ("malloc() failed.");
        goto exit;
    }
    if (regcomp (maytrics->metrics_regex,
                 "/api/v1/(.+)/metrics.json",
                 REG_EXTENDED) == -1) {
        log_fatal ("regcomp() failed");
        goto free_metrics_regex;
    }

    maytrics->metric_regex = (regex_t *)malloc (sizeof (regex_t));
    if (maytrics->metric_regex == NULL) {
        log_fatal ("malloc() failed.");
        goto free_metrics_regex;
    }
    if (regcomp (maytrics->metrics_regex,
                 "/api/v1/(.+)/metrics/(.+).json",
                 REG_EXTENDED) == -1) {
        log_fatal ("regcomp() failed");
        goto free_metric_regex;
    }

    log_level_string = getenv ("LOG_LEVEL");
    if (log_level_string == NULL) {
        return (0);
    }

    if (!strcmp (log_level_string, "LOG_FATAL")) {
        log_level = LOG_FATAL;
    }
    else if (!strcmp (log_level_string, "LOG_ERROR")) {
        log_level = LOG_ERROR;
    }
    else if (!strcmp (log_level_string, "LOG_INFO")) {
        log_level = LOG_INFO;
    }
    else if (!strcmp (log_level_string, "LOG_DEBUG")) {
        log_level = LOG_DEBUG;
    }

    maytrics->allowed_origin = getenv ("ALLOWED_ORIGIN");

    SSL_library_init();
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    maytrics->ssl_ctx = SSL_CTX_new (SSLv23_method ());
    if (maytrics->ssl_ctx == NULL) {
        log_error ("SSL_CTX_new() failed.");
        goto regfree;
    }

    return (0);

  regfree:
    regfree (maytrics->metrics_regex);

  free_metric_regex:
    free (maytrics->metric_regex);

  free_metrics_regex:
    free (maytrics->metrics_regex);

  exit:
    return (-1);
}

int
init_redis_client (struct maytrics * maytrics)
{
    const char *      host;
    const char *      port;

    redisReply *	    reply;

    host = getenv ("REDIS_HOST");
    if (host == NULL) {
        host = "127.0.0.1";
    }

    port = getenv ("REDIS_PORT");
    if (port == NULL) {
        port = "6379";
    }

    maytrics->redis = redisConnect (host, atoi (port));
    if (maytrics->redis == NULL) {
        log_fatal ("redisConnect(%s, %s) failed.", host, port);
        goto exit;
    }

    if (getenv ("REDIS_PASSWORD")) {
        reply = redisCommand (maytrics->redis, "AUTH %s", getenv ("REDIS_PASSWORD"));
        if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
            log_error ("redisCommand(AUTH) failed: %s", maytrics->redis->errstr);
            if (reply) {
                freeReplyObject (reply);
            }
            goto free_redis_client;
        }
        freeReplyObject (reply);
    }

    if (getenv ("REDIS_DATABASE")) {
        reply = redisCommand (maytrics->redis, "SELECT %s", getenv ("REDIS_DATABASE"));
        if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
            log_error ("redisCommand(SELECT) failed: %s", maytrics->redis->errstr);
            if (reply) {
                freeReplyObject (reply);
            }
            goto free_redis_client;
        }
        freeReplyObject (reply);
    }

    return (0);

  free_redis_client:
    redisFree (maytrics->redis);

  exit:
    return (-1);
}
