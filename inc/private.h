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
#define S3_META_HEADER_NAME_PREFIX "x-amz-meta-"
#define S3_MAX_META_HEADER_SIZE 2048

// This is the data associated with a curl request, and is set in the private
// data of the curl request
typedef struct CurlRequest
{
    // The CURL structure driving the request
    CURL *curl;

    // The headers that will be sent to S3
    struct curl_slist *headers;

    // The callback data to pass to all of the callbacks
    void *callbackData;

    // Header callback stuff --------------------------------------------------
    // Callback to make when headers are available
    S3ResponseHeadersCallback *headersCallback;

    // The structure to pass to the headers callback
    S3ResponseHeaders responseHeaders;

    // This is nonzero after the result code has been set in the
    // responseHeaders structure, which typically happens when the first
    // curl header callback happens
    int resultCodeSet;

    // responseHeaders.metaHeaders strings get copied into here.  Since S3
    // supports a max of 2K for all values and keys, limiting to 2K here is
    // sufficient
    char metaHeaderStrings[S3_MAX_META_HEADER_SIZE];

    // The maximum number of meta headers possible is:
    //   2K / strlen("x-amz-meta-a:\0\0")
    S3MetaHeader metaHeaderArray[S3_MAX_META_HEADER_SIZE / 
                                 (sizeof(S3_META_HEADER_NAME_PREFIX) + 1)];

    // The callback to make if an error occurs
    S3ErrorCallback *errorCallback;

    // The callback to make when the response has been completely handled
    S3ResponseCompleteCallback *completeCallback;

    // The callbacks to make for the data payload of the response
    union {
        S3ListServiceCallback *listServiceCallback;
        S3ListBucketCallback *listBucketCallback;
        S3PutObjectCallback *putObjectCallback;
        S3GetObjectCallback *getObjectCallback;
    } u;
} CurlRequest;


struct S3RequestContext
{
    CURLM *curlm;
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


// CurlRequest functions
// ------------------------------------------------------

// Initialize the API
S3Status curl_request_api_initialize(const char *userAgentInfo);

// Deinitialize the API
void curl_request_api_deinitialize();

// Get a CurlRequest that has been initialized, except for the data payload
// callback pointer
S3Status curl_request_get(S3ResponseHandler *handler, void *callbackData,
                          CurlRequest **curlRequestReturn);

void curl_request_release(CurlRequest *curlRequest);

S3Status handle_multi_request(CurlRequest *request, 
                              S3RequestContext *requestContext);

S3Status handle_easy_request(CurlRequest *request);

// Curl callbacks
size_t curl_header_func(void *ptr, size_t size, size_t nmemb, void *fstream);


#endif /* PRIVATE_H */
