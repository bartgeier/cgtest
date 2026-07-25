/* cmsg.c - see cmsg.h */
#include "cmsg.h"

#include <stdlib.h>
#include <string.h>

void cmsg_build(char *buf, size_t bufsize, const char *prefix, const char *text, size_t text_len, const char *suffix)
{
    size_t pos = 0;
    size_t i;

    if (bufsize == 0) {
        return;
    }

    for (i = 0; prefix[i] != '\0' && pos + 1 < bufsize; i++) {
        buf[pos++] = prefix[i];
    }
    for (i = 0; i < text_len && pos + 1 < bufsize; i++) {
        buf[pos++] = text[i];
    }
    for (i = 0; suffix[i] != '\0' && pos + 1 < bufsize; i++) {
        buf[pos++] = suffix[i];
    }
    buf[pos] = '\0';
}

void cmsg_set(char *buf, size_t bufsize, const char *message)
{
    cmsg_build(buf, bufsize, message, "", 0, "");
}

char *cmsg_dup(const char *text, size_t length)
{
    char *copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}
