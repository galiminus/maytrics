#include "main.h"
#include "utils.h"
#include "json.h"
#include "server.h"
#include "redis_store.h"

int
get_user (evhtp_request_t *        req,
          struct maytrics *        maytrics,
          const char *             user)
{
    json_t *            json_root;

    int                 status;

    status = redis_backend_get_user (maytrics, user, &json_root);
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
update_user (evhtp_request_t *        req,
             struct maytrics *        maytrics,
             const char *             user)
{
    char *              json_string;
    size_t              json_length;

    json_t *            json_root;

    int                 status;

    status = extract_data (req, &json_string, &json_length);
    if (status != 0) {
        log_error ("extract_data() failed.");
        goto exit;
    }

    status = parse_user_object (json_string, json_length, &json_root);
    if (status != 0) {
        log_error ("setup_metric_object() failed.");
        goto free_json_string;
    }

    status = redis_backend_store_user (maytrics, user, json_root);
    if (status != 0) {
        log_error ("redis_backend_store_metric() failed.");
        goto json_decref;
    }

    json_decref (json_root);
    free (json_string);

    return (EVHTP_RES_OK);

  json_decref:
    json_decref (json_root);

  free_json_string:
    free (json_string);

  exit:
    return (status);
}
