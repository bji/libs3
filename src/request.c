/** **************************************************************************
 * request.c
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
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include "private.h"

#define USER_AGENT_SIZE 256
#define POOL_SIZE 32


static char userAgentG[USER_AGENT_SIZE];

static struct S3Mutex *poolMutexG;

static Request *poolG[POOL_SIZE];

static int poolCountG;

static const char *urlSafeG = "-_.!~*'()/";
static const char *hexG = "0123456789ABCDEF";


static S3Status request_headers_done(Request *request)
{
    if (request->headersCallbackMade) {
        return S3StatusOK;
    }

    // Get the http response code
    (void) curl_easy_getinfo
        (request->curl, CURLINFO_RESPONSE_CODE, &(request->httpResponseCode));
    
    // Now get the last modification time from curl, since it's easiest to
    // let curl parse it
    (void) curl_easy_getinfo(request->curl, CURLINFO_FILETIME, 
                             &(request->responseHeaders.lastModified));
    request->headersCallbackMade = 1;

    return (*(request->headersCallback))
        (&(request->responseHeaders), request->callbackData);
}


static size_t curl_header_func(void *ptr, size_t size, size_t nmemb, void *data)
{
    size_t len = size * nmemb;
    char *header = (char *) ptr;
    char *end = &(header[len]);
    Request *request = (Request *) data;
    S3ResponseHeaders *responseHeaders = &(request->responseHeaders);

    // Curl might call back the header function after the body has been
    // received, for 'chunked encoded' contents.  We don't handle this as of
    // yet, and it's not clear that it would ever be useful.
    if (request->headersCallbackMade) {
        return len;
    }

    // If we've already filled up the response headers, ignore this data.
    // This sucks, but it shouldn't happen - S3 should not be sending back
    // really long headers.
    if (request->responseHeaderStringsLen == 
        (sizeof(request->responseHeaderStrings) - 1)) {
        return len;
    }        

    // It should not be possible to have a header line less than 3 long
    if (len < 3) {
        return len;
    }

    // Skip whitespace at beginning of header; there never should be any,
    // but just to be safe
    while (isblank(*header)) {
        header++;
    }

    // The header must end in \r\n, so skip back over it, and also over any
    // trailing whitespace
    end -= 3;
    while ((end > header) && isblank(*end)) {
        end--;
    }
    if (!isblank(*end)) {
        end++;
    }

    if (end == header) {
        // totally bogus
        return len;
    }

    *end = 0;
    
    // Find the colon to split the header up
    char *c = header;
    while (*c && (*c != ':')) {
        c++;
    }
    
    int namelen = c - header;

    // Now walk c past the colon
    c++;
    // Now skip whitespace to the beginning of the value
    while (isblank(*c)) {
        c++;
    }

    // xxx todo come up with a single set of macros for allocating strings
    // from a char buffer, since this is being done in multiple places
#define set_value(field)                                                \
    do {                                                                \
        responseHeaders-> field =                                       \
            &(request->responseHeaderStrings                            \
              [request->responseHeaderStringsLen]);                     \
        while (*c && (request->responseHeaderStringsLen <               \
                      (sizeof(request->responseHeaderStrings) - 1))) {  \
            request->responseHeaderStrings                              \
                [request->responseHeaderStringsLen++] = *c++;           \
        }                                                               \
        request->responseHeaderStrings                                  \
            [request->responseHeaderStringsLen++] = 0;                  \
    } while (0)

    if (!strncmp(header, "x-amz-request-id", namelen)) {
        set_value(requestId);
    }
    else if (!strncmp(header, "x-amz-id-2", namelen)) {
        set_value(requestId2);
    }
    else if (!strncmp(header, "Content-Type", namelen)) {
        set_value(contentType);
    }
    else if (!strncmp(header, "Content-Length", namelen)) {
        request->responseHeaders.contentLength = 0;
        while (*c) {
            request->responseHeaders.contentLength *= 10;
            request->responseHeaders.contentLength += (*c++ - '0');
        }
    }
    else if (!strncmp(header, "Server", namelen)) {
        set_value(server);
    }
    else if (!strncmp(header, "ETag", namelen)) {
        set_value(eTag);
    }
    else if (!strncmp(header, "x-amz-meta-", sizeof("x-amz-meta-") - 1)) {
        // Find the name
        char *name = &(header[sizeof("x-amz-meta-")]);
        int metaNameLen = (namelen - (sizeof("x-amz-meta-") - 1));
        int valueLen = (end - c) + 1;
        int available = ((sizeof(request->responseMetaHeaderStrings) - 1) -
                         request->responseMetaHeaderStringsLen);
        if (((metaNameLen + 1) + (valueLen + 1)) > available) {
            // Can't fit it, skip it.
            return len;
        }
        // Copy name in
        S3MetaHeader *metaHeader = 
            &(request->responseMetaHeaders
              [request->responseHeaders.metaHeadersCount++]);
        metaHeader->name = &(request->responseMetaHeaderStrings
                             [request->responseMetaHeaderStringsLen]);
        while (metaNameLen--) {
            request->responseMetaHeaderStrings
                [request->responseMetaHeaderStringsLen++] = *name++;
        }
        request->responseMetaHeaderStrings
            [request->responseMetaHeaderStringsLen++] = 0;
        metaHeader->value = &(request->responseMetaHeaderStrings
                              [request->responseMetaHeaderStringsLen]);
        while (valueLen--) {
            request->responseMetaHeaderStrings
                [request->responseMetaHeaderStringsLen++] = *c++;
        }
    }

    return len;
}


static S3Status errorXmlCallback(const char *elementPath, const char *data,
                                 int dataLen, void *callbackData)
{
    Request *request = (Request *) callbackData;

#define APPEND_DATA(requestField)                                       \
    do {                                                                \
        request-> requestField##Len +=                                  \
            snprintf(&(request-> requestField                           \
                       [request-> requestField##Len]),                  \
                     sizeof(request-> requestField) -                   \
                     request-> requestField##Len - 1, "%.*s",           \
                     dataLen, data);                                    \
    } while (0)

#define RESET_FIELD(requestField, errorField) \
    request->s3Error. errorField = request-> requestField;


    if (!strcmp(elementPath, "Error")) {
        // Ignore, this is the Error element itself, we only care about subs
    }
    else if (!strcmp(elementPath, "Error/Code")) {
        APPEND_DATA(s3ErrorCode);
    }
    else if (!strcmp(elementPath, "Error/Message")) {
        APPEND_DATA(s3ErrorMessage);
        RESET_FIELD(s3ErrorMessage, message);
    }
    else if (!strcmp(elementPath, "Error/Resource")) {
        APPEND_DATA(s3ErrorResource);
        RESET_FIELD(s3ErrorResource, resource);
    }
    else if (!strcmp(elementPath, "Error/FurtherDetails")) {
        APPEND_DATA(s3ErrorFurtherDetails);
        RESET_FIELD(s3ErrorFurtherDetails, furtherDetails);
    }
    else {
        if (strncmp(elementPath, "Error/", sizeof("Error/") - 1)) {
            // If for some weird reason it's not within the Error element,
            // ignore it
            return S3StatusOK;
        }
        // OK, it's an unknown error element ... pass these back?
        // xxx todo
    }

    return S3StatusOK;
}


static size_t curl_write_func(void *ptr, size_t size, size_t nmemb, void *data)
{
    Request *request = (Request *) data;

    // Make sure that headers have been handled
    if (request_headers_done(request) != S3StatusOK) {
        return 0;
    }

    // On HTTP error, we expect to parse an HTTP error response
    if ((request->httpResponseCode < 200) || 
        (request->httpResponseCode > 299)) {
        // If we haven't set up for reading an error yet, do so
        if (!request->errorXmlParserInitialized) {
            request->errorXmlParserInitialized = 1;
            // Set up the simplexml parser
            if (simplexml_initialize
                (&(request->errorXmlParser),
                 &errorXmlCallback, request) != S3StatusOK) {
                return 0;
            }

            // Set up the S3Error that we will be returning
            request->s3ErrorCodeLen = 0;
            request->s3Error.message = 0;
            request->s3ErrorMessageLen = 0;
            request->s3Error.resource = 0;
            request->s3ErrorResourceLen = 0;
            request->s3Error.furtherDetails = 0;
            request->s3ErrorFurtherDetailsLen = 0;
        }
        
        // Now put the data into the xml parser
        size_t len = (size * nmemb);
        return ((simplexml_add(&(request->errorXmlParser), 
                               (char *) ptr, len) == S3StatusOK) ? len : 0);
    }

    // If there was a callback registered, make it
    if (request->curlWriteCallback) {
        return (*(request->curlWriteCallback))(ptr, size, nmemb, data);
    }
    // Else, consider this an error - S3 has sent back data when it was not
    // expected
    else {
        return 0;
    }
}


static size_t curl_read_func(void *ptr, size_t size, size_t nmemb, void *data)
{
    Request *request = (Request *) data;

    if (request_headers_done(request) != S3StatusOK) {
        return 0;
    }

    if (request->curlReadCallback) {
        return (*(request->curlReadCallback))(ptr, size, nmemb, data);
    }
    else {
        return 0;
    }
}


// This function 'normalizes' all x-amz-meta headers provided in
// params->requestHeaders, which means it removes all whitespace from
// them such that they all look exactly like this:
// x-amz-meta-${NAME}: ${VALUE}
// It also adds the x-amz-acl header, if necessary, and always adds the
// x-amz-date header.  It copies the raw string values into
// params->amzHeadersRaw, and creates an array of string pointers representing
// these headers in params->amzHeaders (and also sets params->amzHeadersCount
// to be the count of the total number of x-amz- headers thus created).
static S3Status compose_amz_headers(RequestParams *params)
{
    const S3RequestHeaders *headers = params->requestHeaders;

    params->amzHeadersCount = 0;
    params->amzHeadersRaw[0] = 0;
    int len = 0;

    // Append a header to amzHeaders, trimming whitespace from the end.
    // Does NOT trim whitespace from the beginning.
#define headers_append(isNewHeader, format, ...)                        \
    do {                                                                \
        if (isNewHeader) {                                              \
            params->amzHeaders[params->amzHeadersCount++] =             \
                &(params->amzHeadersRaw[len]);                          \
        }                                                               \
        len += snprintf(&(params->amzHeadersRaw[len]),                  \
                        sizeof(params->amzHeadersRaw) - len,            \
                        format, __VA_ARGS__);                           \
        if (len >= sizeof(params->amzHeadersRaw)) {                     \
            return S3StatusMetaHeadersTooLong;                          \
        }                                                               \
        while ((len > 0) && (params->amzHeadersRaw[len - 1] == ' ')) {  \
            len--;                                                      \
        }                                                               \
        params->amzHeadersRaw[len++] = 0;                               \
    } while (0)

#define header_name_tolower_copy(str, l)                                \
    do {                                                                \
        params->amzHeaders[params->amzHeadersCount++] =                 \
            &(params->amzHeadersRaw[len]);                              \
        if ((len + (l)) >= sizeof(params->amzHeadersRaw)) {             \
            return S3StatusMetaHeadersTooLong;                          \
        }                                                               \
        int todo = l;                                                   \
        while (todo--) {                                                \
            if ((*(str) >= 'A') && (*(str) <= 'Z')) {                   \
                params->amzHeadersRaw[len++] = 'a' + (*(str) - 'A');    \
            }                                                           \
            else {                                                      \
                params->amzHeadersRaw[len++] = *(str);                  \
            }                                                           \
            (str)++;                                                    \
        }                                                               \
    } while (0)

    // Check and copy in the x-amz-meta headers
    if (headers && ((params->httpRequestType == HttpRequestTypePUT) ||
                    (params->httpRequestType == HttpRequestTypeCOPY))) {
        int i;
        for (i = 0; i < headers->metaHeadersCount; i++) {
            const char *header = headers->metaHeaders[i];
            while (isblank(*header)) {
                header++;
            }
            if (strncmp(header, META_HEADER_NAME_PREFIX,
                        sizeof(META_HEADER_NAME_PREFIX) - 1)) {
                return S3StatusBadMetaHeader;
            }
            // Now find the colon
            const char *c = &(header[sizeof(META_HEADER_NAME_PREFIX)]);
            while (*c && isalnum(*c)) {
                c++;
            }
            if (*c != ':') {
                return S3StatusBadMetaHeader;
            }
            c++;
            header_name_tolower_copy(header, c - header);
            // Skip whitespace
            while (*c && isblank(*c)) {
                c++;
            }
            if (!*c) {
                return S3StatusBadMetaHeader;
            }
            // Copy in a space and then the value
            headers_append(0, " %s", c);
        }

        // Add the x-amz-acl header, if necessary
        const char *cannedAclString;
        switch (params->requestHeaders->cannedAcl) {
        case S3CannedAclPrivate:
            cannedAclString = 0;
            break;
        case S3CannedAclPublicRead:
            cannedAclString = "public-read";
            break;
        case S3CannedAclPublicReadWrite:
            cannedAclString = "public-read-write";
            break;
        default: // S3CannedAclAuthenticatedRead
            cannedAclString = "authenticated-read";
            break;
        }
        if (cannedAclString) {
            headers_append(1, "x-amz-acl: %s", cannedAclString);
        }
    }

    // Add the x-amz-date header
    time_t now = time(NULL);
    char date[64];
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
    headers_append(1, "x-amz-date: %s", date);

    if (params->httpRequestType == HttpRequestTypeCOPY) {
        // Add the x-amz-copy-source header
        if (params->requestHeaders->sourceObject &&
            params->requestHeaders->sourceObject[0]) {
            headers_append(1, "x-amz-copy-source: %s", 
                           params->requestHeaders->sourceObject);
        }
        // And the x-amz-metadata-directive header
        if (params->requestHeaders->metaDataDirective != 
            S3MetaDataDirectiveCopy) {
            headers_append(1, "%s", "x-amz-metadata-directive: REPLACE");
        }
    }

    return S3StatusOK;
}


// URL encodes the params->key value into params->urlEncodedKey
static S3Status encode_key(RequestParams *params)
{
    const char *key = params->key;
    char *buffer = params->urlEncodedKey;
    int len = 0;

    if (key) while (*key) {
        if (++len > S3_MAX_KEY_SIZE) {
            return S3StatusKeyTooLong;
        }
        const char *urlsafe = urlSafeG;
        int isurlsafe = 0;
        while (*urlsafe) {
            if (*urlsafe == *key) {
                isurlsafe = 1;
                break;
            }
            urlsafe++;
        }
        if (isurlsafe || isalnum(*key)) {
            *buffer++ = *key++;
        }
        else if (*key == ' ') {
            *buffer++ = '+';
            key++;
        }
        else {
            *buffer++ = '%';
            *buffer++ = hexG[*key / 16];
            *buffer++ = hexG[*key % 16];
            key++;
        }
    }

    *buffer = 0;

    return S3StatusOK;
}


static S3Status compose_standard_headers(RequestParams *params)
{

#define do_header(fmt, sourceField, destField, badError, tooLongError)  \
    do {                                                                \
        if (params->requestHeaders &&                                   \
            params->requestHeaders-> sourceField &&                     \
            params->requestHeaders-> sourceField[0]) {                  \
            /* Skip whitespace at beginning of val */                   \
            const char *val = params->requestHeaders-> sourceField;     \
            while (*val && isblank(*val)) {                             \
                val++;                                                  \
            }                                                           \
            if (!*val) {                                                \
                return badError;                                        \
            }                                                           \
            /* Compose header, make sure it all fit */                  \
            int len = snprintf(params-> destField,                      \
                               sizeof(params-> destField), fmt, val);   \
            if (len >= sizeof(params-> destField)) {                    \
                return tooLongError;                                    \
            }                                                           \
            /* Now remove the whitespace at the end */                  \
            while (isblank(params-> destField[len])) {                  \
                len--;                                                  \
            }                                                           \
            params-> destField[len] = 0;                                \
        }                                                               \
        else {                                                          \
            params-> destField[0] = 0;                                  \
        }                                                               \
    } while (0)


    // 1. Cache-Control
    do_header("Cache-Control: %s", cacheControl, cacheControlHeader,
              S3StatusBadCacheControl, S3StatusCacheControlTooLong);

    // 2. ContentType
    do_header("Content-Type: %s", contentType, contentTypeHeader,
              S3StatusBadContentType, S3StatusContentTypeTooLong);

    // 3. MD5
    do_header("Content-MD5: %s", md5, md5Header, S3StatusBadMD5,
              S3StatusMD5TooLong);

    // 4. Content-Disposition
    do_header("Content-Disposition: attachment; filename=\"%s\"",
              contentDispositionFilename, contentDispositionHeader,
              S3StatusBadContentDispositionFilename,
              S3StatusContentDispositionFilenameTooLong);

    // 5. ContentEncoding
    do_header("Content-Encoding: %s", contentEncoding, contentEncodingHeader,
              S3StatusBadContentEncoding, S3StatusContentEncodingTooLong);

    // 6. Expires
    if (params->requestHeaders && params->requestHeaders->expires) {
        char date[100];
        strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT",
                 gmtime(params->requestHeaders->expires));
        snprintf(params->expiresHeader, sizeof(params->expiresHeader),
                 "Expires: %s", date);
    }
    else {
        params->expiresHeader[0] = 0;
    }

    return S3StatusOK;
}


// Simple comparison function for comparing two HTTP header names that are
// embedded within an HTTP header line, returning true if header1 comes
// before header2 alphabetically, false if not
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
// matter that much which sort is used, and gnome sort is the world's simplest
// stable sort.  Added a slight twist to the standard gnome_sort - don't go
// forward +1, go forward to the last highest index considered.  This saves
// all the string comparisons that would be done "going forward", and thus
// only does the necessary string comparisons to move values back into their
// sorted position.
static void header_gnome_sort(const char **headers, int size)
{
    int i = 0, last_highest = 0;

    while (i < size) {
        if ((i == 0) || headerle(headers[i - 1], headers[i])) {
            i = ++last_highest;
        }
        else {
            const char *tmp = headers[i];
            headers[i] = headers[i - 1];
            headers[--i] = tmp;
        }
    }
}


// Canonicalizes the x-amz- headers into the canonicalizedAmzHeaders buffer
static void canonicalize_amz_headers(RequestParams *params)
{
    // Make a copy of the headers that will be sorted
    const char *sortedHeaders[MAX_META_HEADER_COUNT];

    memcpy(sortedHeaders, params->amzHeaders,
           (params->amzHeadersCount * sizeof(sortedHeaders[0])));

    // Now sort these
    header_gnome_sort(sortedHeaders, params->amzHeadersCount);

    // Now copy this sorted list into the buffer, all the while:
    // - folding repeated headers into single lines, and
    // - folding multiple lines
    // - removing the space after the colon
    int lastHeaderLen, i;
    char *buffer = params->canonicalizedAmzHeaders;
    for (i = 0; i < params->amzHeadersCount; i++) {
        const char *header = sortedHeaders[i];
        const char *c = header;
        // If the header names are the same, append the next value
        if ((i > 0) && 
            !strncmp(header, sortedHeaders[i - 1], lastHeaderLen)) {
            // Replacing the previous newline with a comma
            *(buffer - 1) = ',';
            // Skip the header name and space
            c += (lastHeaderLen + 1);
        }
        // Else this is a new header
        else {
            // Copy in everything up to the space in the ": "
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
            // If c points to a \r\n[whitespace] sequence, then fold
            // this newline out
            if ((*c == '\r') && (*(c + 1) == '\n') && isblank(*(c + 2))) {
                c += 3;
                while (isblank(*c)) {
                    c++;
                }
                // Also, what has most recently been copied into buffer amy
                // have been whitespace, and since we're folding whitespace
                // out around this newline sequence, back buffer up over
                // any whitespace it contains
                while (isblank(*(buffer - 1))) {
                    buffer--;
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


// Canonicalizes the resource into params->canonicalizedResource
static void canonicalize_resource(RequestParams *params)
{
    char *buffer = params->canonicalizedResource;
    int len = 0;

    *buffer = 0;

#define append(str) len += sprintf(&(buffer[len]), "%s", str)

    if (params->bucketName && params->bucketName[0]) {
        buffer[len++] = '/';
        append(params->bucketName);
    }

    if (params->urlEncodedKey[0]) {
        append(params->urlEncodedKey);
    }
    else {
        append("/");
    }

    if (params->subResource && params->subResource[0]) {
        append(params->subResource);
    }
}


// Convert an HttpRequestType to an HTTP Verb string
static const char *http_request_type_to_verb(HttpRequestType requestType)
{
    switch (requestType) {
    case HttpRequestTypeGET:
        return "GET";
    case HttpRequestTypeHEAD:
        return "HEAD";
    case HttpRequestTypePUT:
        return "PUT";
    case HttpRequestTypeCOPY:
        return "COPY";
    default: // HttpRequestTypeDELETE
        return "DELETE";
    }
}


// Composes the Authorization header for the request
static S3Status compose_auth_header(RequestParams *params)
{
    // We allow for:
    // 17 bytes for HTTP-Verb + \n
    // 129 bytes for MD5 + \n
    // 129 bytes for Content-Type + \n
    // 1 byte for Data + \n
    // CanonicalizedAmzHeaders & CanonicalizedResource
    char signbuf[17 + 129 + 129 + 1 + 
                 (sizeof(params->canonicalizedAmzHeaders) - 1) +
                 (sizeof(params->canonicalizedResource) - 1) + 1];
    int len = 0;

#define signbuf_append(format, ...)                             \
    len += snprintf(&(signbuf[len]), sizeof(signbuf) - len,     \
                    format, __VA_ARGS__)

    signbuf_append("%s\n", http_request_type_to_verb(params->httpRequestType));

    // For MD5 and Content-Type, use the value in the actual header, because
    // it's already been trimmed
    signbuf_append("%s\n", params->md5Header[0] ? 
                   &(params->md5Header[sizeof("Content-MD5: ") - 1]) : "");

    signbuf_append
        ("%s\n", params->contentTypeHeader[0] ? 
         &(params->contentTypeHeader[sizeof("Content-Type: ") - 1]) : "");

    signbuf_append("%s", "\n"); // Date - we always use x-amz-date

    signbuf_append("%s", params->canonicalizedAmzHeaders);

    signbuf_append("%s", params->canonicalizedResource);

    unsigned int md_len;
    unsigned char md[EVP_MAX_MD_SIZE];
	
    HMAC(EVP_sha1(), params->secretAccessKey, strlen(params->secretAccessKey),
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

    snprintf(params->authorizationHeader, sizeof(params->authorizationHeader),
             "Authorization: AWS %s:%s", params->accessKeyId, base64mem->data);

    BIO_free_all(base64);

    return S3StatusOK;
}


// Compose the URI to use for the request given the request parameters
S3Status compose_uri(Request *request, const RequestParams *params)
{
    int len = 0;

#define uri_append(fmt, ...)                            \
    do {                                                \
        len += snprintf(&(request->uri[len]),           \
                        sizeof(request->uri) - len,     \
                        fmt, __VA_ARGS__);              \
        if (len >= sizeof(request->uri)) {              \
            return S3StatusUriTooLong;                  \
        }                                               \
    } while (0)

    uri_append("http%s://", (params->protocol == S3ProtocolHTTP) ? "" : "s");

    if (params->bucketName && params->bucketName[0]) {
        if (params->uriStyle == S3UriStyleVirtualHost) {
            uri_append("%s.s3.amazonaws.com", params->bucketName);
        }
        else {
            uri_append("s3.amazonaws.com/%s", params->bucketName);
        }
    }
    else {
        uri_append("%s", "s3.amazonaws.com");
    }

    if (params->key && params->key[0]) {
        uri_append("%s", params->key);
        
        if (params->queryParams) {
            uri_append("%s", params->queryParams);
        }
        else if (params->subResource && params->subResource[0]) {
            uri_append("%s", params->subResource);
        }
    }
    else {
        uri_append("%s", "/");
        if (params->subResource) {
            uri_append("%s", params->subResource);
        }
    }

    return S3StatusOK;
}


// Sets up the curl handle given the completely computed RequestParams
static S3Status setup_curl(CURL *handle, Request *request,
                           const RequestParams *params)
{
    CURLcode status;
    
#define curl_easy_setopt_safe(opt, val)                                 \
    if ((status = curl_easy_setopt(handle, opt, val)) != CURLE_OK) {    \
        return S3StatusFailedToInitializeRequest;                       \
    }

    // Debugging only
    // curl_easy_setopt_safe(CURLOPT_VERBOSE, 1);

    // Always set header callback and data
    curl_easy_setopt_safe(CURLOPT_HEADERDATA, request);
    curl_easy_setopt_safe(CURLOPT_HEADERFUNCTION, &curl_header_func);
    
    // Set write callback and data
    request->curlWriteCallback = params->curlWriteCallback;
    curl_easy_setopt_safe(CURLOPT_WRITEFUNCTION, &curl_write_func);
    curl_easy_setopt_safe(CURLOPT_WRITEDATA, request);

    // Set read callback, data, and readSize
    request->curlReadCallback = params->curlReadCallback;
    curl_easy_setopt_safe(CURLOPT_READFUNCTION, &curl_read_func);
    curl_easy_setopt_safe(CURLOPT_READDATA, request);
    curl_easy_setopt_safe(CURLOPT_INFILESIZE_LARGE, params->readSize);
    
    // Ask curl to parse the Last-Modified header
    curl_easy_setopt_safe(CURLOPT_FILETIME, 1);

    // Curl docs suggest that this is necessary for multithreaded code.
    // However, it also points out that DNS timeouts will not be honored
    // during DNS lookup, which can be worked around by using the c-ares
    // library, which we do not do yet.
    curl_easy_setopt_safe(CURLOPT_NOSIGNAL, 1);

    // Turn off Curl's built-in progress meter
    curl_easy_setopt_safe(CURLOPT_NOPROGRESS, 1);

    // xxx todo - support setting the proxy for Curl to use (can't use https
    // for proxies though)

    // xxx todo - support setting the network interface for Curl to use

    // I think this is useful - we don't need interactive performance, we need
    // to complete large operations quickly
    curl_easy_setopt_safe(CURLOPT_TCP_NODELAY, 1);
    
    // Don't use Curl's 'netrc' feature
    curl_easy_setopt_safe(CURLOPT_NETRC, CURL_NETRC_IGNORED);

    // Follow any redirection directives that S3 sends
    curl_easy_setopt_safe(CURLOPT_FOLLOWLOCATION, 1);

    // A safety valve in case S3 goes bananas with redirects
    curl_easy_setopt_safe(CURLOPT_MAXREDIRS, 10);

    // Set the User-Agent; maybe Amazon will track these?
    curl_easy_setopt_safe(CURLOPT_USERAGENT, userAgentG);

    // Set the low speed limit and time; we abort transfers that stay at
    // less than 1K per second for more than 15 seconds.
    // xxx todo - make these configurable
    // xxx todo - allow configurable max send and receive speed
    curl_easy_setopt_safe(CURLOPT_LOW_SPEED_LIMIT, 1024);
    curl_easy_setopt_safe(CURLOPT_LOW_SPEED_TIME, 15);

    // Tell Curl to keep up to POOL_SIZE / 2 connections open at once
    curl_easy_setopt_safe(CURLOPT_MAXCONNECTS, POOL_SIZE / 2);

    // Append standard headers
#define append_standard_header(fieldName)                               \
    if (params-> fieldName [0]) {                                       \
        request->headers = curl_slist_append(request->headers,          \
                                          params-> fieldName);          \
    }

    append_standard_header(cacheControlHeader);
    append_standard_header(contentTypeHeader);
    append_standard_header(md5Header);
    append_standard_header(contentDispositionHeader);
    append_standard_header(contentEncodingHeader);
    append_standard_header(expiresHeader);
    append_standard_header(authorizationHeader);

    // Append x-amz- headers
    int i;
    for (i = 0; i < params->amzHeadersCount; i++) {
        request->headers = 
            curl_slist_append(request->headers, params->amzHeaders[i]);
    }

    // Set the HTTP headers
    curl_easy_setopt_safe(CURLOPT_HTTPHEADER, request->headers);

    // Set URI
    curl_easy_setopt_safe(CURLOPT_URL, request->uri);

    // Set request type
    switch (params->httpRequestType) {
    case HttpRequestTypeHEAD:
	curl_easy_setopt_safe(CURLOPT_NOBODY, 1);
        break;
    case HttpRequestTypePUT:
        curl_easy_setopt_safe(CURLOPT_UPLOAD, 1);
        break;
    case HttpRequestTypeCOPY:
	curl_easy_setopt_safe(CURLOPT_CUSTOMREQUEST, "COPY");
        break;
    case HttpRequestTypeDELETE:
	curl_easy_setopt_safe(CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
    default: // HttpRequestTypeGET
        break;
    }
    
    return S3StatusOK;
}


static void request_destroy(Request *request)
{
    if (request->headers) {
        curl_slist_free_all(request->headers);
    }
    
    curl_easy_cleanup(request->curl);

    free(request);
}


static S3Status request_initialize(Request *request, 
                                   const RequestParams *params)
{
    if (request->used) {
        // Reset the CURL handle for reuse
        curl_easy_reset(request->curl);
        
        // Free the headers
        if (request->headers) {
            curl_slist_free_all(request->headers);
        }

        // Deinitialize the error xml parser
        if (request->errorXmlParserInitialized) {
            simplexml_deinitialize(&(request->errorXmlParser));
        }
    }
    else {
        request->used = 1;
    }
                        
    // This must be done before any error is returned
    request->headers = 0;

    // Compute the URL
    S3Status status;
    if ((status = compose_uri(request, params)) != S3StatusOK) {
        return status;
    }

    // Set all of the curl handle options
    if ((status = setup_curl(request->curl, request, params)) != S3StatusOK) {
        return status;
    }

    // Now set up for receiving the response
    S3ResponseHandler *handler = params->handler;

    request->callbackData = params->callbackData;

    request->responseHeaderStringsLen = 0;

    request->responseMetaHeaderStringsLen = 0;

    request->headersCallback = handler->headersCallback;

    request->responseHeaders.requestId = 0;
    
    request->responseHeaders.requestId2 = 0;

    request->responseHeaders.contentType = 0;

    request->responseHeaders.contentLength = -1; // -1 means not supplied

    request->responseHeaders.server = 0;

    request->responseHeaders.eTag = 0;

    request->responseHeaders.lastModified = 0;

    request->responseHeaders.metaHeadersCount = 0;

    request->responseHeaders.metaHeaders = request->responseMetaHeaders;
    
    request->headersCallbackMade = 0;

    request->httpResponseCode = 0;

    request->completeCallback = handler->completeCallback;

    request->errorXmlParserInitialized = 0;

    memcpy(&(request->u), &(params->u), sizeof(request->u));

    return S3StatusOK;
}


static S3Status request_get(const RequestParams *params, Request **reqReturn)
{
    Request *request = 0;
    
    // Try to get one from the pool.  We hold the lock for the shortest time
    // possible here.
    mutex_lock(poolMutexG);

    if (poolCountG) {
        request = poolG[poolCountG--];
    }
    
    mutex_unlock(poolMutexG);

    // If there wasn't one available in the pool, create one
    if (!request) {
        if (!(request = (Request *) malloc(sizeof(Request)))) {
            return S3StatusFailedToCreateRequest;
        }
        request->used = 0;
        if (!(request->curl = curl_easy_init())) {
            free(request);
            return S3StatusFailedToInitializeRequest;
        }
    }

    // Initialize it
    S3Status status;
    if ((status = request_initialize(request, params)) != S3StatusOK) {
        request_destroy(request);
        return status;
    }

    *reqReturn = request;
    
    return S3StatusOK;
}


static void request_release(Request *request)
{
    mutex_lock(poolMutexG);

    // If the pool is full, destroy this one
    if (poolCountG == POOL_SIZE) {
        mutex_unlock(poolMutexG);
        request_destroy(request);
    }
    // Else put this one at the front of the pool; we do this because we want
    // the most-recently-used curl handle to be re-used on the next request,
    // to maximize our chances of re-using a TCP connection before its HTTP
    // keep-alive times out
    else {
        poolG[poolCountG++] = request;
        mutex_unlock(poolMutexG);
    }
}


S3Status request_api_initialize(const char *userAgentInfo)
{
    if (!(poolMutexG = mutex_create())) {
        return S3StatusFailedToCreateMutex;
    }

    poolCountG = 0;

    if (!userAgentInfo || !*userAgentInfo) {
        userAgentInfo = "Unknown";
    }

    char platform[96];
    struct utsname utsn;
    if (uname(&utsn)) {
        strncpy(platform, "Unknown", sizeof(platform));
        // Because strncpy doesn't always zero terminate
        platform[sizeof(platform) - 1] = 0;
    }
    else {
        snprintf(platform, sizeof(platform), "%s %s", utsn.sysname, 
                 utsn.machine);
    }

    snprintf(userAgentG, sizeof(userAgentG), 
             "Mozilla/4.0 (Compatible; %s; libs3 %d.%d; %s)",
             userAgentInfo, LIBS3_VER_MAJOR, LIBS3_VER_MINOR, platform);
    
    return S3StatusOK;
}


void request_api_deinitialize()
{
    mutex_destroy(poolMutexG);

    while (poolCountG--) {
        request_destroy(poolG[poolCountG]);
    }
}


S3Status request_perform(RequestParams *params, S3RequestContext *context)
{
    Request *request;
    S3Status status;

    // Validate the bucket name
    if (params->bucketName && 
        ((status = S3_validate_bucket_name(params->bucketName,
                                           params->uriStyle)) != S3StatusOK)) {
        return status;
    }

    // Compose the amz headers
    if ((status = compose_amz_headers(params)) != S3StatusOK) {
        return status;
    }

    // Compose standard headers
    if ((status = compose_standard_headers(params)) != S3StatusOK) {
        return status;
    }

    // URL encode the key
    if ((status = encode_key(params)) != S3StatusOK) {
        return status;
    }

    // Compute the canonicalized amz headers
    canonicalize_amz_headers(params);

    // Compute the canonicalized resource
    canonicalize_resource(params);

    // Compose Authorization header
    if ((status = compose_auth_header(params)) != S3StatusOK) {
        return status;
    }
    
    // Get an initialized Request structure now
    if ((status = request_get(params, &request)) != S3StatusOK) {
        return status;
    }

    // If a RequestContext was provided, add the request to the curl multi
    if (context) {
        switch (curl_multi_add_handle(context->curlm, request->curl)) {
        case CURLM_OK:
            return S3StatusOK;
        default:
            request_release(request);
            // xxx todo - more specific errors
            return S3StatusFailure;
        }
    }
    // Else, perform the request immediately
    else {
        switch (curl_easy_perform(request->curl)) {
        case CURLE_OK:
            status = S3StatusOK;
            break;
        default:
            // xxx todo - more specific errors
            status = S3StatusFailure;
            break;
        }
        // Finish the request, ensuring that all callbacks have been made, and
        // also releases the request
        request_finish(request, status);
        return S3StatusOK;
    }
}


void request_finish(Request *request, S3Status status)
{
    request_headers_done(request);

    // Convert the error status string into a code
    if (request->errorXmlParserInitialized) {
        if (request->s3ErrorCodeLen) {
#define HANDLE_CODE(name)                                       \
            do {                                                \
                if (!strcmp(request->s3ErrorCode, #name)) {     \
                    request->s3Error.code = S3ErrorCode##name;  \
                    goto code_set;                              \
                }                                               \
            } while (0)

            HANDLE_CODE(AccessDenied);
            HANDLE_CODE(AccountProblem);
            HANDLE_CODE(AmbiguousGrantByEmailAddress);
            HANDLE_CODE(BadDigest);
            HANDLE_CODE(BucketAlreadyExists);
            HANDLE_CODE(BucketAlreadyOwnedByYou);
            HANDLE_CODE(BucketNotEmpty);
            HANDLE_CODE(CredentialsNotSupported);
            HANDLE_CODE(CrossLocationLoggingProhibited);
            HANDLE_CODE(EntityTooSmall);
            HANDLE_CODE(EntityTooLarge);
            HANDLE_CODE(ExpiredToken);
            HANDLE_CODE(IncompleteBody);
            HANDLE_CODE(IncorrectNumberOfFilesInPostRequest);
            HANDLE_CODE(InlineDataTooLarge);
            HANDLE_CODE(InternalError);
            HANDLE_CODE(InvalidAccessKeyId);
            HANDLE_CODE(InvalidAddressingHeader);
            HANDLE_CODE(InvalidArgument);
            HANDLE_CODE(InvalidBucketName);
            HANDLE_CODE(InvalidDigest);
            HANDLE_CODE(InvalidLocationConstraint);
            HANDLE_CODE(InvalidPayer);
            HANDLE_CODE(InvalidPolicyDocument);
            HANDLE_CODE(InvalidRange);
            HANDLE_CODE(InvalidSecurity);
            HANDLE_CODE(InvalidSOAPRequest);
            HANDLE_CODE(InvalidStorageClass);
            HANDLE_CODE(InvalidTargetBucketForLogging);
            HANDLE_CODE(InvalidToken);
            HANDLE_CODE(InvalidURI);
            HANDLE_CODE(KeyTooLong);
            HANDLE_CODE(MalformedACLError);
            HANDLE_CODE(MalformedXML);
            HANDLE_CODE(MaxMessageLengthExceeded);
            HANDLE_CODE(MaxPostPreDataLengthExceededError);
            HANDLE_CODE(MetadataTooLarge);
            HANDLE_CODE(MethodNotAllowed);
            HANDLE_CODE(MissingAttachment);
            HANDLE_CODE(MissingContentLength);
            HANDLE_CODE(MissingSecurityElement);
            HANDLE_CODE(MissingSecurityHeader);
            HANDLE_CODE(NoLoggingStatusForKey);
            HANDLE_CODE(NoSuchBucket);
            HANDLE_CODE(NoSuchKey);
            HANDLE_CODE(NotImplemented);
            HANDLE_CODE(NotSignedUp);
            HANDLE_CODE(OperationAborted);
            HANDLE_CODE(PermanentRedirect);
            HANDLE_CODE(PreconditionFailed);
            HANDLE_CODE(Redirect);
            HANDLE_CODE(RequestIsNotMultiPartContent);
            HANDLE_CODE(RequestTimeout);
            HANDLE_CODE(RequestTimeTooSkewed);
            HANDLE_CODE(RequestTorrentOfBucketError);
            HANDLE_CODE(SignatureDoesNotMatch);
            HANDLE_CODE(SlowDown);
            HANDLE_CODE(TemporaryRedirect);
            HANDLE_CODE(TokenRefreshRequired);
            HANDLE_CODE(TooManyBuckets);
            HANDLE_CODE(UnexpectedContent);
            HANDLE_CODE(UnresolvableGrantByEmailAddress);
            HANDLE_CODE(UserKeyMustBeSpecified);
        }

        request->s3Error.code = 0;
    }

 code_set:

    (*(request->completeCallback))
        (status, request->httpResponseCode,
         request->errorXmlParserInitialized ? &(request->s3Error) : 0,
         request->callbackData);

    request_release(request);
}
