/** **************************************************************************
 * bucket.c
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

#include "private.h"

static size_t test_bucket_write_callback(void *data, size_t s, size_t n,
                                         void *req)
{
    Request *request = (Request *) req;
    (void) request;

    int len = s * n;

    if (len == 0) {
        return 0;
    }

    char *str = (char *) data;

    char c = str[len - 1];
    str[len - 1] = 0;

#if 0    
    printf("data: %s", str);
    if (c) {
        printf("%c\n", c);
    }
#else
    (void) c;
#endif

    return len;
}


static size_t create_bucket_read_callback(void *data, size_t s, size_t n,
                                          void *req)
{
    Request *request = (Request *) req;
    (void) request;

    return 0;
}


S3Status S3_test_bucket(S3Protocol protocol, const char *accessKeyId,
                        const char *secretAccessKey, const char *bucketName, 
                        int locationConstraintReturnSize,
                        char *locationConstraintReturn,
                        S3RequestContext *requestContext,
                        S3ResponseHandler *handler, void *callbackData)
{
    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeGET,                 // httpRequestType
        protocol,                           // protocol
        S3UriStylePath,                     // uriStyle
        bucketName,                         // bucketName
        0,                                  // key
        0,                                  // queryParams
        "?location",                        // subResource
        accessKeyId,                        // accessKeyId
        secretAccessKey,                    // secretAccessKey
        0,                                  // requestHeaders 
        handler,                            // handler
        { 0 },                              // special callbacks
        callbackData,                       // callbackData
        &test_bucket_write_callback,        // curlWriteCallback
        0,                                  // curlReadCallback
        0                                   // readSize
    };

    // Initialize response data
    if (locationConstraintReturnSize && locationConstraintReturn) {
        locationConstraintReturn[0] = 0;
    }

    // Perform the request
    return request_perform(&params, requestContext);
}
                         
                            
S3Status S3_create_bucket(S3Protocol protocol, const char *accessKeyId,
                          const char *secretAccessKey, const char *bucketName,
                          S3CannedAcl cannedAcl,
                          const char *locationConstraint,
                          S3RequestContext *requestContext,
                          S3ResponseHandler *handler, void *callbackData)
{
    // Set up S3RequestHeaders
    S3RequestHeaders headers =
    {
        0,                                  // contentType
        0,                                  // md5
        0,                                  // cacheControl
        0,                                  // contentDispositionFilename
        0,                                  // contentEncoding
        0,                                  // expires
        cannedAcl,                          // cannedAcl
        0,                                  // sourceObject
        0,                                  // metaDataDirective
        0,                                  // metaHeadersCount
        0                                   // metaHeaders
    };
    
    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypePUT,                 // httpRequestType
        protocol,                           // protocol
        S3UriStylePath,                     // uriStyle
        bucketName,                         // bucketName
        0,                                  // key
        0,                                  // queryParams
        0,                                  // subResource
        accessKeyId,                        // accessKeyId
        secretAccessKey,                    // secretAccessKey
        &headers,                           // requestHeaders 
        handler,                            // handler
        { 0 },                              // special callbacks
        callbackData,                       // callbackData
        0,                                  // curlWriteCallback
        create_bucket_read_callback,        // curlReadCallback
        0                                   // readSize
    };

    // xxx todo support locationConstraint
    
    // Perform the request
    return request_perform(&params, requestContext);
}

                           
S3Status S3_delete_bucket(S3Protocol protocol, const char *accessKeyId,
                          const char *secretAccessKey, const char *bucketName,
                          S3RequestContext *requestContext,
                          S3ResponseHandler *handler, void *callbackData)
{
    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeDELETE,              // httpRequestType
        protocol,                           // protocol
        S3UriStylePath,                     // uriStyle
        bucketName,                         // bucketName
        0,                                  // key
        0,                                  // queryParams
        0,                                  // subResource
        accessKeyId,                        // accessKeyId
        secretAccessKey,                    // secretAccessKey
        0,                                  // requestHeaders 
        handler,                            // handler
        { 0 },                              // special callbacks
        callbackData,                       // callbackData
        0,                                  // curlWriteCallback
        0,                                  // curlReadCallback
        0                                   // readSize
    };

    // xxx todo support locationConstraint
    
    // Perform the request
    return request_perform(&params, requestContext);
}


S3Status S3_list_bucket(S3BucketContext *bucketContext,
                        const char *prefix, const char *marker, 
                        const char *delimiter, int maxkeys,
                        S3RequestContext *requestContext,
                        S3ListBucketHandler *handler, void *callbackData)
{
    return S3StatusOK;
}
