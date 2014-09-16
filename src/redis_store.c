#include "main.h"

void
redis_backend_store_refresh_token_expire (struct maytrics *   maytrics,
                                          const char *        access_token)
{
#define REDIS_EXPIRE    "604800"
    redisReply *	reply;

    reply = redisCommand (maytrics->redis, "EXPIRE tokens:%s " REDIS_EXPIRE,
                          access_token);
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(EXPIRE) failed: %s.", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }
        return (EVHTP_RES_SERVERR);
    }
    freeReplyObject(reply);


int
redis_backend_store_access_token (struct maytrics *   maytrics,
                                  const char *        user,
                                  const char *        access_token)
{
    redisReply *	reply;

    reply = redisCommand (maytrics->redis, "SET tokens:%s %s",
                          access_token, user);
    if (reply == NULL || reply->type == REDIS_REPLY_NIL) {
        log_error ("redisCommand(SET) failed: %s", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }
        return (EVHTP_RES_SERVERR);
    }
    redis_backend_store_refresh_token_expire (maytrics, access_token);
    freeReplyObject (reply);

    return (0);
}

int
redis_backend_check_user_from_token (struct maytrics *        maytrics,
                                     const char *             access_token,
                                     const char *             user)
{
    redisReply *	reply;

    int			status;

    reply = redisCommand (maytrics->redis, "GET tokens:%s", access_token);

    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(GET) failed: %s", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }
        status = EVHTP_RES_SERVERR;
        goto exit;
    }
    if (reply->type == REDIS_REPLY_NIL) {
        status = EVHTP_RES_UNAUTH;
        goto free_reply_object;
    }
    if (strncmp (user, reply->str, reply->len)) {
        status = EVHTP_RES_UNAUTH;
        goto free_reply_object;
    }
    redis_backend_store_refresh_token_expire (maytrics, access_token);
    return (0);

  free_reply_object:
    freeReplyObject (reply);

  exit:
    return (status);
}

int
redis_backend_delete_metric (struct maytrics *        maytrics,
                             const char *             user,
                             long                     id)
{
    redisReply *	reply;

    reply = redisCommand (maytrics->redis, "DEL metrics:ids:%s:%ld",
                          user, id);
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(DEL) failed: %s", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }
        return (EVHTP_RES_SERVERR);
    }
    freeReplyObject (reply);

    return (0);
}

int
redis_backend_store_metrics (struct maytrics *  maytrics,
                             const char *       user,
                             json_t *           json_array)
{
    redisReply *	reply;
    char *              json_dump;

    int                 status;

    json_dump = json_dumps (json_array, 0);
    if (json_dump == NULL) {
        log_error ("json_dumps() failed.");
        status = EVHTP_RES_SERVERR;
        goto exit;
    }

    reply = redisCommand (maytrics->redis, "SET metrics:users:%s %s",
                          user, json_dump);
    if (reply == NULL || reply->type == REDIS_REPLY_NIL) {
        log_error ("redisCommand(SET) failed: %s", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }
        status = EVHTP_RES_SERVERR;
        goto free_json_dump;
    }
    freeReplyObject (reply);
    free (json_dump);

    return (0);

  free_json_dump:
    free (json_dump);
  exit:
    return (status);
}

int
redis_backend_store_metric_in_metrics (struct maytrics *        maytrics,
                                       const char *             user,
                                       json_t *                 json_array,
                                       json_t *                 json_root)
{
    int                 status;

    if (json_array_append (json_array, json_root) == -1) {
        log_error ("json_array_append() failed.");
        status = EVHTP_RES_SERVERR;
        goto exit;
    }

    return (redis_backend_store_metrics (maytrics, user, json_array));

  exit:
    return (status);
}

int
redis_backend_store_metric (struct maytrics *        maytrics,
                            const char *             user,
                            long                     id,
                            json_t *                 json_root)
{
    redisReply *	reply;
    char *              json_dump;

    int                 status;

    json_dump = json_dumps (json_root, 0);
    if (json_dump == NULL) {
        log_error ("json_dumps() failed.");
        status = EVHTP_RES_SERVERR;
        goto exit;
    }
    reply = redisCommand (maytrics->redis, "SET metrics:ids:%s:%ld %s",
                          user, id, json_dump);
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(SET) failed: %s.", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }
        status = EVHTP_RES_SERVERR;
        goto free_json_dump;
    }
    freeReplyObject (reply);
    free (json_dump);

    return (0);

  free_json_dump:
    free (json_dump);

  exit:
    return (status);
}

int
redis_backend_get_metric (struct maytrics *        maytrics,
                          const char *             user,
                          long                     id,
                          json_t **                json_root)
{
    redisReply *	reply;

    int			status;
    json_error_t        json_error;

    reply = redisCommand (maytrics->redis, "GET metrics:ids:%s:%ld",
                          user, id);

    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(GET) failed: %s", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }
        status = EVHTP_RES_SERVERR;
        goto exit;
    }
    if (reply->type == REDIS_REPLY_NIL) {
        status = EVHTP_RES_NOTFOUND;
        goto free_reply_object;
    }

    *json_root = json_loadb (reply->str, reply->len, 0, &json_error);
    if (*json_root == NULL) {
        log_error ("json_object() or json_loadb() failed.");
        status = EVHTP_RES_SERVERR;
        goto free_reply_object;
    }

    freeReplyObject (reply);
    return (0);

  free_reply_object:
    freeReplyObject (reply);

  exit:
    return (status);
}

int
redis_backend_get_metrics (struct maytrics *        maytrics,
                           const char *             user,
                           json_t **                json_root)
{
    redisReply *	reply;

    int                 status;
    json_error_t        json_error;

    reply = redisCommand (maytrics->redis, "GET metrics:users:%s", user);
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(GET) failed: %s.", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }
        status = EVHTP_RES_SERVERR;
        goto exit;
    }

    if (reply->type == REDIS_REPLY_NIL) {
        *json_root = json_array ();
    }
    else {
        *json_root = json_loadb (reply->str, reply->len, 0, &json_error);
    }

    if (*json_root == NULL) {
        log_error ("json_object() or json_loadb() failed.");
        status = EVHTP_RES_SERVERR;
        goto free_reply_object;
    }
    freeReplyObject (reply);

    return (0);

  free_reply_object:
    freeReplyObject (reply);

  exit:
    return (status);
}

int
redis_backend_get_user (struct maytrics *        maytrics,
                        const char *             user,
                        json_t **                json_root)
{
    redisReply *	reply;

    int			status;
    json_error_t        json_error;

    reply = redisCommand (maytrics->redis, "GET users:%s", user);

    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(GET) failed: %s", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }
        status = EVHTP_RES_SERVERR;
        goto exit;
    }
    if (reply->type == REDIS_REPLY_NIL) {
        status = EVHTP_RES_NOTFOUND;
        goto free_reply_object;
    }

    *json_root = json_loadb (reply->str, reply->len, 0, &json_error);
    if (*json_root == NULL) {
        log_error ("json_object() or json_loadb() failed.");
        status = EVHTP_RES_SERVERR;
        goto free_reply_object;
    }

    freeReplyObject (reply);
    return (0);

  free_reply_object:
    freeReplyObject (reply);

  exit:
    return (status);
}

int
redis_backend_store_user (struct maytrics *        maytrics,
                          const char *             user,
                          json_t *                 json_root)
{
    redisReply *	reply;
    char *              json_dump;

    int                 status;

    json_dump = json_dumps (json_root, 0);
    if (json_dump == NULL) {
        log_error ("json_dumps() failed.");
        status = EVHTP_RES_SERVERR;
        goto exit;
    }
    reply = redisCommand (maytrics->redis, "SET users:%s %s",
                          user, json_dump);
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(SET) failed: %s.", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }
        status = EVHTP_RES_SERVERR;
        goto free_json_dump;
    }
    freeReplyObject (reply);
    free (json_dump);

    return (0);

  free_json_dump:
    free (json_dump);

  exit:
    return (status);
}
