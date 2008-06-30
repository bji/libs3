/** **************************************************************************
 * auth_header.c
 * 
 * Copyright 2008 Bryan Ischo <bryan@ischo.com>
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the
 *
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ************************************************************************** **/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <time.h>
#include "private.h"


static int headerle(const char *header1, const char *header2)
{
    while (1) {
        if (*header1 == ':') {
            return (*header2 == ':');
        }
        else if (*header2 == ':') {
            return 0;
        }
        else if (*header2 < *header1) {
            return 0;
        }
        else if (*header2 > *header1) {
            return 1;
        }
        header1++, header2++;
    }
}

// Replace this with merge sort eventually, it's the best stable sort.  But
// since typically the number of elements being sorted is small, it doesn't
// matter that much which sort is usedn
static void header_gnome_sort(const char **headers, int size)
{
    int i = 0;

    while (i < size) {
        if ((i == 0) || headerle(headers[i - 1], headers[i])) {
            i++;
        }
        else {
            const char *tmp = headers[i];
            headers[i] = headers[i - 1];
            headers[--i] = tmp;
        }
    }
}

// This function assumes that the 2K required to be in [buffer] is enough.
// It can assume this because XAmzHeaders was checked when it was filled in
// to ensure that all of its header contents were less than 2K total, and
// since this function removes characters from before copying them into the
// buffer, buffer is definitely going to be big enough
void canonicalize_amz_headers(char *buffer, const XAmzHeaders *xAmzHeaders)
{
    // Make a copy of the headers that will be sorted
    const char *sortedHeaders[MAX_META_HEADER_COUNT];

    memcpy(sortedHeaders, xAmzHeaders->headers,
           (xAmzHeaders->count * sizeof(sortedHeaders[0])));

    // Now sort these
    header_gnome_sort(sortedHeaders, xAmzHeaders->count);

    // Now copy this sorted list into the buffer, all the while:
    // - folding repeated headers into single lines, and
    // - folding multiple lines
    // - removing the space after the colon
    int lastHeaderLen, i;
    for (i = 0; i < xAmzHeaders->count; i++) {
        const char *header = sortedHeaders[i];
        const char *c = header;
        // If the header names are the same
        if ((i > 0) && 
            !strncmp(header, sortedHeaders[i-1], lastHeaderLen)) {
            // Just append the value, replacing the previous newline with
            // a comma
            *(buffer - 1) = ',';
            // Skip the header name and space
            c += (lastHeaderLen + 1);
        }
        else {
            // New header
            while (*c != ' ') {
                *buffer++ = *c++;
            }
            // Save the header len since it's a new header
            lastHeaderLen = c - header;
            // Skip the space
            c++;
        }
        // Now copy in the value, folding the lines
        while (*c) {
            if ((*c == '\r') && (*(c + 1) == '\n') && isblank(*(c + 2))) {
                c += 3;
                while (isblank(*c)) {
                    c++;
                }
                continue;
            }
            *buffer++ = *c++;
        }
        // Finally, add the newline
        *buffer++ = '\n';
    }

    // Terminate the buffer
    *buffer = 0;
}


void canonicalize_resource(char *buffer, S3UriStyle uriStyle,
                           const char *bucketName, const char *encodedKey,
                           const char *subResource)
{
    int len = 0;

#define append(str) len += sprintf(&(buffer[len]), str);

    if (bucketName && bucketName[0]) {
        buffer[len++] = '/';
        append(bucketName);
    }

    if (encodedKey && encodedKey[0]) {
        append(encodedKey);
    }

    if (subResource && subResource[0]) {
        buffer[len++] = '?';
        append(subResource);
    }
}


S3Status auth_header_snprintf(char *buffer, int bufferSize,
                              const char *accessKeyId,
                              const char *secretAccessKey,
                              const char *httpVerb, const char *md5,
                              const char *contentType,
                              const char *canonicalizedAmzHeaders,
                              const char *canonicalizedResource)
{
    // We allow for:
    // 17 bytes for HTTP-Verb + \n
    // 129 bytes for MD5 + \n
    // 129 bytes for Content-Type + \n
    // 129 bytes for Date + \n
    // S3_MAX_AMZ_HEADER_SIZE for CanonicalizedAmzHeaders
    // MAX_CANONICALIZED_RESOURCE_SIZE for CanonicalizedResourcer 
    char signbuf[17 + 129 + 129 + 129 + MAX_AMZ_HEADER_SIZE +
                 MAX_CANONICALIZED_RESOURCE_SIZE];
    int len = 0;

#define signbuf_append(format, ...)                             \
    do {                                                        \
        len += snprintf(&(signbuf[len]), sizeof(signbuf) - len, \
                        format, __VA_ARGS__);                   \
        if (len >= sizeof(signbuf)) {                           \
            return S3StatusHeadersTooLong;                      \
        }                                                       \
    } while (0)

    signbuf_append("%s\n", httpVerb);

    signbuf_append("%s\n", md5 ? md5 : "");

    signbuf_append("%s\n", contentType ? contentType : "");

    signbuf_append("%s", "\n"); // Date - we always use x-amz-date

    signbuf_append("%s", canonicalizedAmzHeaders);

    signbuf_append("%s", canonicalizedResource);

    unsigned int md_len;
    unsigned char md[EVP_MAX_MD_SIZE];
	
    HMAC(EVP_sha1(), secretAccessKey, strlen(secretAccessKey),
         (unsigned char *) signbuf, len, md, &md_len);

    BIO *base64 = BIO_push(BIO_new(BIO_f_base64()), BIO_new(BIO_s_mem()));
    BIO_write(base64, md, md_len);
    if (BIO_flush(base64) != 1) {
        BIO_free_all(base64);
        return S3StatusFailure;
    }
    BUF_MEM *base64mem;
    BIO_get_mem_ptr(base64, &base64mem);
    base64mem->data[base64mem->length - 1] = 0;

    snprintf(buffer, bufferSize, "Authorization: AWS %s:%s", accessKeyId,
             base64mem->data);

    BIO_free_all(base64);

    return S3StatusOK;
}
