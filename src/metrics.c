#include "main.h"
#include "utils.h"

int
metrics_controller_get (evhtp_request_t *       req,
                        struct maytrics *       maytrics)
{
    int			status;
    redisReply *		reply;

    reply = redisCommand (maytrics->redis, "GET metrics:users:%b",
                          &req->uri->path->full[req->uri->path->matched_soff],
                          req->uri->path->matched_eoff - req->uri->path->matched_soff);
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
metrics_controller_post (evhtp_request_t *        req,
                         struct maytrics *        maytrics,
                         long *                   id)
{
#define METRICS_REGEX_MAX_MATCH 3

    regmatch_t          pmatch[METRICS_REGEX_MAX_MATCH];
    char *              user;

    json_t *            json_root;
    json_t *            json_id;
    json_t *            json_value;

    size_t              index;
    json_t *            json_metric;
    json_t *            json_metric_id;

    json_error_t        json_error;
    char *              json_dump;

    char *              data;
    size_t              data_length;

    const char *        metric;
    int                 value;

    int                 status;

    json_t *            json_metrics_root;
    char *              json_metrics_dump;

    redisReply *	reply;

    if (regexec (maytrics->metrics_regex,
                 req->uri->path->full,
                 METRICS_REGEX_MAX_MATCH + 1, pmatch, 0) == REG_NOMATCH) {
        log_error ("regexec() failed.")
            status = EVHTP_RES_SERVERR;
        goto error;
    }
    user = strndup (&req->uri->path->full[pmatch[1].rm_so],
                    pmatch[1].rm_eo - pmatch[1].rm_so);
    if (user == NULL) {
        log_error ("strndup() failed.")
            status = EVHTP_RES_SERVERR;
        goto error;
    }

    data = (char *)malloc (evbuffer_get_length(req->buffer_in));
    if (data == NULL) {
        log_error ("malloc() failed.")
            status = EVHTP_RES_SERVERR;
        goto free_user;
    }

    data_length = evbuffer_copyout (req->buffer_in,
                                    data,
                                    evbuffer_get_length(req->buffer_in));
    json_root = json_loadb (data, data_length, 0, &json_error);
    if (json_root == NULL) {
        log_error ("evbuffer_copyout() failed.")
            status = EVHTP_RES_400;
        goto free_data;
    }

    if (!json_is_object (json_root)) {
        status = EVHTP_RES_400;
        goto json_decref;
    }

    json_value = json_object_get (json_root, "metric");
    if (!json_is_string (json_value)) {
        status = EVHTP_RES_400;
        goto json_decref;
    }

    metric = json_string_value (json_value);
    if (strlen (metric) == 0) {
        status = EVHTP_RES_400;
        goto json_decref;
    }

    json_value = json_object_get (json_root, "value");
    if (!json_is_integer (json_value)) {
        status = EVHTP_RES_400;
        goto json_decref;
    }

    value = json_integer_value (json_value);
    if (value < 0 || value > 10) {
        status = EVHTP_RES_400;
        goto json_decref;
    }

    *id = generate_id (maytrics);
    if (*id == -1) {
        status = EVHTP_RES_SERVERR;
        goto json_decref;
    }

    json_id = json_integer (*id);
    if (json_id == NULL) {
        log_error ("json_integer() failed.");
        status = EVHTP_RES_SERVERR;
        goto json_decref;
    }

    if (json_object_set (json_root, "id", json_id) == -1) {
        log_error ("json_object_set() failed.");
        status = EVHTP_RES_SERVERR;
        goto json_id_decref;
    }

    json_dump = json_dumps (json_root, 0);
    if (json_dump == NULL) {
        log_error ("json_dumps() failed.");
        status = EVHTP_RES_SERVERR;
        goto json_id_decref;
    }
    reply = redisCommand (maytrics->redis, "SET metrics:ids:%ld %s",
                          *id, json_dump);
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(SET) failed: %s.", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }

        status = EVHTP_RES_SERVERR;
        goto free_json_dump;
    }
    freeReplyObject (reply);

    reply = redisCommand (maytrics->redis, "GET metrics:users:%s", user);
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(GET) failed: %s.", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }

        status = EVHTP_RES_SERVERR;
        goto free_json_dump;
    }

    if (reply->type == REDIS_REPLY_NIL) {
        json_metrics_root = json_array ();
    }
    else {
        json_metrics_root = json_loadb (reply->str,
                                        reply->len,
                                        0, &json_error);
    }
    freeReplyObject (reply);

    if (json_metrics_root == NULL) {
        log_error ("json_object() or json_loadb() failed.");

        status = EVHTP_RES_SERVERR;
        goto free_json_dump;
    }

    for (index = 0;
         index < json_array_size (json_metrics_root);
         index++) {
        json_metric = json_array_get (json_metrics_root, index);
        json_metric_id = json_object_get (json_metric, "id");
        if (!json_is_integer (json_metric_id)) {
            status = EVHTP_RES_SERVERR;
            goto json_metrics_decref;
        }
        if (*id == json_integer_value (json_metric_id)) {
            if (json_array_remove (json_metrics_root, index) == -1) {
                status = EVHTP_RES_SERVERR;
                goto json_metrics_decref;
            }
            break ;
        }
    }

    if (json_array_append (json_metrics_root, json_root) == -1) {
        log_error ("json_array_append() failed.");
        status = EVHTP_RES_SERVERR;
        goto json_metrics_decref;
    }

    json_metrics_dump = json_dumps (json_metrics_root, 0);
    if (json_metrics_dump == NULL) {
        log_error ("json_dumps() failed.");
        status = EVHTP_RES_SERVERR;
        goto json_metrics_decref;
    }

    reply = redisCommand (maytrics->redis, "SET metrics:users:%s %s",
                          user, json_metrics_dump);
    if (reply == NULL || reply->type == REDIS_REPLY_NIL) {
        log_error ("redisCommand(SET) failed: %s", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }

        status = EVHTP_RES_SERVERR;
        goto json_metrics_decref;
    }
    freeReplyObject (reply);

    free (json_metrics_dump);
    free (json_dump);
    free (data);
    json_decref (json_metrics_root);
    json_decref (json_root);
    json_decref (json_id);
    free (user);

    return (EVHTP_RES_CREATED);

  json_metrics_decref:
    json_decref (json_metrics_root);

  free_json_dump:
    free (json_dump);

  json_id_decref:
    json_decref (json_id);

  json_decref:
    json_decref (json_root);

  free_data:
    free (data);

  free_user:
    free (user);

  error:
    return (status);
}

void
metrics_controller (evhtp_request_t * req, void * _maytrics)
{
    struct maytrics *    maytrics =
        (struct maytrics *)_maytrics;

    int                         status;
    long                        id;

    switch (req->method) {
    case htp_method_POST:
        status = metrics_controller_post (req, maytrics, &id);
        evbuffer_add_printf (req->buffer_out, "{\"id\": %ld}", id);
        break ;

    case htp_method_HEAD:
    case htp_method_GET:
        status = metrics_controller_get (req, maytrics);
        set_metrics_comment (req, status);
        break ;

    default:
        status = EVHTP_RES_METHNALLOWED;
        set_metrics_comment (req, status);
    }

    evhtp_send_reply (req, status);

    return ;
}
