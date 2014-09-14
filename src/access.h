#ifndef __MAYTRICS_ACCESS_H__
# define __MAYTRICS_ACCESS_H__

#include "main.h"

int
connected_context (evhtp_request_t *      req,
                   struct maytrics *      maytrics,
                   const char *           user,
                   const char *           access_token,
                   int (*callback)(evhtp_request_t *, struct maytrics *, int));

#endif /* !__MAYTRICS_ACCESS_H__ */
