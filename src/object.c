/** **************************************************************************
 * object.c
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

#include "libs3.h"
#include "request.h"


// put object ----------------------------------------------------------------

void S3_put_object(S3BucketContext *bucketContext, const char *key,
                   uint64_t contentLength,
                   const S3PutProperties *putProperties,
                   S3RequestContext *requestContext,
                   S3PutObjectHandler *handler, void *callbackData)
{
    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypePUT,                           // httpRequestType
        bucketContext->protocol,                      // protocol
        bucketContext->uriStyle,                      // uriStyle
        bucketContext->bucketName,                    // bucketName
        key,                                          // key
        0,                                            // queryParams
        0,                                            // subResource
        bucketContext->accessKeyId,                   // accessKeyId
        bucketContext->secretAccessKey,               // secretAccessKey
        0,                                            // getConditions
        0,                                            // startByte
        0,                                            // byteCount
        putProperties,                                // putProperties
        handler->responseHandler.propertiesCallback,  // propertiesCallback
        handler->putObjectDataCallback,               // toS3Callback
        contentLength,                                // toS3CallbackTotalSize
        0,                                            // fromS3Callback
        handler->responseHandler.completeCallback,    // completeCallback
        callbackData                                  // callbackData
    };

    // Perform the request
    request_perform(&params, requestContext);
}


void S3_copy_object(S3BucketContext *bucketContext, const char *key,
                    const char *destinationBucket, const char *destinationKey,
                    const S3PutProperties *putProperties,
                    S3RequestContext *requestContext,
                    S3ResponseHandler *handler, void *callbackData)
{
}


void S3_get_object(S3BucketContext *bucketContext, const char *key,
                   const S3GetConditions *getConditions,
                   uint64_t startByte, uint64_t byteCount,
                   S3RequestContext *requestContext,
                   S3GetObjectHandler *handler, void *callbackData)
{
    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeGET,                           // httpRequestType
        bucketContext->protocol,                      // protocol
        bucketContext->uriStyle,                      // uriStyle
        bucketContext->bucketName,                    // bucketName
        key,                                          // key
        0,                                            // queryParams
        0,                                            // subResource
        bucketContext->accessKeyId,                   // accessKeyId
        bucketContext->secretAccessKey,               // secretAccessKey
        getConditions,                                // getConditions
        startByte,                                    // startByte
        byteCount,                                    // byteCount
        0,                                            // putProperties
        handler->responseHandler.propertiesCallback,  // propertiesCallback
        0,                                            // toS3Callback
        0,                                            // toS3CallbackTotalSize
        handler->getObjectDataCallback,               // fromS3Callback
        handler->responseHandler.completeCallback,    // completeCallback
        callbackData                                  // callbackData
    };

    // Perform the request
    request_perform(&params, requestContext);
}


void S3_head_object(S3BucketContext *bucketContext, const char *key,
                    const S3GetConditions *getConditions,
                    S3RequestContext *requestContext,
                    S3ResponseHandler *handler, void *callbackData)
{
}
                         

void S3_delete_object(S3BucketContext *bucketContext, const char *key,
                      S3RequestContext *requestContext,
                      S3ResponseHandler *handler, void *callbackData)
{
}
