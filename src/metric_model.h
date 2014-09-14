#ifndef __MAYTRICS_METRIC_MODEL_H__
# define __MAYTRICS_METRIC_MODEL_H__

#include "main.h"

int
get_metrics (evhtp_request_t *        req,
             struct maytrics *        maytrics,
             const char *             user);

int
get_metric (evhtp_request_t *        req,
            struct maytrics *        maytrics,
            const char *             user,
            const char *             id);

int
delete_metric (evhtp_request_t *        req,
               struct maytrics *        maytrics,
               const char *             user,
               const char *             id);
int
create_metric (evhtp_request_t *        req,
               struct maytrics *        maytrics,
               const char *             user,
               long *                   id);

int
update_metric (evhtp_request_t *        req,
               struct maytrics *        maytrics,
               const char *             user,
               const char *             id);

#endif /* !__MAYTRICS_METRIC_MODEL_H__ */
