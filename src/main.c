#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <evhtp.h>

#include <hiredis/hiredis.h>

#include <jansson.h>

struct maytrics {
  redisContext *	      redis;

  regex_t *                   metrics_regex;

  const char *                host;
  int                         port;

  const char *                db_path;
};

enum {
  LOG_FATAL,
  LOG_ERROR,
  LOG_INFO,
  LOG_DEBUG
};

int log_level = LOG_ERROR; /* error */

#define clean_errno() (errno == 0 ? "None" : strerror(errno))
#define log_fatal(M, ...) if (log_level >= LOG_FATAL) {								\
    fprintf(stdout, "[FATAL] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__);	\
}

#define log_error(M, ...) if (log_level >= LOG_ERROR) {								\
    fprintf(stdout, "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__);	\
}

#define log_info(M, ...) if (log_level >= LOG_DEBUG) {								\
    fprintf(stdout, "[INFO] " M "\n", ##__VA_ARGS__);								\
}

#define log_debug(M, ...) if (log_level >= LOG_DEBUG) {								\
    fprintf(stdout, "[DEBUG] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__);				\
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
send_value_from_url (evhtp_request_t *               req,
                     struct maytrics *        maytrics)
{
  int			status;
  redisReply *		reply;

  reply = redisCommand (maytrics->redis, "GET %b",
			&req->uri->path->full[req->uri->path->matched_soff],
			req->uri->path->matched_eoff - req->uri->path->matched_soff);
  if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
    log_error ("redisCommand() failed: %s", maytrics->redis->errstr);
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
metrics_controller_get (evhtp_request_t *               req,
                        struct maytrics *        maytrics)
{
  return (send_value_from_url (req, maytrics));
}

int
metric_controller_get (evhtp_request_t *               req,
                       struct maytrics *        maytrics)
{
  return (send_value_from_url (req, maytrics));
}

int
metric_controller_post (evhtp_request_t *               req,
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

  redisReply *	      reply;

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
    log_error ("json_string() failed.")
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

  reply = redisCommand (maytrics->redis, "SET %b %b",
			&req->uri->path->full[req->uri->path->matched_soff],
			req->uri->path->matched_eoff - req->uri->path->matched_soff,
			json_dump, strlen (json_dump));
  if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
    log_error ("redisCommand() failed: %s", maytrics->redis->errstr);
    if (reply) {
      freeReplyObject (reply);
    }

    status = EVHTP_RES_SERVERR;
    goto free_json_dump;
  }
  freeReplyObject (reply);

  reply = redisCommand (maytrics->redis, "GET %b",
			user, strlen (user));

  if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
    log_error ("redisCommand() failed: %s", maytrics->redis->errstr);
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

  reply = redisCommand (maytrics->redis, "SET %b %b",
			user, strlen (user),
			json_metrics_dump, strlen (json_metrics_dump));
  if (reply == NULL || reply->type == REDIS_REPLY_NIL) {
    log_error ("redisCommand() failed: %s", maytrics->redis->errstr);
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
metrics_controller (evhtp_request_t * req, void * _maytrics)
{
  struct maytrics *    maytrics =
    (struct maytrics *)_maytrics;

  int                         status;

  switch (req->method) {
  case htp_method_GET:
    status = metrics_controller_get (req, maytrics);
    break ;

  default:
    status = EVHTP_RES_METHNALLOWED;
  }

  set_metrics_comment (req, status);
  evhtp_send_reply (req, status);

  return ;
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

int
init_maytrics (struct maytrics *  maytrics)
{
  const char *        port;
  const char *        log_level_string;

  maytrics->host = getenv ("HOST");
  if (maytrics->host == NULL) {
    maytrics->host = "127.0.0.1";
  }

  port = getenv ("PORT");
  if (port == NULL) {
    port = "8081";
  }
  maytrics->port = atoi (port);

  maytrics->metrics_regex = (regex_t *)malloc (sizeof (regex_t));
  if (maytrics->metrics_regex == NULL) {
    log_fatal ("malloc() failed.");  
    goto exit;
  }
  if (regcomp (maytrics->metrics_regex,
	       "/api/v1/(.+)/(.+).json",
	       REG_EXTENDED) == -1) {
    log_fatal ("regcomp() failed");
    goto free_metrics_regex;
  }

  log_level_string = getenv ("LOG_LEVEL");
  if (log_level_string == NULL) {
    return (0);
  }

  if (!strcmp (log_level_string, "LOG_FATAL")) {
    log_level = LOG_FATAL;
  }
  else if (!strcmp (log_level_string, "LOG_ERROR")) {
    log_level = LOG_ERROR;
  }
  else if (!strcmp (log_level_string, "LOG_INFO")) {
    log_level = LOG_INFO;
  }
  else if (!strcmp (log_level_string, "LOG_DEBUG")) {
    log_level = LOG_DEBUG;
  }

  return (0);

 free_metrics_regex:
  free (maytrics->metrics_regex);

 exit:
  return (-1);
}

int
init_redis_client (struct maytrics * maytrics)
{
  const char *      host;
  const char *      port;

  host = getenv ("REDIS_HOST");
  if (host == NULL) {
    host = "127.0.0.1";
  }

  port = getenv ("REDIS_PORT");
  if (port == NULL) {
    port = "6379";
  }

  maytrics->redis = redisConnect (host, atoi (port));
  if (maytrics->redis == NULL) {
    log_fatal ("redisConnect(%s, %s) failed.", host, port);
    goto exit;
  }
  return (0);

 exit:
  return (-1);
}

int
main ()
{
  evbase_t *                  evbase;
  evhtp_t  *                  htp;

  int                         status = 0;

  struct maytrics *    maytrics;

  evhtp_callback_t *          metrics_controller_cb;
  evhtp_callback_t *          metric_controller_cb;

  maytrics = (struct maytrics *)malloc (sizeof (struct maytrics));
  if (maytrics == NULL) {
    log_fatal ("malloc() failed.");
    status = 1;
    goto exit;
  }

  if (init_maytrics (maytrics) == -1) {
    log_fatal ("init_maytrics() failed.");
    status = 2;
    goto free_maytrics;
  }

  evbase = event_base_new();
  if (evbase == NULL) {
    log_fatal ("event_base_new() failed.");
    status = 3;
    goto free_maytrics;
  }

  if (init_redis_client (maytrics) == -1) {
    log_fatal ("init_redis_server() failed.");
    status = 4;
    goto event_base_free;
  }

  htp = evhtp_new(evbase, NULL);
  if (htp == NULL) {
    log_fatal ("evhtp_new() failed.");
    status = 5;
    goto free_redis_client;
  }

  metric_controller_cb = evhtp_set_regex_cb (htp, "/api/v1/(.+/.+).json",
					     metric_controller,
					     maytrics);
  if (metric_controller_cb == NULL) {
    log_fatal ("evhtp_set_regex_cb() failed.");
    status = 6;
    goto evhtp_free;
  }

  metrics_controller_cb = evhtp_set_regex_cb (htp, "/api/v1/(.+).json",
					      metrics_controller,
					      maytrics);
  if (metrics_controller_cb == NULL) {
    log_fatal ("evhtp_set_regex_cb() failed.");
    status = 7;
    goto evhtp_free;
  }

  if (evhtp_bind_socket (htp, maytrics->host, maytrics->port, 1024) != 0) {
    log_fatal ("evhtp_bind_socket(%s, %d) failed.", maytrics->host, maytrics->port);
    status = 8;
    goto evhtp_free;
  }
  log_info ("Server launched on %s:%d", maytrics->host, maytrics->port);

  if (event_base_loop (evbase, 0) == -1) {
    log_fatal ("event_base_loop() failed.");
    status = 9;
    goto evhtp_unbind_socket;
  }

 evhtp_unbind_socket:
  evhtp_unbind_socket (htp);

 evhtp_free:
  evhtp_free (htp);

 free_redis_client:
  redisFree (maytrics->redis);

 event_base_free:
  event_base_free (evbase);

 free_maytrics:
  free (maytrics);

 exit:
  return (status);
}
