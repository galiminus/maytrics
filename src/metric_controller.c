#include "main.h"
#include "metric_model.h"
#include "utils.h"



int
extract_user_and_id_from_path (evhtp_request_t *       req,
                               struct maytrics *       maytrics,
                               char **                 user,
                               char **                 id)
{
#define METRICS_REGEX_MAX_MATCH 3

    regmatch_t          pmatch[METRICS_REGEX_MAX_MATCH];

    if (regexec (maytrics->metric_regex,
                 req->uri->path->full,
                 METRICS_REGEX_MAX_MATCH + 1, pmatch, 0) == REG_NOMATCH) {
        log_error ("regexec() failed.");
        goto exit;
    }
    *user = strndup (&req->uri->path->full[pmatch[1].rm_so],
                     pmatch[1].rm_eo - pmatch[1].rm_so);
    if (*user == NULL) {
        log_error ("strndup() failed.");
        goto exit;
    }

    *id = strndup (&req->uri->path->full[pmatch[2].rm_so],
                   pmatch[1].rm_eo - pmatch[2].rm_so);
    if (*id == NULL) {
        log_error ("strndup() failed.");
        goto free_user;
    }
    return (0);

  free_user:
    free (user);

  exit:
    return (-1);
}

int
metric_controller_delete (evhtp_request_t *        req,
                          struct maytrics *        maytrics)
{
    char *              user;
    char *              id;
    int                 status;

    if (extract_user_and_id_from_path (req, maytrics, &user, &id) == -1) {
        return (EVHTP_RES_SERVERR);
    }

    status = delete_metric (req, maytrics, user, id);

    free (user);
    free (id);

    return (status);
}

int
metric_controller_put (evhtp_request_t *        req,
                          struct maytrics *        maytrics)
{
    char *              user;
    char *              id;
    int                 status;

    if (extract_user_and_id_from_path (req, maytrics, &user, &id) == -1) {
        return (EVHTP_RES_SERVERR);
    }

    status = update_metric (req, maytrics, user, id);

    free (user);
    free (id);

    return (status);
}

int
metric_controller_get (evhtp_request_t *        req,
                       struct maytrics *        maytrics)
{
    char *              user;
    char *              id;
    int                 status;

    if (extract_user_and_id_from_path (req, maytrics, &user, &id) == -1) {
        return (EVHTP_RES_SERVERR);
    }

    status = get_metric (req, maytrics, user, id);

    free (user);
    free (id);

    return (status);
}

void
metric_controller (evhtp_request_t * req, void * _maytrics)
{
    struct maytrics *    maytrics =
        (struct maytrics *)_maytrics;

    int                  status;

    switch (req->method) {
    case htp_method_DELETE:
        status = metric_controller_delete (req, maytrics);
        break ;

    case htp_method_PUT:
        status = metric_controller_put (req, maytrics);
        break ;

    case htp_method_HEAD:
    case htp_method_GET:
        status = metric_controller_get (req, maytrics);
        break ;

    default:
        status = EVHTP_RES_METHNALLOWED;
    }

    set_metrics_comment (req, status);
    evhtp_send_reply (req, status);

    return ;
}
