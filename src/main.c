#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <evhtp.h>

#include <leveldb/c.h>
#include <jansson.h>

struct maytrics_server {
    leveldb_t *                 db;
    leveldb_options_t *         options;
    leveldb_readoptions_t *     roptions;
    leveldb_writeoptions_t *    woptions;

    regex_t *                   metrics_regex;

    const char *                host;
    int                         port;

    const char *                db_path;
};

#define clean_errno() (errno == 0 ? "None" : strerror(errno))
#define log_err(M, ...) fprintf(stderr, "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)
#define log_debug(M, ...) fprintf(stderr, "[DEBUG] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define log_info(M, ...) fprintf(stderr, "[INFO] " M "\n", ##__VA_ARGS__)

void
set_metrics_comment (evhtp_request_t *  req,
                     int                status)
{
    const char *                comment = NULL;

    switch (status) {
    case EVHTP_RES_400:
        comment = "bad json format";
        break ;
    case EVHTP_RES_SERVERR:
        comment = "internal server error";
        break ;
    }
    if (comment) {
        evbuffer_add_printf (req->buffer_out, "{comment: \"%s\"}", comment);
    }
    log_info ("%s: %d\n", req->uri->path->full, status);


    return ;
}

int
send_value_from_url (evhtp_request_t *               req,
                     struct maytrics_server *        maytrics_server)
{
    char *                      db_error = NULL;

    char *                      value;
    size_t                      value_length;

    value = leveldb_get (maytrics_server->db,
                         maytrics_server->roptions,
                         &req->uri->path->full[req->uri->path->matched_soff],
                         req->uri->path->matched_eoff - req->uri->path->matched_soff,
                         &value_length, &db_error);
    free (db_error);
    if (value == NULL) {
        return (EVHTP_RES_NOTFOUND);
    }

    evbuffer_add_printf (req->buffer_out, "%.*s", (int)value_length, value);
    free (value);

    return (EVHTP_RES_OK);
}

int
metrics_controller_get (evhtp_request_t *               req,
                        struct maytrics_server *        maytrics_server)
{
    return (send_value_from_url (req, maytrics_server));
}

int
metric_controller_get (evhtp_request_t *               req,
                       struct maytrics_server *        maytrics_server)
{
    return (send_value_from_url (req, maytrics_server));
}

int
metric_controller_post (evhtp_request_t *               req,
                        struct maytrics_server *        maytrics_server)
{
#define METRICS_REGEX_MAX_MATCH 3

    regmatch_t          pmatch[METRICS_REGEX_MAX_MATCH];
    char *              user;
    char *              metric;

    char *              db_error = NULL;

    json_t *            json_root;
    json_t *            json_value;
    json_error_t        json_error;
    json_t *            json_metric_string;
    char *              json_dump;

    char *              data;
    size_t              data_length;

    int                 value;

    int                 status;

    char *              json_metrics_value;
    size_t              json_metrics_value_length;

    json_t *            json_metrics_root;
    char *              json_metrics_dump;

    if (regexec (maytrics_server->metrics_regex,
                 req->uri->path->full,
                 METRICS_REGEX_MAX_MATCH + 1, pmatch, 0) == REG_NOMATCH) {
        status = EVHTP_RES_SERVERR;
        goto error;
    }
    user = strndup (&req->uri->path->full[pmatch[1].rm_so],
                    pmatch[1].rm_eo - pmatch[1].rm_so);
    if (user == NULL) {
        status = EVHTP_RES_SERVERR;
        goto error;
    }

    metric = strndup (&req->uri->path->full[pmatch[2].rm_so],
                      pmatch[2].rm_eo - pmatch[2].rm_so);
    if (metric == NULL) {
        status = EVHTP_RES_SERVERR;
        goto free_user;
    }

    if (evbuffer_get_length (req->buffer_in) == 0) {
        status = EVHTP_RES_400;
        goto free_metric;
    }

    data = (char *)malloc (evbuffer_get_length(req->buffer_in));
    if (data == NULL) {
        status = EVHTP_RES_SERVERR;
        goto free_metric;
    }

    data_length = evbuffer_copyout (req->buffer_in,
                                    data,
                                    evbuffer_get_length(req->buffer_in));
    json_root = json_loadb (data, data_length, 0, &json_error);
    if (json_root == NULL) {
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
        status = EVHTP_RES_SERVERR;
        goto json_decref;
    }

    if (json_object_set (json_root, "metric", json_metric_string) == -1) {
        status = EVHTP_RES_SERVERR;
        goto json_metric_string_decref;
    }

    json_dump = json_dumps (json_root, 0);
    if (json_dump == NULL) {
        status = EVHTP_RES_SERVERR;
        goto json_metric_string_decref;
    }

    leveldb_put (maytrics_server->db,
                 maytrics_server->woptions,
                 &req->uri->path->full[req->uri->path->matched_soff],
                 req->uri->path->matched_eoff - req->uri->path->matched_soff,
                 json_dump, strlen (json_dump), &db_error);
    free (db_error);

    json_metrics_value = leveldb_get (maytrics_server->db,
                                      maytrics_server->roptions,
                                      user, strlen (user),
                                      &json_metrics_value_length, &db_error);
    free (db_error);
    if (json_metrics_value == NULL) {
        json_metrics_root = json_object ();
    }
    else {
        json_metrics_root = json_loadb (json_metrics_value,
                                        json_metrics_value_length,
                                        0, &json_error);
    }
    if (json_metrics_root == NULL) {
        status = EVHTP_RES_SERVERR;

        if (json_metrics_value) {
            goto free_json_metrics_value;
        }
        else {
            goto free_json_dump;
        }
    }

    if (json_object_set (json_metrics_root, metric, json_value) == -1) {
        status = EVHTP_RES_SERVERR;
        goto json_metrics_decref;
    }

    json_metrics_dump = json_dumps (json_metrics_root, 0);
    if (json_metrics_dump == NULL) {
        status = EVHTP_RES_SERVERR;
        goto json_metrics_decref;
    }

    leveldb_put (maytrics_server->db,
                 maytrics_server->woptions,
                 user, strlen (user),
                 json_metrics_dump, strlen (json_metrics_dump), &db_error);
    free (db_error);

    free (json_metrics_value);
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

  free_json_metrics_value:
    free (json_metrics_value);

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
metrics_controller (evhtp_request_t * req, void * _maytrics_server)
{
    struct maytrics_server *    maytrics_server =
        (struct maytrics_server *)_maytrics_server;

    int                         status;
    const char *                comment = NULL;

    switch (req->method) {
    case htp_method_GET:
        status = metrics_controller_get (req, maytrics_server);
        break ;

    default:
        status = EVHTP_RES_METHNALLOWED;
    }

    set_metrics_comment (req, status);
    evhtp_send_reply (req, status);

    return ;
}

void
metric_controller (evhtp_request_t * req, void * _maytrics_server)
{
    struct maytrics_server *    maytrics_server =
        (struct maytrics_server *)_maytrics_server;

    int                         status;

    switch (req->method) {
    case htp_method_POST:
        status = metric_controller_post (req, maytrics_server);
        break ;

    case htp_method_GET:
        status = metric_controller_get (req, maytrics_server);
        break ;

    default:
        status = EVHTP_RES_METHNALLOWED;
    }

    set_metrics_comment (req, status);
    evhtp_send_reply (req, status);

    return ;
}

int
init_maytrics_server (struct maytrics_server *  maytrics_server)
{
    char *              error = NULL;
    const char *        port;

    maytrics_server->host = getenv ("HOST");
    if (maytrics_server->host == NULL) {
        maytrics_server->host = "localhost";
    }

    port = getenv ("PORT");
    if (port == NULL) {
        port = getenv ("PORT");
    }

    if (port == NULL) {
        maytrics_server->port = 8081;
    }
    else {
        maytrics_server->port = atoi (port);
    }

    maytrics_server->db_path = getenv ("DB_PATH");
    if (maytrics_server->db_path == NULL) {
        maytrics_server->db_path = "/tmp/maytrics_tmp_base";
    }

    maytrics_server->options = leveldb_options_create ();
    if (maytrics_server->options == NULL) {
        log_err ("could not create database [%s]\n", maytrics_server->db_path);
        goto exit;
    }
    free (error);

    leveldb_options_set_create_if_missing (maytrics_server->options, 1);

    maytrics_server->db = leveldb_open (maytrics_server->options, maytrics_server->db_path, &error);
    if (maytrics_server->db == NULL) {
        log_err ("could not open database [%s]\n", maytrics_server->db_path);
        goto leveldb_options_destroy;
    }

    if (error != NULL) {
        goto exit;
    }

    maytrics_server->woptions = leveldb_writeoptions_create ();
    if (maytrics_server->woptions == NULL) {
        log_err ("could not create database write options\n");
        goto leveldb_close;
    }

    maytrics_server->roptions = leveldb_readoptions_create ();
    if (maytrics_server->roptions == NULL) {
        log_err ("could not create database read options\n");
        goto leveldb_writeoptions_destroy;
    }

    maytrics_server->metrics_regex = (regex_t *)malloc (sizeof (regex_t));
    if (maytrics_server->metrics_regex == NULL) {
        goto leveldb_readoptions_destroy;
    }
    if (regcomp (maytrics_server->metrics_regex,
                 "/api/v1/(.+)/(.+).json",
                 REG_EXTENDED) == -1) {
        log_err ("fatal error in regcomp\n");
        goto free_metrics_regex;
    }

    return (0);

  free_metrics_regex:
    free (maytrics_server->metrics_regex);

  leveldb_readoptions_destroy:
    leveldb_readoptions_destroy (maytrics_server->roptions);

  leveldb_writeoptions_destroy:
    leveldb_writeoptions_destroy (maytrics_server->woptions);

  leveldb_close:
    leveldb_close (maytrics_server->db);

  leveldb_options_destroy:
    leveldb_options_destroy (maytrics_server->options);

  exit:
    return (-1);
}

int
main (int argc, char ** argv)
{
    evbase_t *                  evbase;
    evhtp_t  *                  htp;

    int                         status = 0;

    struct maytrics_server *    maytrics_server;

    evhtp_callback_t *          metrics_controller_cb;
    evhtp_callback_t *          metric_controller_cb;

    maytrics_server = (struct maytrics_server *)malloc (sizeof (struct maytrics_server));
    if (maytrics_server == NULL) {
        status = 1;
        goto exit;
    }

    if (init_maytrics_server (maytrics_server) == -1) {
        status = 2;
        goto free_maytrics_server;
    }

    evbase = event_base_new();
    if (evbase == NULL) {
        status = 3;
        goto free_maytrics_server;
    }

    htp = evhtp_new(evbase, NULL);
    if (htp == NULL) {
        status = 4;
        goto event_base_free;
    }

    metric_controller_cb = evhtp_set_regex_cb (htp, "/api/v1/(.+/.+).json",
                                               metric_controller,
                                               maytrics_server);
    if (metric_controller_cb == NULL) {
        status = 5;
        goto evhtp_free;
    }

    metrics_controller_cb = evhtp_set_regex_cb (htp, "/api/v1/(.+).json",
                                                metrics_controller,
                                                maytrics_server);
    if (metrics_controller_cb == NULL) {
        status = 7;
        goto evhtp_free;
    }

    if (evhtp_bind_socket (htp, maytrics_server->host, maytrics_server->port, 1024) != 0) {
        status = 8;
        goto evhtp_free;
    }
    printf ("Server launched on %s:%d\n", maytrics_server->host, maytrics_server->port);

    if (event_base_loop (evbase, 0) == -1) {
        status = 9;
        goto evhtp_unbind_socket;
    }

  evhtp_unbind_socket:
    evhtp_unbind_socket (htp);

  evhtp_free:
    evhtp_free (htp);

  event_base_free:
    event_base_free (evbase);

  free_maytrics_server:
    free (maytrics_server);

  exit:
    return (status);
}

