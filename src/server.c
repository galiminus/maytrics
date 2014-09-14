#include "main.h"

int
extract_data (evhtp_request_t * req,
              char **           json_string,
              size_t *          json_length)
{
    *json_string = malloc (evbuffer_get_length(req->buffer_in));
    if (*json_string == NULL) {
        log_error ("malloc() failed.");
        return (EVHTP_RES_SERVERR);
    }

    *json_length = evbuffer_copyout (req->buffer_in, json_string,
                                     evbuffer_get_length(req->buffer_in));

    return (0);
}
