#include "main.h"
#include "utils.h"
#include "init.h"
#include "metric_controller.h"
#include "metrics_controller.h"
#include "user_controller.h"

int
main ()
{
    evhtp_t  *                  htp;

    int                         status = 0;

    struct maytrics *           maytrics;

    evhtp_callback_t *          controller_cb;

    maytrics = (struct maytrics *)malloc (sizeof (struct maytrics));
    if (maytrics == NULL) {
        log_fatal ("malloc() failed.");
        status = 1;
        goto exit;
    }

    if (init_maytrics (maytrics) == -1) {
        log_fatal ("init_maytrics() failed.");
        status = 2;
        goto free_maytrics;
    }

    maytrics->evbase = event_base_new();
    if (maytrics->evbase == NULL) {
        log_fatal ("event_base_new() failed.");
        status = 3;
        goto free_maytrics;
    }

    if (init_redis_client (maytrics) == -1) {
        log_fatal ("init_redis_server() failed.");
        status = 4;
        goto event_base_free;
    }

    htp = evhtp_new (maytrics->evbase, NULL);
    if (htp == NULL) {
        log_fatal ("evhtp_new() failed.");
        status = 5;
        goto free_redis_client;
    }

    controller_cb = evhtp_set_regex_cb (htp, "/api/v1/(.+)/metrics.json",
                                        metrics_controller,
                                        maytrics);
    if (controller_cb == NULL) {
        log_fatal ("evhtp_set_regex_cb() failed.");
        status = 8;
        goto evhtp_free;
    }

    controller_cb = evhtp_set_regex_cb (htp, "/api/v1/(.+)/metrics/(.+).json",
                                        metric_controller,
                                        maytrics);
    if (controller_cb == NULL) {
        log_fatal("evhtp_set_regex_cb() failed.");
        status = 9;
        goto evhtp_free;
    }

    controller_cb = evhtp_set_regex_cb (htp, "/api/v1/(.+).json",
                                        user_controller,
                                        maytrics);
    if (controller_cb == NULL) {
        log_fatal ("evhtp_set_regex_cb() failed.");
        status = 7;
        goto evhtp_free;
    }

    if (evhtp_bind_socket (htp, maytrics->host, maytrics->port, 1024) != 0) {
        log_fatal ("evhtp_bind_socket(%s, %d) failed.", maytrics->host, maytrics->port);
        status = 10;
        goto evhtp_free;
    }
    log_info ("Server launched on %s:%d", maytrics->host, maytrics->port);

    if (event_base_loop (maytrics->evbase, 0) == -1) {
        log_fatal ("event_base_loop() failed.");
        status = 11;
        goto evhtp_unbind_socket;
    }

  evhtp_unbind_socket:
    evhtp_unbind_socket (htp);

  evhtp_free:
    evhtp_free (htp);

  free_redis_client:
    redisFree (maytrics->redis);

  event_base_free:
    event_base_free (maytrics->evbase);

  free_maytrics:
    free (maytrics);

  exit:
    return (status);
}
