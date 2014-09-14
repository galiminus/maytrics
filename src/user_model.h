#ifndef __MAYTRICS_USER_MODEL_H__
# define __MAYTRICS_USER_MODEL_H__

int
get_user (evhtp_request_t *        req,
          struct maytrics *        maytrics,
          const char *             user);

int
update_user (evhtp_request_t *        req,
             struct maytrics *        maytrics,
             const char *             user);

#endif /* !__MAYTRICS_USER_MODEL_H__ */
