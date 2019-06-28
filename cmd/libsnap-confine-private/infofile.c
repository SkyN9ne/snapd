/*
 * Copyright (C) 2019 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "infofile.h"

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "../libsnap-confine-private/cleanup-funcs.h"
#include "../libsnap-confine-private/error.h"
#include "../libsnap-confine-private/string-utils.h"
#include "../libsnap-confine-private/utils.h"

int sc_infofile_get_key(FILE *stream, const char *key, char **value, sc_error **err_out) {
    sc_error *err = NULL;
    size_t line_size = 0;
    char *line_buf SC_CLEANUP(sc_cleanup_string) = NULL;

    if (stream == NULL) {
        err = sc_error_init(SC_LIBSNAP_ERROR, SC_API_MISUSE, "stream cannot be NULL");
        goto out;
    }
    if (key == NULL) {
        err = sc_error_init(SC_LIBSNAP_ERROR, SC_API_MISUSE, "key cannot be NULL");
        goto out;
    }
    if (value == NULL) {
        err = sc_error_init(SC_LIBSNAP_ERROR, SC_API_MISUSE, "value cannot be NULL");
        goto out;
    }

    /* Store NULL in case we don't find the key.
     * This makes the value always well-defined. */
    *value = NULL;

    /* This loop advances through subsequent lines. */
    for (int lineno = 1;; ++lineno) {
        errno = 0;
        ssize_t nread = getline(&line_buf, &line_size, stream);
        if (nread < 0 && errno != 0) {
            err = sc_error_init_from_errno(errno, "cannot read beyond line %d", lineno);
            goto out;
        }
        if (nread <= 0) {
            break; /* There is nothing more to read. */
        }
        /* NOTE: beyond this line the buffer is never empty. */

        /* Guard against malformed input that may contain NUL bytes that
         * would confuse the code below. */
        if (memchr(line_buf, '\0', nread) != NULL) {
            err = sc_error_init(SC_LIBSNAP_ERROR, 0, "line %d contains NUL byte", lineno);
            goto out;
        }
        /* Replace the newline character, if any, with the NUL byte. */
        if (line_buf[nread - 1] == '\n') {
            line_buf[nread - 1] = '\0';
        }
        /* Guard against malformed input that does not contain '=' byte */
        char *eq_ptr = memchr(line_buf, '=', nread);
        if (eq_ptr == NULL) {
            err = sc_error_init(SC_LIBSNAP_ERROR, 0, "line %d is not a key=value assignment", lineno);
            goto out;
        }
        /* Guard against malformed input with empty key. */
        if (eq_ptr == line_buf) {
            err = sc_error_init(SC_LIBSNAP_ERROR, 0, "line %d contains empty key", lineno);
            goto out;
        }
        /* Replace the first '=' with string terminator byte. */
        *eq_ptr = '\0';

        /* If the key matches the one we are looking for, store it and stop scanning. */
        const char *scanned_key = line_buf;
        const char *scanned_value = eq_ptr + 1;
        if (sc_streq(scanned_key, key)) {
            *value = sc_strdup(scanned_value);
            break;
        }
    }

out:
    return sc_error_forward(err_out, err);
}
