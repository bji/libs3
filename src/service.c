/** **************************************************************************
 * service.c
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
#include <time.h>
#include "request.h"


typedef struct XmlCallbackData
{
    SimpleXml simpleXml;
    
    S3ResponsePropertiesCallback *responsePropertiesCallback;
    S3ListServiceCallback *listServiceCallback;
    S3ResponseCompleteCallback *responseCompleteCallback;
    void *callbackData;

    string_buffer(ownerId, 256);
    string_buffer(ownerDisplayName, 256);
    string_buffer(bucketName, 256);
    string_buffer(creationDate, 128);
} XmlCallbackData;


static S3Status xmlCallback(const char *elementPath, const char *data,
                            int dataLen, void *callbackData)
{
    XmlCallbackData *cbData = (XmlCallbackData *) callbackData;

    int fit;

    if (data) {
        if (!strcmp(elementPath, "ListAllMyBucketsResult/Owner/ID")) {
            string_buffer_append(cbData->ownerId, data, dataLen, fit);
        }
        else if (!strcmp(elementPath, 
                         "ListAllMyBucketsResult/Owner/DisplayName")) {
            string_buffer_append(cbData->ownerDisplayName, data, dataLen, fit);
        }
        else if (!strcmp(elementPath, 
                         "ListAllMyBucketsResult/Buckets/Bucket/Name")) {
            string_buffer_append(cbData->bucketName, data, dataLen, fit);
        }
        else if (!strcmp
                 (elementPath, 
                  "ListAllMyBucketsResult/Buckets/Bucket/CreationDate")) {
            string_buffer_append(cbData->creationDate, data, dataLen, fit);
        }
    }
    else {
        if (!strcmp(elementPath, "ListAllMyBucketsResult/Buckets/Bucket")) {
            // Parse date.  Assume ISO-8601 date format.
            time_t creationDate = parseIso8601Time(cbData->creationDate);

            // Make the callback - a bucket just finished
            S3Status status = (*(cbData->listServiceCallback))
                (cbData->ownerId, cbData->ownerDisplayName,
                 cbData->bucketName, creationDate, cbData->callbackData);

            string_buffer_initialize(cbData->ownerId);
            string_buffer_initialize(cbData->ownerDisplayName);
            string_buffer_initialize(cbData->bucketName);
            string_buffer_initialize(cbData->creationDate);

            return status;
        }
    }

    return S3StatusOK;
}


static S3Status propertiesCallback
    (const S3ResponseProperties *responseProperties, void *callbackData)
{
    XmlCallbackData *cbData = (XmlCallbackData *) callbackData;
    
    return (*(cbData->responsePropertiesCallback))
        (responseProperties, cbData->callbackData);
}


static S3Status dataCallback(int bufferSize, const char *buffer,
                             void *callbackData)
{
    XmlCallbackData *cbData = (XmlCallbackData *) callbackData;

    return simplexml_add(&(cbData->simpleXml), buffer, bufferSize);
}


static void completeCallback(S3Status requestStatus, int httpResponseCode, 
                             const S3ErrorDetails *s3ErrorDetails,
                             void *callbackData)
{
    XmlCallbackData *cbData = (XmlCallbackData *) callbackData;

    (*(cbData->responseCompleteCallback))
        (requestStatus, httpResponseCode, s3ErrorDetails, 
         cbData->callbackData);

    simplexml_deinitialize(&(cbData->simpleXml));

    free(cbData);
}


void S3_list_service(S3Protocol protocol, const char *accessKeyId,
                     const char *secretAccessKey,
                     S3RequestContext *requestContext,
                     S3ListServiceHandler *handler, void *callbackData)
{
    // Create and set up the callback data
    XmlCallbackData *data = 
        (XmlCallbackData *) malloc(sizeof(XmlCallbackData));
    if (!data) {
        (*(handler->responseHandler.completeCallback))
            (S3StatusOutOfMemory, 0, 0, callbackData);
        return;
    }

    S3Status status = simplexml_initialize
        (&(data->simpleXml), &xmlCallback, data);
    if (status != S3StatusOK) {
        free(data);
        (*(handler->responseHandler.completeCallback))
            (status, 0, 0, callbackData);
        return;
    }

    data->responsePropertiesCallback =
        handler->responseHandler.propertiesCallback;
    data->listServiceCallback = handler->listServiceCallback;
    data->responseCompleteCallback = handler->responseHandler.completeCallback;
    data->callbackData = callbackData;

    string_buffer_initialize(data->ownerId);
    string_buffer_initialize(data->ownerDisplayName);
    string_buffer_initialize(data->bucketName);
    string_buffer_initialize(data->creationDate);
    
    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeGET,                           // httpRequestType
        protocol,                                     // protocol
        S3UriStylePath,                               // uriStyle
        0,                                            // bucketName
        0,                                            // key
        0,                                            // queryParams
        0,                                            // subResource
        accessKeyId,                                  // accessKeyId
        secretAccessKey,                              // secretAccessKey
        0,                                            // getConditions
        0,                                            // startByte
        0,                                            // byteCount
        0,                                            // requestProperties
        &propertiesCallback,                          // propertiesCallback
        0,                                            // toS3Callback
        0,                                            // toS3CallbackTotalSize
        &dataCallback,                                // fromS3Callback
        &completeCallback,                            // completeCallback
        data                                          // callbackData
    };

    // Perform the request
    request_perform(&params, requestContext);
}


