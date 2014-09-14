#include "main.h"

int
extract_data (evhtp_request_t * req,
              char **           json_string,
              size_t *          json_length)
{
    *json_length = evbuffer_get_length(req->buffer_in);
    *json_string = malloc (*json_length);
    if (*json_string == NULL) {
        log_error ("malloc() failed.");
        return (EVHTP_RES_SERVERR);
    }
    evbuffer_copyout (req->buffer_in, *json_string, *json_length);

    return (0);
}

int
add_json_response (evhtp_request_t *    req,
                   json_t *             json_root)
{
    char *         json_dump;

    json_dump = json_dumps (json_root, 0);
    if (json_root == NULL) {
        log_error ("json_dumps() failed.");
        return (EVHTP_RES_SERVERR);
    }

    evbuffer_add_printf (req->buffer_out, "%s", json_dump);
    free (json_dump);

    return (0);
}
