#include <limits.h>

#include "main.h"

long
generate_id (struct maytrics *       maytrics)
{
    redisReply *	reply;
    long long           id;

    reply = redisCommand (maytrics->redis, "INCR id");
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(INCR) failed: %s.", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }
        goto exit;
    }

    if (reply->type != REDIS_REPLY_INTEGER) {
        log_error ("redisCommand(INCR) ID is not an integer.");
        goto free_reply_object;
    }
    id = reply->integer;
    if (id > LONG_MAX) {
        log_error ("redisCommand(INCR) ID is too big.");
    }
    freeReplyObject (reply);

    return ((long) id);

  free_reply_object:
    freeReplyObject (reply);

  exit:
    return (-1);
}

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
        evbuffer_add_printf (req->buffer_out, "{\"comment\": \"%s\"}", comment);
    }
    log_debug ("%s: %d", req->uri->path->full, status);

    return ;
}
