#ifndef __MAYTRICS_ACCESS_H__
# define __MAYTRICS_ACCESS_H__

#include "main.h"

int
logged_in (struct maytrics *      maytrics,
           const char *           user,
           const char *           access_token);

#endif /* !__MAYTRICS_ACCESS_H__ */
