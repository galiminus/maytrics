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

#endif /* !__MAYTRICS_SERVER_H__ */
