#include "main.h"
#include "metric_model.h"
#include "utils.h"

static int
extract_user_and_id_from_path (evhtp_request_t *       req,
                               struct maytrics *       maytrics,
                               char **                 user,
                               long *                  id)
{
#define METRICS_REGEX_MAX_MATCH 4

    regmatch_t          pmatch[METRICS_REGEX_MAX_MATCH];
    char *              id_string;

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

    id_string = strndup (&req->uri->path->full[pmatch[2].rm_so],
                         pmatch[1].rm_eo - pmatch[2].rm_so);
    if (id == NULL) {
        log_error ("strndup() failed.");
        goto free_user;
    }
    *id = atol (id_string);
    free (id_string);

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
    long                id;
    int                 status;

    if (extract_user_and_id_from_path (req, maytrics, &user, &id) == -1) {
        return (EVHTP_RES_SERVERR);
    }

    status = delete_metric (req, maytrics, user, id);

    free (user);

    return (status);
}

int
metric_controller_put (evhtp_request_t *        req,
                          struct maytrics *        maytrics)
{
    char *              user;
    long                id;
    int                 status;

    if (extract_user_and_id_from_path (req, maytrics, &user, &id) == -1) {
        return (EVHTP_RES_SERVERR);
    }

    status = update_metric (req, maytrics, user, id);

    free (user);

    return (status);
}

int
metric_controller_get (evhtp_request_t *        req,
                       struct maytrics *        maytrics)
{
    char *              user;
    long                id;
    int                 status;

    if (extract_user_and_id_from_path (req, maytrics, &user, &id) == -1) {
        return (EVHTP_RES_SERVERR);
    }

    status = get_metric (req, maytrics, user, id);

    free (user);

    return (status);
}

void
metric_controller (evhtp_request_t * req, void * _maytrics)
{
    struct maytrics *    maytrics =
        (struct maytrics *)_maytrics;

    int                  status;

    if (set_origin (req, maytrics) == -1) {
        status = EVHTP_RES_SERVERR;
        goto exit;
    }

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

    case htp_method_OPTIONS:
        status = EVHTP_RES_OK;
        break ;

    default:
        status = EVHTP_RES_METHNALLOWED;
    }

  exit:
    set_metrics_comment (req, status);
    evhtp_send_reply (req, status);

    return ;
}
