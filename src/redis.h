#ifndef __MAYTRICS_REDIS_H__
# define __MAYTRICS_REDIS_H__

int
redis_get (evhtp_request_t *        req,
           struct maytrics *        maytrics,
           const char *             user,
           const char *             id);

int
redis_update (evhtp_request_t *        req,
              struct maytrics *        maytrics,
              const char *             user,
              long *                   id);

int
redis_delete (evhtp_request_t *        req,
              struct maytrics *        maytrics);

int
redis_create (evhtp_request_t *        req,
              struct maytrics *        maytrics,
              const char *             user,
              long *                   id);

#endif /* !__MAYTRICS_REDIS_H__ */
