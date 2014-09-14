#include "main.h"
#include "utils.h"
#include "json.h"
#include "server.h"
#include "redis_store.h"

int
get_metrics (evhtp_request_t *        req,
             struct maytrics *        maytrics,
             const char *             user)
{
    json_t *            json_metrics_root;

    int                 status;

    status = redis_backend_get_metrics (maytrics, user, &json_metrics_root);
    if (status != 0) {
        log_error ("redis_backend_get_metrics() failed.");
        goto exit;
    }

    status = add_json_response (req, json_metrics_root);
    if (status != 0) {
        log_error ("add_json_response() failed.");
        goto json_metrics_decref;
    }
    json_decref (json_metrics_root);

    return (EVHTP_RES_OK);

  json_metrics_decref:
    json_decref (json_metrics_root);

  exit:
    return (status);
}

int
get_metric (evhtp_request_t *        req,
            struct maytrics *        maytrics,
            const char *             user,
            long                     id)
{
    json_t *            json_root;

    int                 status;

    status = redis_backend_get_metric (maytrics, user, id, &json_root);
    if (status != 0) {
        log_error ("redis_backend_get_metric() failed.");
        goto exit;
    }

    status = add_json_response (req, json_root);
    if (status != 0) {
        log_error ("add_json_response() failed.");
        goto json_metrics_decref;
    }
    json_decref (json_root);

    return (EVHTP_RES_OK);

  json_metrics_decref:
    json_decref (json_root);

  exit:
    return (status);
}

int
create_metric (evhtp_request_t *        req,
               struct maytrics *        maytrics,
               const char *             user)
{
    long                id = 0;

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
        goto free_json_string;
    }

    status = add_id_to_object (maytrics, json_root, &id);
    if (status != 0) {
        log_error ("add_id_to_object() failed.");
        goto json_decref;
    }

    status = redis_backend_store_metric (maytrics, user, id, json_root);
    if (status != 0) {
        log_error ("redis_backend_store_metric() failed.");
        goto json_decref;
    }

    status = redis_backend_get_metrics (maytrics, user, &json_metrics_root);
    if (status != 0) {
        log_error ("redis_backend_get_metrics() failed.");
        goto json_decref;
    }

    status = remove_metric_from_array (json_metrics_root, id);
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

    evbuffer_add_printf (req->buffer_out, "{\"id\": %ld}", id);

    return (EVHTP_RES_CREATED);

  json_metrics_decref:
    json_decref (json_metrics_root);

  json_decref:
    json_decref (json_root);

  free_json_string:
    free (json_string);

  exit:
    return (status);
}

int
delete_metric (evhtp_request_t *        req,
               struct maytrics *        maytrics,
               const char *             user,
               long                     id)
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

    evbuffer_add_printf (req->buffer_out, "{\"deleted\": %ld}", id);
    return (EVHTP_RES_OK);

  json_metrics_root_decref:
    json_decref (json_metrics_root);

  exit:
    return (status);
}

int
update_metric (evhtp_request_t *        req,
               struct maytrics *        maytrics,
               const char *             user,
               long                     id)
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
        goto free_json_string;
    }

    status = add_id_to_object (maytrics, json_root, &id);
    if (status != 0) {
        log_error ("add_id_to_object() failed.");
        goto json_decref;
    }

    status = redis_backend_store_metric (maytrics, user, id, json_root);
    if (status != 0) {
        log_error ("redis_backend_store_metric() failed.");
        goto json_decref;
    }

    status = redis_backend_get_metrics (maytrics, user, &json_metrics_root);
    if (status != 0) {
        log_error ("redis_backend_get_metrics() failed.");
        goto json_decref;
    }

    status = remove_metric_from_array (json_metrics_root, id);
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

    return (EVHTP_RES_OK);

  json_metrics_decref:
    json_decref (json_metrics_root);

  json_decref:
    json_decref (json_root);

  free_json_string:
    free (json_string);

  exit:
    return (status);
}
