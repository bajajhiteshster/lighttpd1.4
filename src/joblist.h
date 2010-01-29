#ifndef _JOB_LIST_H_
#define _JOB_LIST_H_

#include "base.h"

/* The joblist allows the code handling a connection to voluntarily pause its
 * execution to allow other connections to be processed. This results in
 * co-operative multitasking.
 */

int joblist_append(server *srv, connection *con);
void joblist_free(server *srv, connections *joblist);

/* The fdwaitqueue allows handler code to pause and wait when it needs a
 * file descriptor, but there are none left. The server will notify the
 * handler when a file descriptor has been closed and is now available.
 * Ideally, this would be used universally, but it isn't (see mod_cgi.c,
 * cgi_create_env(), calls to pipe()).
 */
int fdwaitqueue_append(server *srv, connection *con);
void fdwaitqueue_free(server *srv, connections *fdwaitqueue);
connection *fdwaitqueue_unshift(server *srv, connections *fdwaitqueue);

#endif
