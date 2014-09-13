#include "main.h"

int
set_origin (evhtp_request_t *        req,
            struct maytrics *        maytrics)
{
    evhtp_header_t *    header;
    if (!maytrics->allowed_origin) {
        return (0);
    }
    header = evhtp_header_new("Access-Control-Allow-Origin",
                              maytrics->allowed_origin, 0, 0);
    if (header == NULL) {
        return (-1);
    }
    evhtp_headers_add_header(req->headers_out, header);

    return (0);
}

void
set_metrics_comment (evhtp_request_t *  req,
                     int                status)
{
    const char *                comment = NULL;

    switch (status) {
    case EVHTP_RES_400:
        comment = "bad json format";
        break ;
    case EVHTP_RES_UNAUTH:
        comment = "invalid access token";
        break ;
    case EVHTP_RES_SERVERR:
        comment = "internal server error";
        break ;
    }
    if (comment) {
        evbuffer_add_printf (req->buffer_out, "{comment: \"%s\"}", comment);
    }
    log_debug ("%s: %d", req->uri->path->full, status);

    return ;
}

int
send_value_from_url (evhtp_request_t *        req,
                     struct maytrics *        maytrics)
{
    int			status;
    redisReply *		reply;

    reply = redisCommand (maytrics->redis, "GET metrics:%b",
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
