/** **************************************************************************
 * request.c
 * 
 * Copyright 2008 Bryan Ischo <bryan@ischo.com>
 * 
 * This file is part of libs3.
 * 
 * libs3 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 3 of the License.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of this library and its programs with the
 * OpenSSL library, and distribute linked combinations including the two.
 *
 * libs3 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License version 3
 * along with libs3, in a file named COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 ************************************************************************** **/

#include <ctype.h>
#include <gcrypt.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include "request.h"
#include "request_context.h"
#include "response_headers_handler.h"
#include "util.h"


#define USER_AGENT_SIZE 256
#define REQUEST_STACK_SIZE 32

static char userAgentG[USER_AGENT_SIZE];

static pthread_mutex_t requestStackMutexG;

static Request *requestStackG[REQUEST_STACK_SIZE];

static int requestStackCountG;


typedef struct RequestComputedValues
{
    // All x-amz- headers, in normalized form (i.e. NAME: VALUE, no other ws)
    char *amzHeaders[S3_MAX_METADATA_COUNT + 2]; // + 2 for acl and date

    // The number of x-amz- headers
    int amzHeadersCount;

    // Storage for amzHeaders (the +256 is for x-amz-acl and x-amz-date)
    char amzHeadersRaw[COMPACTED_METADATA_BUFFER_SIZE + 256 + 1];

    // Canonicalized x-amz- headers
    string_multibuffer(canonicalizedAmzHeaders,
                       COMPACTED_METADATA_BUFFER_SIZE + 256 + 1);

    // URL-Encoded key
    char urlEncodedKey[MAX_URLENCODED_KEY_SIZE + 1];

    // Canonicalized resource
    char canonicalizedResource[MAX_CANONICALIZED_RESOURCE_SIZE + 1];

    // Cache-Control header (or empty)
    char cacheControlHeader[128];

    // Content-Type header (or empty)
    char contentTypeHeader[128];

    // Content-MD5 header (or empty)
    char md5Header[128];

    // Content-Disposition header (or empty)
    char contentDispositionHeader[128];

    // Content-Encoding header (or empty)
    char contentEncodingHeader[128];

    // Expires header (or empty)
    char expiresHeader[128];

    // If-Modified-Since header
    char ifModifiedSinceHeader[128];

    // If-Unmodified-Since header
    char ifUnmodifiedSinceHeader[128];

    // If-Match header
    char ifMatchHeader[128];

    // If-None-Match header
    char ifNoneMatchHeader[128];

    // Range header
    char rangeHeader[128];

    // Authorization header
    char authorizationHeader[128];
} RequestComputedValues;


// Called whenever we detect that the request headers have been completely
// processed; which happens either when we get our first read/write callback,
// or the request is finished being procesed.  Returns nonzero on success,
// zero on failure.
static void request_headers_done(Request *request)
{
    if (request->propertiesCallbackMade) {
        return;
    }

    request->propertiesCallbackMade = 1;

    // Get the http response code
    long httpResponseCode;
    request->httpResponseCode = 0;
    if (curl_easy_getinfo(request->curl, CURLINFO_RESPONSE_CODE, 
                          &httpResponseCode) != CURLE_OK) {
        // Not able to get the HTTP response code - error
        request->status = S3StatusInternalError;
        return;
    }
    else {
        request->httpResponseCode = httpResponseCode;
    }

    response_headers_handler_done(&(request->responseHeadersHandler), 
                                  request->curl);

    // Only make the callback if it was a successful request; otherwise we're
    // returning information about the error response itself
    if (request->propertiesCallback &&
        (request->httpResponseCode >= 200) &&
        (request->httpResponseCode <= 299)) {
        request->status = (*(request->propertiesCallback))
            (&(request->responseHeadersHandler.responseProperties), 
             request->callbackData);
    }
}


static size_t curl_header_func(void *ptr, size_t size, size_t nmemb,
                               void *data)
{
    Request *request = (Request *) data;

    int len = size * nmemb;

    response_headers_handler_add
        (&(request->responseHeadersHandler), (char *) ptr, len);

    return len;
}


static size_t curl_read_func(void *ptr, size_t size, size_t nmemb, void *data)
{
    Request *request = (Request *) data;

    int len = size * nmemb;

    request_headers_done(request);

    if (request->status != S3StatusOK) {
        return CURL_READFUNC_ABORT;
    }

    if (request->toS3Callback) {
        int ret = (*(request->toS3Callback))
            (len, (char *) ptr, request->callbackData);
        if (ret < 0) {
            request->status = S3StatusAbortedByCallback;
            return CURL_READFUNC_ABORT;
        }
        else {
            return ret;
        }
    }
    else {
        request->status = S3StatusInternalError;
        return CURL_READFUNC_ABORT;
    }
}


static size_t curl_write_func(void *ptr, size_t size, size_t nmemb,
                              void *data)
{
    Request *request = (Request *) data;

    int len = size * nmemb;

    request_headers_done(request);

    if (request->status != S3StatusOK) {
        return 0;
    }

    // On HTTP error, we expect to parse an HTTP error response
    if ((request->httpResponseCode < 200) || 
        (request->httpResponseCode > 299)) {
        request->status = error_parser_add
            (&(request->errorParser), (char *) ptr, len);
    }
    // If there was a callback registered, make it
    else if (request->fromS3Callback) {
        request->status = (*(request->fromS3Callback))
            (len, (char *) ptr, request->callbackData);
    }
    // Else, consider this an error - S3 has sent back data when it was not
    // expected
    else {
        request->status = S3StatusInternalError;
    }

    return ((request->status == S3StatusOK) ? len : 0);
}


// This function 'normalizes' all x-amz-meta headers provided in
// params->requestHeaders, which means it removes all whitespace from
// them such that they all look exactly like this:
// x-amz-meta-${NAME}: ${VALUE}
// It also adds the x-amz-acl, x-amz-copy-source, and x-amz-metadata-directive
// headers if necessary, and always adds the x-amz-date header.  It copies the
// raw string values into params->amzHeadersRaw, and creates an array of
// string pointers representing these headers in params->amzHeaders (and also
// sets params->amzHeadersCount to be the count of the total number of x-amz-
// headers thus created).
static S3Status compose_amz_headers(const RequestParams *params,
                                    RequestComputedValues *values)
{
    const S3PutProperties *properties = params->putProperties;

    values->amzHeadersCount = 0;
    values->amzHeadersRaw[0] = 0;
    int len = 0;

    // Append a header to amzHeaders, trimming whitespace from the end.
    // Does NOT trim whitespace from the beginning.
#define headers_append(isNewHeader, format, ...)                        \
    do {                                                                \
        if (isNewHeader) {                                              \
            values->amzHeaders[values->amzHeadersCount++] =             \
                &(values->amzHeadersRaw[len]);                          \
        }                                                               \
        len += snprintf(&(values->amzHeadersRaw[len]),                  \
                        sizeof(values->amzHeadersRaw) - len,            \
                        format, __VA_ARGS__);                           \
        if (len >= sizeof(values->amzHeadersRaw)) {                     \
            return S3StatusMetaDataHeadersTooLong;                      \
        }                                                               \
        while ((len > 0) && (values->amzHeadersRaw[len - 1] == ' ')) {  \
            len--;                                                      \
        }                                                               \
        values->amzHeadersRaw[len++] = 0;                               \
    } while (0)

#define header_name_tolower_copy(str, l)                                \
    do {                                                                \
        values->amzHeaders[values->amzHeadersCount++] =                 \
            &(values->amzHeadersRaw[len]);                              \
        if ((len + l) >= sizeof(values->amzHeadersRaw)) {               \
            return S3StatusMetaDataHeadersTooLong;                      \
        }                                                               \
        int todo = l;                                                   \
        while (todo--) {                                                \
            if ((*(str) >= 'A') && (*(str) <= 'Z')) {                   \
                values->amzHeadersRaw[len++] = 'a' + (*(str) - 'A');    \
            }                                                           \
            else {                                                      \
                values->amzHeadersRaw[len++] = *(str);                  \
            }                                                           \
            (str)++;                                                    \
        }                                                               \
    } while (0)

    // Check and copy in the x-amz-meta headers
    if (properties) {
        int i;
        for (i = 0; i < properties->metaDataCount; i++) {
            const S3NameValue *property = &(properties->metaData[i]);
            char headerName[S3_MAX_METADATA_SIZE - sizeof(": v")];
            int l = snprintf(headerName, sizeof(headerName),
                             S3_METADATA_HEADER_NAME_PREFIX "%s",
                             property->name);
            char *hn = headerName;
            header_name_tolower_copy(hn, l);
            // Copy in the value
            headers_append(0, ": %s", property->value);
        }

        // Add the x-amz-acl header, if necessary
        const char *cannedAclString;
        switch (params->putProperties->cannedAcl) {
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
        if (params->copySourceBucketName && params->copySourceBucketName[0] &&
            params->copySourceKey && params->copySourceKey[0]) {
            headers_append(1, "x-amz-copy-source: /%s/%s",
                           params->copySourceBucketName,
                           params->copySourceKey);
        }
        // And the x-amz-metadata-directive header
        if (params->putProperties) {
            headers_append(1, "%s", "x-amz-metadata-directive: REPLACE");
        }
    }

    return S3StatusOK;
}


// Composes the other headers
static S3Status compose_standard_headers(const RequestParams *params,
                                         RequestComputedValues *values)
{

#define do_put_header(fmt, sourceField, destField, badError, tooLongError)  \
    do {                                                                    \
        if (params->putProperties &&                                        \
            params->putProperties-> sourceField &&                          \
            params->putProperties-> sourceField[0]) {                       \
            /* Skip whitespace at beginning of val */                       \
            const char *val = params->putProperties-> sourceField;          \
            while (*val && isblank(*val)) {                                 \
                val++;                                                      \
            }                                                               \
            if (!*val) {                                                    \
                return badError;                                            \
            }                                                               \
            /* Compose header, make sure it all fit */                      \
            int len = snprintf(values-> destField,                          \
                               sizeof(values-> destField), fmt, val);       \
            if (len >= sizeof(values-> destField)) {                        \
                return tooLongError;                                        \
            }                                                               \
            /* Now remove the whitespace at the end */                      \
            while (isblank(values-> destField[len])) {                      \
                len--;                                                      \
            }                                                               \
            values-> destField[len] = 0;                                    \
        }                                                                   \
        else {                                                              \
            values-> destField[0] = 0;                                      \
        }                                                                   \
    } while (0)

#define do_get_header(fmt, sourceField, destField, badError, tooLongError)  \
    do {                                                                    \
        if (params->getConditions &&                                        \
            params->getConditions-> sourceField &&                          \
            params->getConditions-> sourceField[0]) {                       \
            /* Skip whitespace at beginning of val */                       \
            const char *val = params->getConditions-> sourceField;          \
            while (*val && isblank(*val)) {                                 \
                val++;                                                      \
            }                                                               \
            if (!*val) {                                                    \
                return badError;                                            \
            }                                                               \
            /* Compose header, make sure it all fit */                      \
            int len = snprintf(values-> destField,                          \
                               sizeof(values-> destField), fmt, val);       \
            if (len >= sizeof(values-> destField)) {                        \
                return tooLongError;                                        \
            }                                                               \
            /* Now remove the whitespace at the end */                      \
            while (isblank(values-> destField[len])) {                      \
                len--;                                                      \
            }                                                               \
            values-> destField[len] = 0;                                    \
        }                                                                   \
        else {                                                              \
            values-> destField[0] = 0;                                      \
        }                                                                   \
    } while (0)

    // Cache-Control
    do_put_header("Cache-Control: %s", cacheControl, cacheControlHeader,
                  S3StatusBadCacheControl, S3StatusCacheControlTooLong);
    
    // ContentType
    do_put_header("Content-Type: %s", contentType, contentTypeHeader,
                  S3StatusBadContentType, S3StatusContentTypeTooLong);

    // MD5
    do_put_header("Content-MD5: %s", md5, md5Header, S3StatusBadMD5,
                  S3StatusMD5TooLong);

    // Content-Disposition
    do_put_header("Content-Disposition: attachment; filename=\"%s\"",
                  contentDispositionFilename, contentDispositionHeader,
                  S3StatusBadContentDispositionFilename,
                  S3StatusContentDispositionFilenameTooLong);
    
    // ContentEncoding
    do_put_header("Content-Encoding: %s", contentEncoding, 
                  contentEncodingHeader, S3StatusBadContentEncoding,
                  S3StatusContentEncodingTooLong);
    
    // Expires
    if (params->putProperties && (params->putProperties->expires >= 0)) {
        strftime(values->expiresHeader, sizeof(values->expiresHeader),
                 "Expires: %a, %d %b %Y %H:%M:%S UTC", 
                 gmtime(&(params->putProperties->expires)));
    }
    else {
        values->expiresHeader[0] = 0;
    }

    // If-Modified-Since
    if (params->getConditions &&
        (params->getConditions->ifModifiedSince >= 0)) {
        strftime(values->ifModifiedSinceHeader,
                 sizeof(values->ifModifiedSinceHeader),
                 "If-Modified-Since: %a, %d %b %Y %H:%M:%S UTC", 
                 gmtime(&(params->getConditions->ifModifiedSince)));
    }
    else {
        values->ifModifiedSinceHeader[0] = 0;
    }

    // If-Unmodified-Since header
    if (params->getConditions &&
        (params->getConditions->ifNotModifiedSince >= 0)) {
        strftime(values->ifUnmodifiedSinceHeader,
                 sizeof(values->ifUnmodifiedSinceHeader),
                 "If-Unmodified-Since: %a, %d %b %Y %H:%M:%S UTC", 
                 gmtime(&(params->getConditions->ifNotModifiedSince)));
    }
    else {
        values->ifUnmodifiedSinceHeader[0] = 0;
    }
    
    // If-Match header
    do_get_header("If-Match: %s", ifMatchETag, ifMatchHeader,
                  S3StatusBadIfMatchETag, S3StatusIfMatchETagTooLong);
    
    // If-None-Match header
    do_get_header("If-None-Match: %s", ifNotMatchETag, ifNoneMatchHeader,
                  S3StatusBadIfNotMatchETag, 
                  S3StatusIfNotMatchETagTooLong);
    
    // Range header
    if (params->getConditions && (params->startByte || params->byteCount)) {
        if (params->byteCount) {
            snprintf(values->rangeHeader, sizeof(values->rangeHeader),
                     "Range: bytes=%llu-%llu", 
                     (unsigned long long) params->startByte,
                     (unsigned long long) (params->startByte + 
                                           params->byteCount - 1));
        }
        else {
            snprintf(values->rangeHeader, sizeof(values->rangeHeader),
                     "Range: bytes=%llu-", 
                     (unsigned long long) params->startByte);
        }
    }
    else {
        values->rangeHeader[0] = 0;
    }

    return S3StatusOK;
}


// URL encodes the params->key value into params->urlEncodedKey
static S3Status encode_key(const RequestParams *params,
                           RequestComputedValues *values)
{
    return (urlEncode(values->urlEncodedKey, params->key, S3_MAX_KEY_SIZE) ?
            S3StatusOK : S3StatusUriTooLong);
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
static void canonicalize_amz_headers(RequestComputedValues *values)
{
    // Make a copy of the headers that will be sorted
    const char *sortedHeaders[S3_MAX_METADATA_COUNT];

    memcpy(sortedHeaders, values->amzHeaders,
           (values->amzHeadersCount * sizeof(sortedHeaders[0])));

    // Now sort these
    header_gnome_sort(sortedHeaders, values->amzHeadersCount);

    // Now copy this sorted list into the buffer, all the while:
    // - folding repeated headers into single lines, and
    // - folding multiple lines
    // - removing the space after the colon
    int lastHeaderLen = 0, i;
    char *buffer = values->canonicalizedAmzHeaders;
    for (i = 0; i < values->amzHeadersCount; i++) {
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
static void canonicalize_resource(const RequestParams *params,
                                  RequestComputedValues *values)
{
    char *buffer = values->canonicalizedResource;
    int len = 0;

    *buffer = 0;

#define append(str) len += sprintf(&(buffer[len]), "%s", str)

    if (params->bucketName && params->bucketName[0]) {
        buffer[len++] = '/';
        append(params->bucketName);
    }

    append("/");

    if (values->urlEncodedKey[0]) {
        append(values->urlEncodedKey);
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
    case HttpRequestTypeCOPY:
        return "PUT";
    default: // HttpRequestTypeDELETE
        return "DELETE";
    }
}


// Composes the Authorization header for the request
static S3Status compose_auth_header(const RequestParams *params,
                                    RequestComputedValues *values)
{
    // We allow for:
    // 17 bytes for HTTP-Verb + \n
    // 129 bytes for MD5 + \n
    // 129 bytes for Content-Type + \n
    // 1 byte for Data + \n
    // CanonicalizedAmzHeaders & CanonicalizedResource
    char signbuf[17 + 129 + 129 + 1 + 
                 (sizeof(values->canonicalizedAmzHeaders) - 1) +
                 (sizeof(values->canonicalizedResource) - 1) + 1];
    int len = 0;

#define signbuf_append(format, ...)                             \
    len += snprintf(&(signbuf[len]), sizeof(signbuf) - len,     \
                    format, __VA_ARGS__)

    signbuf_append
        ("%s\n", http_request_type_to_verb(params->httpRequestType));

    // For MD5 and Content-Type, use the value in the actual header, because
    // it's already been trimmed
    signbuf_append("%s\n", values->md5Header[0] ? 
                   &(values->md5Header[sizeof("Content-MD5: ") - 1]) : "");

    signbuf_append
        ("%s\n", values->contentTypeHeader[0] ? 
         &(values->contentTypeHeader[sizeof("Content-Type: ") - 1]) : "");

    signbuf_append("%s", "\n"); // Date - we always use x-amz-date

    signbuf_append("%s", values->canonicalizedAmzHeaders);

    signbuf_append("%s", values->canonicalizedResource);

    // Generate a SHA-1 of the signbuf

    // Message Digest handle
    gcry_md_hd_t mdh;

    // "Open" the Message Digest Handle - SHA-1 with HMAC feature
    if (gcry_md_open
        (&mdh, GCRY_MD_SHA1, GCRY_MD_FLAG_HMAC) != GPG_ERR_NO_ERROR) {
        return S3StatusInternalError;
    }

    // Set the key that will be used with the HMAC feature
    if (gcry_md_setkey
        (mdh, params->secretAccessKey, 
         strlen(params->secretAccessKey)) != GPG_ERR_NO_ERROR) {
        gcry_md_close(mdh);
        return S3StatusInternalError;
    }

    // Specify the signbuf data to compute SHA-1 of
    gcry_md_write(mdh, signbuf, len);

    // Get the results
    unsigned int md_len = gcry_md_get_algo_dlen(GCRY_MD_SHA1);
    unsigned char *md = gcry_md_read(mdh, GCRY_MD_SHA1);

    // Now base-64 encode the results
    unsigned char b64[((md_len + 1) * 4) / 3];
    int b64Len = base64Encode(md, md_len, b64);

    // Be sure to release the Message Digest handle
    gcry_md_close(mdh);

    snprintf(values->authorizationHeader, sizeof(values->authorizationHeader),
             "Authorization: AWS %s:%.*s", params->accessKeyId, b64Len, b64);

    return S3StatusOK;
}


// Compose the URI to use for the request given the request parameters
static S3Status compose_uri(Request *request, const RequestParams *params, 
                            const RequestComputedValues *values)
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

    uri_append("%s", "/");

    if (params->key && params->key[0]) {
        uri_append("%s", values->urlEncodedKey);
    }

    if (params->subResource && params->subResource[0]) {
        uri_append("%s", params->subResource);
    }
    
    if (params->queryParams) {
        uri_append("%s", params->queryParams);
    }

    return S3StatusOK;
}


// Sets up the curl handle given the completely computed RequestParams
static S3Status setup_curl(Request *request,
                           const RequestParams *params,
                           const RequestComputedValues *values)
{
    CURLcode status;

#define curl_easy_setopt_safe(opt, val)                                 \
    if ((status = curl_easy_setopt                                      \
         (request->curl, opt, val)) != CURLE_OK) {                      \
        return S3StatusFailedToInitializeRequest;                       \
    }

    // Debugging only
    curl_easy_setopt_safe(CURLOPT_VERBOSE, 1);
    
    // Set private data to request for the benefit of S3RequestContext
    curl_easy_setopt_safe(CURLOPT_PRIVATE, request);
    
    // Set header callback and data
    curl_easy_setopt_safe(CURLOPT_HEADERDATA, request);
    curl_easy_setopt_safe(CURLOPT_HEADERFUNCTION, &curl_header_func);
    
    // Set read callback, data, and readSize
    curl_easy_setopt_safe(CURLOPT_READFUNCTION, &curl_read_func);
    curl_easy_setopt_safe(CURLOPT_READDATA, request);
    
    // Set write callback and data
    curl_easy_setopt_safe(CURLOPT_WRITEFUNCTION, &curl_write_func);
    curl_easy_setopt_safe(CURLOPT_WRITEDATA, request);

    // Ask curl to parse the Last-Modified header.  This is easier than
    // parsing it ourselves.
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

    // Don't verify S3's certificate, there are known to be issues with
    // them sometimes
    // xxx todo - support an option for verifying the S3 CA (default false)
    curl_easy_setopt_safe(CURLOPT_SSL_VERIFYPEER, 0);

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

    // Append standard headers
#define append_standard_header(fieldName)                               \
    if (values-> fieldName [0]) {                                       \
        request->headers = curl_slist_append(request->headers,          \
                                             values-> fieldName);       \
    }

    // Would use CURLOPT_INFILESIZE_LARGE, but it is buggy in libcurl
    if (params->httpRequestType == HttpRequestTypePUT) {
        char header[256];
        snprintf(header, sizeof(header), "Content-Length: %llu",
                 (unsigned long long) params->toS3CallbackTotalSize);
        request->headers = curl_slist_append(request->headers, header);
        request->headers = curl_slist_append(request->headers, 
                                             "Transfer-Encoding:");
    }
    
    append_standard_header(cacheControlHeader);
    append_standard_header(contentTypeHeader);
    append_standard_header(md5Header);
    append_standard_header(contentDispositionHeader);
    append_standard_header(contentEncodingHeader);
    append_standard_header(expiresHeader);
    append_standard_header(ifModifiedSinceHeader);
    append_standard_header(ifUnmodifiedSinceHeader);
    append_standard_header(ifMatchHeader);
    append_standard_header(ifNoneMatchHeader);
    append_standard_header(rangeHeader);
    append_standard_header(authorizationHeader);

    // Append x-amz- headers
    int i;
    for (i = 0; i < values->amzHeadersCount; i++) {
        request->headers = 
            curl_slist_append(request->headers, values->amzHeaders[i]);
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
    case HttpRequestTypeCOPY:
        curl_easy_setopt_safe(CURLOPT_UPLOAD, 1);
        break;
    case HttpRequestTypeDELETE:
	curl_easy_setopt_safe(CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
    default: // HttpRequestTypeGET
        break;
    }
    
    return S3StatusOK;
}


static void request_deinitialize(Request *request)
{
    if (request->headers) {
        curl_slist_free_all(request->headers);
    }
    
    error_parser_deinitialize(&(request->errorParser));
}


static S3Status request_get(const RequestParams *params, 
                            const RequestComputedValues *values,
                            Request **reqReturn)
{
    Request *request = 0;
    
    // Try to get one from the request stack.  We hold the lock for the
    // shortest time possible here.
    pthread_mutex_lock(&requestStackMutexG);

    if (requestStackCountG) {
        request = requestStackG[--requestStackCountG];
    }
    
    pthread_mutex_unlock(&requestStackMutexG);

    // If we got one, deinitialize it for re-use
    if (request) {
        request_deinitialize(request);
    }
    // Else there wasn't one available in the request stack, so create one
    else {
        if (!(request = (Request *) malloc(sizeof(Request)))) {
            return S3StatusOutOfMemory;
        }
        if (!(request->curl = curl_easy_init())) {
            free(request);
            return S3StatusFailedToInitializeRequest;
        }
    }

    // Initialize the request
    request->prev = request->next = 0;

    // Request status is initialized to no error, will be updated whenever
    // an error occurs
    request->status = S3StatusOK;

    S3Status status;
                        
    // Start out with no headers
    request->headers = 0;

    // Compute the URL
    if ((status = compose_uri(request, params, values)) != S3StatusOK) {
        curl_easy_cleanup(request->curl);
        free(request);
        return status;
    }

    // Set all of the curl handle options
    if ((status = setup_curl(request, params, values)) != S3StatusOK) {
        curl_easy_cleanup(request->curl);
        free(request);
        return status;
    }

    request->propertiesCallback = params->propertiesCallback;

    request->toS3Callback = params->toS3Callback;

    request->fromS3Callback = params->fromS3Callback;

    request->completeCallback = params->completeCallback;

    request->callbackData = params->callbackData;

    response_headers_handler_initialize(&(request->responseHeadersHandler));

    request->propertiesCallbackMade = 0;
    
    error_parser_initialize(&(request->errorParser));

    *reqReturn = request;
    
    return S3StatusOK;
}


static void request_destroy(Request *request)
{
    request_deinitialize(request);
    curl_easy_cleanup(request->curl);
    free(request);
}


static void request_release(Request *request)
{
    pthread_mutex_lock(&requestStackMutexG);

    // If the request stack is full, destroy this one
    if (requestStackCountG == REQUEST_STACK_SIZE) {
        pthread_mutex_unlock(&requestStackMutexG);
        request_destroy(request);
    }
    // Else put this one at the front of the request stack; we do this because
    // we want the most-recently-used curl handle to be re-used on the next
    // request, to maximize our chances of re-using a TCP connection before it
    // times out
    else {
        requestStackG[requestStackCountG++] = request;
        pthread_mutex_unlock(&requestStackMutexG);
    }
}


S3Status request_api_initialize(const char *userAgentInfo, int flags)
{
    if (curl_global_init(CURL_GLOBAL_ALL & 
                         ~((flags & S3_INIT_WINSOCK) ? 0 : CURL_GLOBAL_WIN32))
        != CURLE_OK) {
        return S3StatusInternalError;
    }

    pthread_mutex_init(&requestStackMutexG, 0);

    requestStackCountG = 0;

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
        snprintf(platform, sizeof(platform), "%s%s%s", utsn.sysname, 
                 utsn.machine[0] ? " " : "", utsn.machine);
    }

    snprintf(userAgentG, sizeof(userAgentG), 
             "Mozilla/4.0 (Compatible; %s; libs3 %s.%s; %s)",
             userAgentInfo, LIBS3_VER_MAJOR, LIBS3_VER_MINOR, platform);
    
    return S3StatusOK;
}


void request_api_deinitialize()
{
    pthread_mutex_destroy(&requestStackMutexG);

    while (requestStackCountG--) {
        request_destroy(requestStackG[requestStackCountG]);
    }
}


void request_perform(const RequestParams *params, S3RequestContext *context)
{
    Request *request;
    S3Status status;

#define return_status(status)                                           \
    (*(params->completeCallback))(status, 0, params->callbackData);     \
    return

    // These will hold the computed values
    RequestComputedValues computed;

    // Validate the bucket name
    if (params->bucketName && 
        ((status = S3_validate_bucket_name
          (params->bucketName, params->uriStyle)) != S3StatusOK)) {
        return_status(status);
    }

    // Compose the amz headers
    if ((status = compose_amz_headers(params, &computed)) != S3StatusOK) {
        return_status(status);
    }

    // Compose standard headers
    if ((status = compose_standard_headers
         (params, &computed)) != S3StatusOK) {
        return_status(status);
    }

    // URL encode the key
    if ((status = encode_key(params, &computed)) != S3StatusOK) {
        return_status(status);
    }

    // Compute the canonicalized amz headers
    canonicalize_amz_headers(&computed);

    // Compute the canonicalized resource
    canonicalize_resource(params, &computed);

    // Compose Authorization header
    if ((status = compose_auth_header(params, &computed)) != S3StatusOK) {
        return_status(status);
    }
    
    // Get an initialized Request structure now
    if ((status = request_get(params, &computed, &request)) != S3StatusOK) {
        return_status(status);
    }

    // If a RequestContext was provided, add the request to the curl multi
    if (context) {
        CURLMcode code = curl_multi_add_handle(context->curlm, request->curl);
        if (code == CURLM_OK) {
            if (context->requests) {
                request->prev = context->requests->prev;
                request->next = context->requests;
                context->requests->prev->next = 
                    context->requests->prev = request;
            }
            else {
                context->requests = request->next = request->prev = request;
            }
        }
        else {
            if (request->status == S3StatusOK) {
                request->status = (code == CURLM_OUT_OF_MEMORY) ?
                    S3StatusOutOfMemory : S3StatusInternalError;
            }
            request_finish(request);
        }
    }
    // Else, perform the request immediately
    else {
        CURLcode code = curl_easy_perform(request->curl);
        if ((code != CURLE_OK) && (request->status == S3StatusOK)) {
            request->status = request_curl_code_to_status(code);
        }
        // Finish the request, ensuring that all callbacks have been made, and
        // also releases the request
        request_finish(request);
    }
}


void request_finish(Request *request)
{
    // If we haven't detected this already, we now know that the headers are
    // definitely done being read in
    request_headers_done(request);
    
    // If there was no error processing the request, then possibly there was
    // an S3 error parsed, which should be converted into the request status
    if (request->status == S3StatusOK) {
        error_parser_convert_status(&(request->errorParser), 
                                    &(request->status));
        // If there still was no error recorded, then it is possible that
        // there was in fact an error but that there was no error XML
        // detailing the error
        if ((request->status == S3StatusOK) &&
            ((request->httpResponseCode < 200) ||
             (request->httpResponseCode > 299))) {
            switch (request->httpResponseCode) {
            case 100: // Some versions of libcurl erroneously set HTTP
                      // status to this
                break;
            case 301:
                request->status = S3StatusErrorPermanentRedirect;
                break;
            case 307:
                request->status = S3StatusHttpErrorMovedTemporarily;
                break;
            case 400:
                request->status = S3StatusHttpErrorBadRequest;
                break;
            case 403: 
                request->status = S3StatusHttpErrorForbidden;
                break;
            case 404:
                request->status = S3StatusHttpErrorNotFound;
                break;
            case 405:
                request->status = S3StatusErrorMethodNotAllowed;
                break;
            case 409:
                request->status = S3StatusHttpErrorConflict;
                break;
            case 411:
                request->status = S3StatusErrorMissingContentLength;
                break;
            case 412:
                request->status = S3StatusErrorPreconditionFailed;
                break;
            case 416:
                request->status = S3StatusErrorInvalidRange;
                break;
            case 500:
                request->status = S3StatusErrorInternalError;
                break;
            case 501:
                request->status = S3StatusErrorNotImplemented;
                break;
            case 503:
                request->status = S3StatusErrorSlowDown;
                break;
            default:
                request->status = S3StatusHttpErrorUnknown;
                break;
            }
        }
    }

    (*(request->completeCallback))
        (request->status, &(request->errorParser.s3ErrorDetails),
         request->callbackData);

    request_release(request);
}


S3Status request_curl_code_to_status(CURLcode code)
{
    switch (code) {
    case CURLE_OUT_OF_MEMORY:
        return S3StatusOutOfMemory;
    case CURLE_COULDNT_RESOLVE_PROXY:
    case CURLE_COULDNT_RESOLVE_HOST:
        return S3StatusNameLookupError;
    case CURLE_COULDNT_CONNECT:
        return S3StatusFailedToConnect;
    case CURLE_WRITE_ERROR:
    case CURLE_OPERATION_TIMEDOUT:
        return S3StatusConnectionFailed;
    case CURLE_PARTIAL_FILE:
        return S3StatusOK;
    case CURLE_SSL_CACERT:
        return S3StatusServerFailedVerification;
    default:
        return S3StatusInternalError;
    }
}
