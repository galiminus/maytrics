#ifndef __MAYTRICS_UTILS_H__
# define __MAYTRICS_UTILS_H__

long
generate_id (struct maytrics *       maytrics);

int
set_origin (evhtp_request_t *        req,
            struct maytrics *        maytrics);

void
set_metrics_comment (evhtp_request_t *  req,
                     int                status);

#endif /* !__MAYTRICS_UTILS_H__ */
