#include <event2/bufferevent_ssl.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/http.h>

#include <evhttp.h>

#include <sys/queue.h>

#include "main.h"
#include "utils.h"

struct profile_request_data {
    struct maytrics *   maytrics;
    evhtp_request_t *   req;

    const char *        access_token;
};

void
parse_google_profile (struct evhttp_request *   response,
                      void *                    _profile_request_data)
{
#define REDIS_EXPIRE    "3600"

    int                 status;

    int                 response_size;
    char *              body;

    json_t *            json_root;
    json_t *            json_id;
    json_error_t        json_error;

    const char *        id;

    redisReply *	reply;

    struct profile_request_data *       profile_request_data =
        (struct profile_request_data *)_profile_request_data;

    struct maytrics *   maytrics = profile_request_data->maytrics;
    evhtp_request_t *   req = profile_request_data->req;

    if (evhttp_request_get_response_code (response) != 200) {
        status = evhttp_request_get_response_code (response);
        goto free_profile_request_data;
    }

    response_size = evbuffer_get_length (evhttp_request_get_input_buffer(response));
    body = malloc (response_size);
    if (body == NULL) {
        log_error ("malloc() failed.");
        status = EVHTP_RES_SERVERR;
        goto free_profile_request_data;
    }

    if (evbuffer_copyout(evhttp_request_get_input_buffer(response), body, response_size) == -1) {
        log_error ("evbuffer_remove(%d) failed.", response_size);
        status = EVHTP_RES_SERVERR;
        goto free_body;
    }

    json_root = json_loadb (body, response_size, 0, &json_error);
    if (json_root == NULL) {
        log_error ("json_loadb() failed.");
        status = EVHTP_RES_SERVERR;
        goto free_body;
    }
    json_id = json_object_get (json_root, "id");
    if (!json_is_string (json_id)) {
        status = EVHTP_RES_SERVERR;
        log_error ("ID is not a string.");
        goto json_decref;
    }

    id = json_string_value (json_id);

    reply = redisCommand (maytrics->redis, "SET tokens:%s %s",
                          profile_request_data->access_token, id);
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(SET) failed: %s.", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }

        status = EVHTP_RES_SERVERR;
        goto json_decref;
    }
    freeReplyObject(reply);

    reply = redisCommand (maytrics->redis, "EXPIRE tokens:%s " REDIS_EXPIRE,
                          profile_request_data->access_token);
    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        log_error ("redisCommand(EXPIRE) failed: %s.", maytrics->redis->errstr);
        if (reply) {
            freeReplyObject (reply);
        }

        status = EVHTP_RES_SERVERR;
        goto json_decref;
    }
    freeReplyObject(reply);

    json_decref (json_root);
    free (body);
    free (profile_request_data);

    status = EVHTP_RES_OK;

    set_metrics_comment (req, status);
    evhtp_send_reply (req, status);
    return ;

  json_decref:
    json_decref (json_root);

  free_body:
    free (body);

  free_profile_request_data:
    free (profile_request_data);

    set_metrics_comment (req, status);
    evhtp_send_reply (req, status);
    return ;
}

int
make_profile_path (const char *             access_token,
                   char **                  path)
{
#define GOOGLE_PLUS_PATH        "/plus/v1/people/me?access_token="

    int                         status = 0;


    size_t                      path_size;



    path_size = strlen (GOOGLE_PLUS_PATH) + strlen (access_token) + sizeof (char);
    *path = malloc (path_size);
    if (*path == NULL) {
        status = EVHTP_RES_SERVERR;
        goto exit;
    }
    if (snprintf (*path, path_size, "%s%s", GOOGLE_PLUS_PATH, access_token) == -1) {
        status = EVHTP_RES_SERVERR;
        goto free_path;
    }

    return (0);

  free_path:
    free (*path);

  exit:
    return (status);
}

int
access_controller_get (evhtp_request_t *        req,
                       struct maytrics *        maytrics)
{
#define GOOGLE_HOST             "www.googleapis.com"
#define GOOGLE_PORT             443

    int                         status = 0;

    char *                      path;

    struct evhttp_connection *  connection;
    struct evhttp_request *     profile_request;
    struct evkeyvalq *          output_headers;

    SSL *                       ssl;
    struct bufferevent *        buffer_event;

    struct profile_request_data *       profile_request_data;

    profile_request_data = malloc (sizeof (struct profile_request_data));
    if (profile_request_data == NULL) {
        log_error ("malloc() failed.");
        status = EVHTP_RES_SERVERR;
        goto exit;
    }
    profile_request_data->maytrics = maytrics;
    profile_request_data->req = req;

    profile_request_data->access_token =
        evhtp_kv_find (req->uri->query, "access_token");
    if (profile_request_data->access_token == NULL) {
        status = EVHTP_RES_UNAUTH;
        goto free_profile_request_data;
    }

    if ((status = make_profile_path (profile_request_data->access_token,
                                     &path)) != 0) {
        goto free_profile_request_data;
    }

    ssl = SSL_new (maytrics->ssl_ctx);
    if (ssl == NULL) {
        log_error ("SSL_new() failed.");
        status = EVHTP_RES_SERVERR;
        goto free_path;
    }

    SSL_set_tlsext_host_name (ssl, GOOGLE_HOST);

    buffer_event =
        bufferevent_openssl_socket_new (maytrics->evbase, -1, ssl,
                                        BUFFEREVENT_SSL_CONNECTING,
                                        BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
    if (buffer_event == NULL) {
        log_error ("bufferevent_openssl_socket_new() failed.");
        status = EVHTP_RES_SERVERR;
        goto ssl_free;
    }

    bufferevent_openssl_set_allow_dirty_shutdown (buffer_event, 1);

    connection = evhttp_connection_base_bufferevent_new (maytrics->evbase,
                                                         NULL,
                                                         buffer_event,
                                                         GOOGLE_HOST,
                                                         GOOGLE_PORT);
    if (connection == NULL) {
        log_error ("evhttp_connection_base_new(GOOGLE) failed.");
        status = EVHTP_RES_SERVERR;
        goto bufferevent_free;
    }

    profile_request = evhttp_request_new (parse_google_profile,
                                          profile_request_data);
    if (profile_request == NULL) {
        log_error ("evhttp_request_new() failed.");
        status = EVHTP_RES_SERVERR;
        goto evhttp_connection_free;
    }

    log_error("path %s%s\n", GOOGLE_HOST, path);

    output_headers = evhttp_request_get_output_headers (profile_request);
    evhttp_add_header(output_headers, "Host", GOOGLE_HOST);
    evhttp_add_header(output_headers, "Connection", "close");

    if (evhttp_make_request (connection, profile_request,
                             EVHTTP_REQ_GET, path) == -1) {
        status = EVHTP_RES_SERVERR;
        log_error ("evhttp_make_request() failed;");
        goto evhttp_request_free;
    }
    evhttp_connection_set_timeout(profile_request->evcon, 600);

    free (path);

    return (status);

  evhttp_request_free:
    evhttp_request_free (profile_request);

  evhttp_connection_free:
    evhttp_connection_free (connection);

  bufferevent_free:
    bufferevent_free (buffer_event);

  ssl_free:
    SSL_free (ssl);

  free_path:
    free (path);

  free_profile_request_data:
    free (profile_request_data);

  exit:
    return (status);
}

void
access_controller (evhtp_request_t * req, void * _maytrics)
{
    struct maytrics *    maytrics =
        (struct maytrics *)_maytrics;

    int                  status;

    switch (req->method) {
    case htp_method_GET:
        status = access_controller_get (req, maytrics);
        break ;

    default:
        status = EVHTP_RES_METHNALLOWED;
    }
    if (status != 0) {
        set_metrics_comment (req, status);
        evhtp_send_reply (req, status);
    }

    return ;
}
