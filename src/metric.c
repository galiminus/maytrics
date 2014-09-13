#include "main.h"
#include "utils.h"

int
metric_controller_get (evhtp_request_t *        req,
                       struct maytrics *        maytrics)
{
    int			status;
    redisReply *		reply;

    reply = redisCommand (maytrics->redis, "GET metrics:ids:%b",
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

void
metric_controller (evhtp_request_t * req, void * _maytrics)
{
    struct maytrics *    maytrics =
        (struct maytrics *)_maytrics;

    int                         status;

    switch (req->method) {
    case htp_method_HEAD:
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
