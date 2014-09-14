#ifndef __MAYTRICS_REDIS_STORE_H__
# define __MAYTRICS_REDIS_STORE_H__

#include "main.h"

int
redis_backend_delete_metric (struct maytrics *        maytrics,
                             const char *             user,
                             long                     id);

int
redis_backend_store_metrics (struct maytrics *  maytrics,
                             const char *       user,
                             json_t *           json_array);

int
redis_backend_store_metric_in_metrics (struct maytrics *        maytrics,
                                       const char *             user,
                                       json_t *                 json_array,
                                       json_t *                 json_root);

int
redis_backend_store_metric (struct maytrics *        maytrics,
                            const char *             user,
                            long                     id,
                            json_t *                 json_root);

int
redis_backend_get_metrics (struct maytrics *        maytrics,
                           const char *             user,
                           json_t **                json_root);

int
redis_backend_get_metric (struct maytrics *        maytrics,
                          const char *             user,
                          long                     id,
                          json_t **                json_root);

int
redis_backend_get_user (struct maytrics *        maytrics,
                        const char *             user,
                        json_t **                json_root);

int
redis_backend_store_user (struct maytrics *        maytrics,
                          const char *             user,
                          json_t *                 json_root);

#endif /* !__MAYTRICS_REDIS_STORE_H__ */
