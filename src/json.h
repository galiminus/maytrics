#ifndef __MAYTRICS_JSON_H__
# define __MAYTRICS_JSON_H__

int
remove_metric_from_array (json_t *      json_array,
                          long          id);

int
add_id_to_object (struct maytrics *     maytrics,
                  json_t *              json_root,
                  long *                id);

int
parse_metric_object (const char *       json_string,
                     size_t             json_length,
                     json_t **          json_root);

int
parse_user_object (const char *       json_string,
                   size_t             json_length,
                   json_t **          json_root);

#endif /* !__MAYTRICS_JSON_H__ */
