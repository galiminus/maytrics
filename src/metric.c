#include "main.h"
#include "utils.h"

int
metric_controller_get (evhtp_request_t *        req,
                       struct maytrics *        maytrics)
{
    return (send_value_from_url (req, maytrics));
}

int
metric_controller_post (evhtp_request_t *        req,
                        struct maytrics *        maytrics)
{
#define METRICS_REGEX_MAX_MATCH 3

    regmatch_t          pmatch[METRICS_REGEX_MAX_MATCH];
    char *              user;
    char *              metric;

    json_t *            json_root;
    json_t *            json_value;
    json_error_t        json_error;
    json_t *            json_metric_string;
    char *              json_dump;

    char *              data;
    size_t              data_length;

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

    metric = strndup (&req->uri->path->full[pmatch[2].rm_so],
                      pmatch[2].rm_eo - pmatch[2].rm_so);
    if (metric == NULL) {
        log_error ("strndup() failed.")
            status = EVHTP_RES_SERVERR;
        goto free_user;
    }

    if (evbuffer_get_length (req->buffer_in) == 0) {
        status = EVHTP_RES_400;
        goto free_metric;
    }

    data = (char *)malloc (evbuffer_get_length(req->buffer_in));
    if (data == NULL) {
        log_error ("malloc() failed.")
            status = EVHTP_RES_SERVERR;
        goto free_metric;
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

    json_metric_string = json_string (metric);
    if (json_metric_string == NULL) {
        log_error ("json_string() failed.");
        status = EVHTP_RES_SERVERR;
        goto json_decref;
    }

    if (json_object_set (json_root, "metric", json_metric_string) == -1) {
        log_error ("json_object_set() failed.")
            status = EVHTP_RES_SERVERR;
        goto json_metric_string_decref;
    }

    json_dump = json_dumps (json_root, 0);
    if (json_dump == NULL) {
        log_error ("json_dumps() failed.")
            status = EVHTP_RES_SERVERR;
        goto json_metric_string_decref;
    }

    reply = redisCommand (maytrics->redis, "SET metrics:%b %b",
                          &req->uri->path->full[req->uri->path->matched_soff],
                          req->uri->path->matched_eoff - req->uri->path->matched_soff,
                          json_dump, strlen (json_dump));
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(SET) failed: %s.", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }

        status = EVHTP_RES_SERVERR;
        goto free_json_dump;
    }
    freeReplyObject (reply);

    reply = redisCommand (maytrics->redis, "GET metrics:%b",
                          user, strlen (user));

    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(GET) failed: %s.", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }

        status = EVHTP_RES_SERVERR;
        goto free_json_dump;
    }

    if (reply->type == REDIS_REPLY_NIL) {
        json_metrics_root = json_object ();
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

    if (json_object_set (json_metrics_root, metric, json_value) == -1) {
        log_error ("json_object_set() failed.");
        status = EVHTP_RES_SERVERR;
        goto json_metrics_decref;
    }

    json_metrics_dump = json_dumps (json_metrics_root, 0);
    if (json_metrics_dump == NULL) {
        log_error ("json_dumps() failed.");
        status = EVHTP_RES_SERVERR;
        goto json_metrics_decref;
    }

    reply = redisCommand (maytrics->redis, "SET metrics:%b %b",
                          user, strlen (user),
                          json_metrics_dump, strlen (json_metrics_dump));
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
    json_decref (json_metric_string);
    free (user);
    free (metric);

    return (EVHTP_RES_CREATED);

  json_metrics_decref:
    json_decref (json_metrics_root);

  free_json_dump:
    free (json_dump);

  json_metric_string_decref:
    json_decref (json_metric_string);

  json_decref:
    json_decref (json_root);

  free_data:
    free (data);

  free_metric:
    free (metric);

  free_user:
    free (user);

  error:
    return (status);
}

void
metric_controller (evhtp_request_t * req, void * _maytrics)
{
    struct maytrics *    maytrics =
        (struct maytrics *)_maytrics;

    int                         status;

    switch (req->method) {
    case htp_method_POST:
        status = metric_controller_post (req, maytrics);
        break ;

    case htp_method_GET:
        status = metric_controller_get (req, maytrics);
        break ;

    default:
        status = EVHTP_RES_METHNALLOWED;
    }

    set_metrics_comment (req, status);
    evhtp_send_reply (req, status);

    return ;
}
