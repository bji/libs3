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

// This is the data associated with a request, and is set in the private data
// of the curl request
typedef struct Request
{
    // The CURL structure driving the request
    CURL *curl;

    // The headers that will be sent to S3
    struct curl_slist *headers;

    // The callback data to pass to all of the callbacks
    void *callbackData;

    // responseHeaders.metaHeaders strings get copied into here.  Since S3
    // supports a max of 2K for all values and keys, limiting to 2K here is
    // sufficient
    char metaHeaderStrings[S3_MAX_META_HEADER_SIZE];

    // The length thus far of metaHeaderStrings
    int metaHeaderStringsLen;

    // The maximum number of meta headers possible is:
    //   2K / strlen("x-amz-meta-a:\0\0")
    S3MetaHeader metaHeaders[S3_MAX_META_HEADER_SIZE / 
                             (sizeof(S3_META_HEADER_NAME_PREFIX "a:") + 1)];


    // If S3 sends back an error, we store the error text here
    char errorMessage[256];

    // If S3 sends back an error, we store the further details here
    char errorFurtherDetails[1024];

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

    // If S3 did send an XML error, this is it
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


#endif /* PRIVATE_H */
