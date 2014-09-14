#include "main.h"
#include "redis.h"

int
get_metrics (evhtp_request_t *        req,
             struct maytrics *        maytrics,
             const char *             user)
{
    return (redis_get (req, maytrics, user, NULL));
}

int
get_metric (evhtp_request_t *        req,
            struct maytrics *        maytrics,
            const char *             user,
            const char *             id)
{
    return (redis_get (req, maytrics, user, id));
}

int
create_metric (evhtp_request_t *        req,
               struct maytrics *        maytrics,
               const char *             user,
               long *                   id)
{
    return (redis_create (req, maytrics, user, id));
}

int
delete_metric (evhtp_request_t *        req,
               struct maytrics *        maytrics,
               const char *             user,
               long *                   id)
{
    (void) req;
    (void) maytrics;
    (void) user;
    (void) id;

    return (0);
}

int
update_metric (evhtp_request_t *        req,
               struct maytrics *        maytrics,
               const char *             user,
               const char *             id)
{
    (void) req;
    (void) maytrics;
    (void) user;
    (void) id;

    return (0);
}
