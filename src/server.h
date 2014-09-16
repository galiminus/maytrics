#ifndef __MAYTRICS_SERVER_H__
# define __MAYTRICS_SERVER_H__

#include "main.h"

int
add_json_response (evhtp_request_t *    req,
                   json_t *             json_root);

int
extract_data (evhtp_request_t * req,
              char **           json_string,
              size_t *          json_length);

int
extract_access_token (evhtp_request_t * req,
                      struct maytrics * maytrics,
                      const char **     access_token);

#endif /* !__MAYTRICS_SERVER_H__ */
