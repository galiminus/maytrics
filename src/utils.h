#ifndef __MAYTRICS_UTILS_H__
# define __MAYTRICS_UTILS_H__

void
set_metrics_comment (evhtp_request_t *  req,
                     int                status);

int
send_value_from_url (evhtp_request_t *        req,
                     struct maytrics *        maytrics);

#endif /* !__MAYTRICS_UTILS_H__ */
