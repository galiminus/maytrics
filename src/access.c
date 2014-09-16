#include "main.h"
#include "redis_store.h"
#include "utils.h"

static int
set_token_from_profile (struct maytrics *      maytrics,
                        const char *           json,
                        const char *           user,
                        const char *           access_token)
{
#define REDIS_EXPIRE    "3600"

    int                 status;

    json_t *            json_root;
    json_t *            json_id;
    json_error_t        json_error;

    const char *        id;

    json_root = json_loads (json, 0, &json_error);
    if (json_root == NULL) {
        log_error ("json_loadb() failed.");
        status = EVHTP_RES_SERVERR;
        goto exit;
    }
    json_id = json_object_get (json_root, "id");
    if (!json_is_string (json_id)) {
        status = EVHTP_RES_SERVERR;
        log_error ("ID is not a string.");
        goto json_decref;
    }

    id = json_string_value (json_id);
    if (strcmp (id, user)) {
        status = EVHTP_RES_UNAUTH;
        log_error ("Google ID and user ID don't match.");
        goto json_decref;
    }

    status = redis_backend_store_access_token (maytrics, user, access_token);
    if (status != 0) {
        goto json_decref;
    }
    json_decref (json_root);

    return (EVHTP_RES_OK);

  json_decref:
    json_decref (json_root);

  exit:
    return (status);
}

static int
make_curl_command (const char *             access_token,
                   char **                  path)
{
#define CURL_COMMAND            "curl "
#define GOOGLE_PLUS_PATH        "/plus/v1/people/me?access_token="
#define GOOGLE_HOST             "https://www.googleapis.com"

    int                         status = 0;
    size_t                      path_size;

    path_size = strlen (CURL_COMMAND) +
        strlen (GOOGLE_HOST) +
        strlen (GOOGLE_PLUS_PATH) +
        strlen (access_token) + sizeof (char);

    *path = malloc (path_size);
    if (*path == NULL) {
        status = EVHTP_RES_SERVERR;
        goto exit;
    }
    if (snprintf (*path, path_size, CURL_COMMAND GOOGLE_HOST "%s%s", GOOGLE_PLUS_PATH, access_token) == -1) {
        status = EVHTP_RES_SERVERR;
        goto free_path;
    }

    return (0);

  free_path:
    free (*path);

  exit:
    return (status);
}

int
logged_in (struct maytrics *      maytrics,
           const char *           user,
           const char *           access_token)
{
    char *                              curl_command;
    FILE *                              curl_stream;
    char                                json[65536];
    size_t                              bytes_read;

    int                                 status;

    if (redis_backend_check_user_from_token (maytrics, access_token, user) == 0) {
        return (EVHTP_RES_OK);
    }

    if ((status = make_curl_command (access_token, &curl_command)) != 0) {
        log_error ("make_curl_command() failed.");
        status = EVHTP_RES_SERVERR;
        goto exit;
    }
    curl_stream = popen (curl_command, "r");
    if (curl_stream == NULL) {
        log_error ("popen() failed.");
        status = EVHTP_RES_SERVERR;
        goto free_curl_command;
    }
    bytes_read = fread (json, sizeof (char), sizeof (json), curl_stream);
    if (bytes_read >= (sizeof (json) - 1) || bytes_read == 0) {
        log_error ("fread() failed.");
        status = EVHTP_RES_SERVERR;
        goto close_curl_stream;
    }
    json[bytes_read] = '\0';

    status = set_token_from_profile (maytrics, json, user, access_token);
    fclose (curl_stream);
    free (curl_command);

    return (status);

  close_curl_stream:
    fclose (curl_stream);

  free_curl_command:
    free (curl_command);

  exit:
    return (status);
}

