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

typedef struct CurlRequest
{
    CURL *curl;
    struct curl_slist *headers;
} CurlRequest;

// PrivateData is what is passed into every curl callback, as well as being
// available in the CURLOPT_PRIVATE option.  It includes all callbacks and
// callback data associated with the request
typedef struct PrivateData
{
    // Always set
    S3ResponseHeadersCallback *headersCallback;
    // Always set
    S3ErrorCallback *errorCallback;
    // Only set for list service operation
    S3ListServiceCallback *listServiceCallback;
    // Only set for list bucket operation
    S3ListBucketCallback *listBucketCallback;
    // Only set for put object operation
    S3PutObjectCallback *putObjectCallback;
    // Only set for get object operation
    S3GetObjectCallback *getObjectCallback;
    // Always set
    S3ResponseCompleteCallback *completeCallback;
    // Always set
    void *data;
} PrivateData;

struct S3RequestContext
{
    CURLM *curlm;
};

struct S3Mutex *mutex_create();

void mutex_lock(struct S3Mutex *mutex);

void mutex_unlock(struct S3Mutex *mutex);

void mutex_destroy(struct S3Mutex *mutex);

PrivateData *create_private_data(S3RequestHandler *handler,
                                 S3ListServiceCallback *listServiceCallback,
                                 S3ListBucketCallback *listBucketCallback,
                                 S3PutObjectCallback *putObjectCallback,
                                 S3GetObjectCallback *getObjectCallback,
                                 void *data);

S3Status pool_initialize(const char *userAgentInfo);

void pool_deinitialize();

S3Status pool_get(PrivateData *privateData, CurlRequest **curlRequestReturn);

void pool_release(CurlRequest *curlRequest);

S3Status handle_multi_request(CurlRequest *request, 
                              S3RequestContext *requestContext);

S3Status handle_easy_request(CurlRequest *request);

// Curl callbacks
size_t curl_header_func(void *ptr, size_t size, size_t nmemb, void *fstream);



#endif /* PRIVATE_H */
