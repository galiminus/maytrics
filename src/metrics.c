#include "main.h"
#include "utils.h"

int
metrics_controller_get (evhtp_request_t *       req,
                        struct maytrics *       maytrics)
{
    return (send_value_from_url (req, maytrics));
}

void
metrics_controller (evhtp_request_t * req, void * _maytrics)
{
    struct maytrics *    maytrics =
        (struct maytrics *)_maytrics;

    int                         status;

    switch (req->method) {
    case htp_method_HEAD:
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
