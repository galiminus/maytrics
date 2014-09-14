#include <event2/bufferevent_ssl.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/http.h>

#include <evhttp.h>

#include <sys/queue.h>

#include "main.h"
#include "redis_store.h"
#include "utils.h"

struct profile_request_data {
    struct maytrics *   maytrics;
    evhtp_request_t *   req;

    char *              user;
    char *              access_token;

    int (*callback)(evhtp_request_t *, struct maytrics *, int);
};

void
parse_google_profile (struct evhttp_request *   response,
                      void *                    _data)
{
#define REDIS_EXPIRE    "3600"

    int                 status;

    int                 response_size;
    char *              body;

    json_t *            json_root;
    json_t *            json_id;
    json_error_t        json_error;

    const char *        id;

    struct profile_request_data *       data =
        (struct profile_request_data *)_data;

    struct maytrics *   maytrics = data->maytrics;
    evhtp_request_t *   req = data->req;

    if (evhttp_request_get_response_code (response) != 200) {
        status = evhttp_request_get_response_code (response);
        goto free_data;
    }

    response_size = evbuffer_get_length (evhttp_request_get_input_buffer (response));
    body = malloc (response_size);
    if (body == NULL) {
        log_error ("malloc() failed.");
        status = EVHTP_RES_SERVERR;
        goto free_data;
    }

    if (evbuffer_copyout(evhttp_request_get_input_buffer (response), body, response_size) == -1) {
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
    if (strcmp (id, data->user)) {
        status = EVHTP_RES_UNAUTH;
        log_error ("google ID and user ID don't match.");
        goto json_decref;
    }

    status = redis_backend_store_access_token (maytrics, data->user,
                                               data->access_token);
    if (status != 0) {
        status = EVHTP_RES_SERVERR;
        goto json_decref;
    }
    json_decref (json_root);
    free (body);
    free (data);

    status = data->callback (req, maytrics, EVHTP_RES_OK);

    set_metrics_comment (req, status);
    evhtp_send_reply (req, status);
    return ;

  json_decref:
    json_decref (json_root);

  free_body:
    free (body);

  free_data:
    free (data);

    status = data->callback (req, maytrics, status);

    set_metrics_comment (req, status);
    evhtp_send_reply (req, status);
    return ;
}

static int
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
connected_context (evhtp_request_t *      req,
                   struct maytrics *      maytrics,
                   const char *           user,
                   const char *           access_token,
                   int (*callback)(evhtp_request_t *, struct maytrics *, int))
{
#define GOOGLE_HOST             "www.googleapis.com"
#define GOOGLE_PORT             443

    struct profile_request_data *       data;
    struct evhttp_request *             profile_request;

    char *                              path;
    SSL *                               ssl;
    struct bufferevent *                buffer_event;
    int                                 status;

    struct evhttp_connection *          connection;

    struct evkeyvalq *                  output_headers;

    status = redis_backend_check_user_from_token (maytrics, access_token, user);
    if (status == 0) {
        return (callback (req, maytrics, EVHTP_RES_OK));
    }

    data = malloc (sizeof (struct profile_request_data));
    if (data == NULL) {
        log_error ("malloc() failed.");
        status = EVHTP_RES_SERVERR;
        goto exit;
    }
    data->user = strdup (user);
    if (data->user == NULL) {
        log_error ("strdup() failed.");
        status = EVHTP_RES_SERVERR;
        goto free_data;
    }
    data->access_token = strdup (access_token);
    if (data->user == NULL) {
        log_error ("strdup() failed.");
        status = EVHTP_RES_SERVERR;
        goto free_user;
    }
    data->maytrics = maytrics;
    data->req = req;
    data->callback = callback;

    if ((status = make_profile_path (data->access_token, &path)) != 0) {
        log_error ("make_profile_path() failed.");
        status = EVHTP_RES_SERVERR;
        goto free_access_token;
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

    profile_request = evhttp_request_new (parse_google_profile, data);
    if (profile_request == NULL) {
        log_error ("evhttp_request_new() failed.");
        status = EVHTP_RES_SERVERR;
        goto evhttp_connection_free;
    }

    output_headers = evhttp_request_get_output_headers (profile_request);
    evhttp_add_header(output_headers, "Host", GOOGLE_HOST);
    evhttp_add_header(output_headers, "Connection", "close");

    if (evhttp_make_request (connection, profile_request,
                             EVHTTP_REQ_GET, path) == -1) {
        log_error ("evhttp_make_request() failed;");
        status = EVHTP_RES_SERVERR;
        goto evhttp_request_free;
    }
    evhttp_connection_set_timeout(profile_request->evcon, 600);

    free (path);

    return (0);

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

  free_access_token:
    free (data->access_token);

  free_user:
    free (data->user);

  free_data:
    free (data);

  exit:
    return (status);
}

