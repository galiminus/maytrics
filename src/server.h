#ifndef __MAYTRICS_SERVER_H__
# define __MAYTRICS_SERVER_H__

int
extract_data (evhtp_request_t * req,
              char **           json_string,
              size_t *          json_length);

#endif /* !__MAYTRICS_SERVER_H__ */
