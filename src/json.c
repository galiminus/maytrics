#include "main.h"
#include "utils.h"

int
remove_metric_from_array (json_t *      json_array,
                          long          id)
{
    size_t      index;

    json_t *    json_metric;
    json_t *    json_metric_id;

    for (index = 0;
         index < json_array_size (json_array);
         index++) {
        json_metric = json_array_get (json_array, index);
        json_metric_id = json_object_get (json_metric, "id");
        if (!json_is_integer (json_metric_id)) {
            return (EVHTP_RES_SERVERR);
        }
        if (id == json_integer_value (json_metric_id)) {
            if (json_array_remove (json_array, index) == -1) {
                return (EVHTP_RES_SERVERR);
            }
            break ;
        }
    }
    return (0);
}

int
add_id_to_object (struct maytrics *     maytrics,
                  json_t *              json_root,
                  long *                id)
{
    json_t *    json_id;

    int         status;

    *id = generate_id (maytrics);
    if (*id == -1) {
        status = EVHTP_RES_SERVERR;
        goto exit;
    }

    json_id = json_integer (*id);
    if (json_id == NULL) {
        log_error ("json_integer() failed.");
        status = EVHTP_RES_SERVERR;
        goto exit;
    }

    if (json_object_set (json_root, "id", json_id) == -1) {
        log_error ("json_object_set() failed.");
        status = EVHTP_RES_SERVERR;
        goto json_decref;
    }

    return (0);

  json_decref:
    json_decref (json_id);

  exit:
    return (status);
}


int
parse_metric_object (const char *       json_string,
                     size_t             json_length,
                     json_t **          json_root)
{
    json_t *            json_metric;
    json_t *            json_value;

    json_error_t        json_error;

    int                 status;

    const char *        metric;
    long                value;

    *json_root = json_loadb (json_string, json_length, 0, &json_error);
    if (*json_root == NULL) {
        log_error ("evbuffer_copyout() failed.");
        status = EVHTP_RES_400;
        goto exit;
    }

    if (!json_is_object (*json_root)) {
        status = EVHTP_RES_400;
        goto json_decref;
    }

    json_metric = json_object_get (*json_root, "metric");
    if (!json_is_string (json_metric)) {
        status = EVHTP_RES_400;
        goto json_decref;
    }
    metric = json_string_value (json_metric);
    if (strlen (metric) == 0) {
        status = EVHTP_RES_400;
        goto json_decref;
    }

    json_value = json_object_get (*json_root, "value");
    if (!json_is_integer (json_value)) {
        status = EVHTP_RES_400;
        goto json_decref;
    }

    value = json_integer_value (json_value);
    if (value < 0 || value > 10) {
        status = EVHTP_RES_400;
        goto json_decref;
    }

    return (0);

  json_decref:
    json_decref (*json_root);

  exit:
    return (status);
}
