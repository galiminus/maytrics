#include "main.h"
#include "utils.h"
#include "json.h"
#include "server.h"
#include "redis_store.h"

int
redis_get (evhtp_request_t *        req,
           struct maytrics *        maytrics,
           const char *             user,
           const char *             id)
{
    int			status;
    redisReply *	reply;

    if (id) {
        reply = redisCommand (maytrics->redis, "GET metrics:users:%s:%s",
                              user, id);
    }
    else {
        reply = redisCommand (maytrics->redis, "GET metrics:users:%s",
                              user);
    }

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

    evbuffer_add_printf (req->buffer_out, "%.*s", reply->len, reply->str);
    freeReplyObject (reply);

    return (EVHTP_RES_OK);

  free_reply_object:
    freeReplyObject(reply);
  exit:
    return (status);
}

int
redis_update (evhtp_request_t *        req,
              struct maytrics *        maytrics,
              const char *             user,
              const char *             id)
{
    (void) req;
    (void) maytrics;
    (void) user;
    (void) id;
    return (0);
}

int
redis_create (evhtp_request_t *        req,
              struct maytrics *        maytrics,
              const char *             user,
              long *                   id)
{
    char *              json_string;
    size_t              json_length;

    json_t *            json_root;
    json_t *            json_metrics_root;

    int                 status;

    status = extract_data (req, &json_string, &json_length);
    if (status != 0) {
        log_error ("extract_data() failed.");
        goto exit;
    }

    status = parse_metric_object (json_string, json_length, &json_root);
    if (status != 0) {
        log_error ("setup_metric_object() failed.");
        goto free_data;
    }

    status = add_id_to_object (maytrics, json_root, id);
    if (status != 0) {
        log_error ("add_id_to_object() failed.");
        goto json_decref;
    }

    status = redis_backend_store_metric (maytrics, user, *id, json_root);
    if (status != 0) {
        log_error ("redis_backend_store_metric() failed.");
        goto json_decref;
    }

    status = redis_backend_get_metrics (maytrics, user, &json_metrics_root);
    if (status != 0) {
        log_error ("redis_backend_get_metrics() failed.");
        goto json_decref;
    }

    status = remove_metric_from_array (json_metrics_root, *id);
    if (status != 0) {
        log_error ("remove_metric_from_array() failed.");
        goto json_metrics_decref;
    }

    status = redis_backend_store_metric_in_metrics (maytrics,
                                                    user,
                                                    json_metrics_root,
                                                    json_root);
    if (status != 0) {
        log_error ("redis_backend_store_metric_in_metrics() failed.");
        goto json_metrics_decref;
    }

    json_decref (json_metrics_root);
    json_decref (json_root);
    free (json_string);

    return (EVHTP_RES_CREATED);

  json_metrics_decref:
    json_decref (json_metrics_root);

  json_decref:
    json_decref (json_root);

  free_data:
    free (json_string);

  exit:
    return (status);
}

int
redis_delete (struct maytrics *         maytrics,
              const char *              user,
              long                      id)
{
    json_t *            json_metrics_root;

    int                 status;

    status = redis_backend_delete_metric (maytrics, user, id);
    if (status != 0) {
        log_error ("redis_backend_delete_metric() failed.");
        goto exit;
    }

    status = redis_backend_get_metrics (maytrics, user, &json_metrics_root);
    if (status != 0) {
        log_error ("redis_backend_get_metrics() failed.");
        goto exit;
    }

    status = remove_metric_from_array (json_metrics_root, id);
    if (status != 0) {
        log_error ("remove_metric_from_array() failed.");
        goto json_metrics_root_decref;
    }

    status = redis_backend_store_metrics (maytrics, user, json_metrics_root);
    if (status != 0) {
        log_error ("redis_backend_store_metrics() failed.");
        goto json_metrics_root_decref;
    }
    json_decref (json_metrics_root);

    return (EVHTP_RES_OK);

  json_metrics_root_decref:
    json_decref (json_metrics_root);

  exit:
    return (status);
}
