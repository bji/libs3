/** **************************************************************************
 * private.h
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

#ifndef PRIVATE_H
#define PRIVATE_H

#include <curl/curl.h>
#include <curl/multi.h>
#include "libs3.h"


// As specified in S3 documntation
#define META_HEADER_NAME_PREFIX "x-amz-meta-"
#define HOSTNAME "s3.amazonaws.com"


// Derived from S3 documentation
// The maximum number of x-amz-meta headers that the user can supply is
// limited by the fact that every x-amz-meta header must be of the form:
// x-amz-meta-${NAME}: ${VALUE}
// where NAME and VALUE must be at least 1 character long, so the shortest
// x-amz-meta header would be:
// x-amz-meta-n: v
// So we take the S3's total limit of 2K and divide it by the length of that
#define MAX_META_HEADER_COUNT \
    (S3_MAX_META_HEADER_SIZE / (sizeof(META_HEADER_NAME_PREFIX "n: v")))
// Now the total that we allow for all x-amz- headers includes the ones that
// we additionally add, which is x-amz-acl and x-amz-date
// 256 bytes will be more than enough to cover those
#define MAX_AMZ_HEADER_SIZE (S3_MAX_META_HEADER_SIZE + 256)

#define MAX_CANONICALIZED_RESOURCE_SIZE (S3_MAX_KEY_LENGTH + 1024)

#define MAX_URLENCODED_KEY_SIZE (3 * S3_MAX_KEY_LENGTH)

// This is the data associated with a request, and is set in the private data
// of the curl request
typedef struct Request
{
    // True if this request has already been used
    int used;

    // The CURL structure driving the request
    CURL *curl;

    // The headers that will be sent to S3
    struct curl_slist *headers;

    // The callback data to pass to all of the callbacks
    void *callbackData;

    // responseHeaders.metaHeaders strings get copied into here.  Since S3
    // supports a max of 2K for all values and keys, limiting to 2K here is
    // sufficient
    char responseMetaHeaderStrings[S3_MAX_META_HEADER_SIZE];

    // The length thus far of metaHeaderStrings
    int responseMetaHeaderStringsLen;

    // The maximum number of meta headers possible is:
    //   2K / strlen("x-amz-meta-a:\0\0")
    S3MetaHeader responseMetaHeaders[MAX_META_HEADER_COUNT];

    // Callback stuff ---------------------------------------------------------

    // Callback to make when headers are available
    S3ResponseHeadersCallback *headersCallback;

    // The structure to pass to the headers callback
    S3ResponseHeaders responseHeaders;

    // This is set to nonzero after the haders callback has been made
    int headersCallbackMade;

    // The callback to make when the response has been completely handled
    S3ResponseCompleteCallback *completeCallback;

    // This is set to nonzero after the complete callback has been made
    int completeCallbackMade;
    
    // This will be 0 if S3 didn't send any XML error
    int receivedS3Error;

    // If S3 did send an XML error, this is the parsed form of it
    S3Error s3Error;

    // The callbacks to make for the data payload of the response
    union {
        S3ListServiceCallback *listServiceCallback;
        S3ListBucketCallback *listBucketCallback;
        S3PutObjectCallback *putObjectCallback;
        S3GetObjectCallback *getObjectCallback;
    } u;
} Request;


struct S3RequestContext
{
    CURLM *curlm;

    int count;
};


// Mutex functions ------------------------------------------------------------

// Create a mutex.  Returns 0 if none could be created.
struct S3Mutex *mutex_create();

// Lock a mutex
void mutex_lock(struct S3Mutex *mutex);

// Unlock a mutex
void mutex_unlock(struct S3Mutex *mutex);

// Destroy a mutex
void mutex_destroy(struct S3Mutex *mutex);


// Request functions
// ------------------------------------------------------

// Initialize the API
S3Status request_api_initialize(const char *userAgentInfo);

// Deinitialize the API
void request_api_deinitialize();

// Get a Request that has been initialized, except for the data payload
// callback pointer
S3Status request_get(S3ResponseHandler *handler, void *callbackData,
                          Request **requestReturn);

// Release a Request that is no longer needed
void request_release(Request *request);

// Add a Request to a S3RequestContext
S3Status request_multi_add(Request *request,
                                S3RequestContext *requestContext);

// Perform a Request
void request_easy_perform(Request *request);

// Finish a request; ensures that all callbacks have been made, and also
// releases the request
void request_finish(Request *request, S3Status status);

typedef struct XAmzHeaders
{
    int count;
    char *headers[MAX_META_HEADER_COUNT];
    char headers_raw[MAX_AMZ_HEADER_SIZE];
} XAmzHeaders;

// Composes the entire list of x-amz- headers, which includes all x-amz-meta-
// headers and any other headers.  Each one is guaranteed to be of the form:
// [HEADER]: [VALUE]
// Where HEADER and VALUE have no whitespace in them, and they are separated
// by exactly ": "
// There may be duplicate x-amz-meta- headers returned
// All header names will be lower cased
S3Status request_compose_x_amz_headers(XAmzHeaders *xAmzHeaders,
                                       const S3RequestHeaders *requestHeaders);

// buffer must be at least MAX_URLENCODED_KEY_SIZE bytes
void request_encode_key(char *buffer, const char *key);


// Authorization functions
// ------------------------------------------------------------

// Canonicalizes the given x-amz- headers into the given buffer with the
// given bufferSize.  buffer must be at least S3_MAX_AMZ_HEADER_SIZE bytes.
void canonicalize_amz_headers(char *buffer, const XAmzHeaders *xAmzHeaders);

// Canonicalizes a resource into buffer; buffer must be at least
// MAX_CANONICALIZED_RESOURCE_SIZE bytes long.  subResource is one of:
// "acl", "location", "logging", and "torrent"
void canonicalize_resource(char *buffer, S3UriStyle uriStyle,
                           const char *bucketName,
                           const char *encodedKey, const char *subResource);

S3Status auth_header_snprintf(char *buffer, int bufferSize,
                              const char *accessKeyId,
                              const char *secretAccessKey,
                              const char *httpVerb, const char *md5,
                              const char *contentType,
                              const char *canonicalizedAmzHeaders,
                              const char *canonicalizedResource);


#endif /* PRIVATE_H */
