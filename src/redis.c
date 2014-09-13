#include "main.h"

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
