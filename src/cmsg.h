/* cmsg.h - small helper for building bounded, truncation-safe human-
 * readable messages that embed a piece of text of arbitrary (possibly
 * untrusted) length - e.g. an error message that embeds a JSON key or
 * a file path. Never overflows the caller's buffer; truncates the
 * embedded text rather than the surrounding message shape.
 *
 * Shared by cgtest_config.c and ctestfiles.c, both of which need to
 * report specific, human-readable failures (see specification.md:
 * "error and exit ... with an appropriate message") without risking a
 * buffer overflow on attacker- or mistake-controlled input length.
 */
#ifndef CMSG_H
#define CMSG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Writes "prefix" + up to "text_len" bytes of "text" + "suffix" into
 * "buf" (capacity "bufsize"), truncating whichever piece doesn't fit
 * rather than overflowing. Always NUL-terminates if bufsize > 0; does
 * nothing if bufsize == 0. */
void cmsg_build(char *buf, size_t bufsize, const char *prefix, const char *text, size_t text_len, const char *suffix);

/* Equivalent to cmsg_build(buf, bufsize, message, "", 0, ""). */
void cmsg_set(char *buf, size_t bufsize, const char *message);

/* Returns a malloc'd, NUL-terminated copy of the first "length" bytes
 * of "text". Returns NULL only on allocation failure. */
char *cmsg_dup(const char *text, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* CMSG_H */
