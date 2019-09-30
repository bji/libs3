/** **************************************************************************
 * object.c
 *
 * Copyright 2008 Bryan Ischo <bryan@ischo.com>
 *
 * This file is part of libs3.
 *
 * libs3 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, version 3 or above of the License.  You can also
 * redistribute and/or modify it under the terms of the GNU General Public
 * License, version 2 or above of the License.
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
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with libs3, in a file named COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * You should also have received a copy of the GNU General Public License
 * version 2 along with libs3, in a file named COPYING-GPLv2.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 ************************************************************************** **/

#include <stdlib.h>
#include <string.h>
#include "libs3.h"
#include "request.h"
#include "md5base64.h"

#ifdef LIBS3_DEBUG
#define _return(status) \
    do { debug_printf("%s:%d return %s\n", __FILE__, __LINE__, S3_get_status_name(status)); return status; } while(0)
#else
#define _return(status) \
    return status
#endif

// put object ----------------------------------------------------------------

void S3_put_object(const S3BucketContext *bucketContext, const char *key,
                   uint64_t contentLength,
                   const S3PutProperties *putProperties,
                   S3RequestContext *requestContext,
                   int timeoutMs,
                   const S3PutObjectHandler *handler, void *callbackData)
{
    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypePUT,                           // httpRequestType
        { bucketContext->hostName,                    // hostName
          bucketContext->bucketName,                  // bucketName
          bucketContext->protocol,                    // protocol
          bucketContext->uriStyle,                    // uriStyle
          bucketContext->accessKeyId,                 // accessKeyId
          bucketContext->secretAccessKey,             // secretAccessKey
          bucketContext->securityToken,               // securityToken
          bucketContext->authRegion },                // authRegion
        key,                                          // key
        0,                                            // queryParams
        0,                                            // subResource
        0,                                            // copySourceBucketName
        0,                                            // copySourceKey
        0,                                            // getConditions
        0,                                            // startByte
        0,                                            // byteCount
        putProperties,                                // putProperties
        handler->responseHandler.propertiesCallback,  // propertiesCallback
        handler->putObjectDataCallback,               // toS3Callback
        contentLength,                                // toS3CallbackTotalSize
        0,                                            // fromS3Callback
        handler->responseHandler.completeCallback,    // completeCallback
        callbackData,                                 // callbackData
        timeoutMs                                     // timeoutMs
    };

    // Perform the request
    request_perform(&params, requestContext);
}


// copy object ---------------------------------------------------------------


typedef struct CopyObjectData
{
    SimpleXml simpleXml;

    S3ResponsePropertiesCallback *responsePropertiesCallback;
    S3ResponseCompleteCallback *responseCompleteCallback;
    void *callbackData;

    int64_t *lastModifiedReturn;
    int eTagReturnSize;
    char *eTagReturn;
    int eTagReturnLen;

    string_buffer(lastModified, 256);
} CopyObjectData;


static S3Status copyObjectXmlCallback(const char *elementPath,
                                      const char *data, int dataLen,
                                      void *callbackData)
{
    CopyObjectData *coData = (CopyObjectData *) callbackData;

    int fit;

    if (data) {
        if (!strcmp(elementPath, "CopyObjectResult/LastModified")) {
            string_buffer_append(coData->lastModified, data, dataLen, fit);
        }
        else if (!strcmp(elementPath, "CopyObjectResult/ETag")) {
            if (coData->eTagReturnSize && coData->eTagReturn) {
                coData->eTagReturnLen +=
                    snprintf(&(coData->eTagReturn[coData->eTagReturnLen]),
                             coData->eTagReturnSize -
                             coData->eTagReturnLen - 1,
                             "%.*s", dataLen, data);
                if (coData->eTagReturnLen >= coData->eTagReturnSize) {
                    return S3StatusXmlParseFailure;
                }
            }
        }
    }

    /* Avoid compiler error about variable set but not used */
    (void) fit;

    return S3StatusOK;
}


static S3Status copyObjectPropertiesCallback
    (const S3ResponseProperties *responseProperties, void *callbackData)
{
    CopyObjectData *coData = (CopyObjectData *) callbackData;

    return (*(coData->responsePropertiesCallback))
        (responseProperties, coData->callbackData);
}


static S3Status copyObjectDataCallback(int bufferSize, const char *buffer,
                                       void *callbackData)
{
    CopyObjectData *coData = (CopyObjectData *) callbackData;

    return simplexml_add(&(coData->simpleXml), buffer, bufferSize);
}


static void copyObjectCompleteCallback(S3Status requestStatus,
                                       const S3ErrorDetails *s3ErrorDetails,
                                       void *callbackData)
{
    CopyObjectData *coData = (CopyObjectData *) callbackData;

    if (coData->lastModifiedReturn) {
        time_t lastModified = -1;
        if (coData->lastModifiedLen) {
            lastModified = parseIso8601Time(coData->lastModified);
        }

        *(coData->lastModifiedReturn) = lastModified;
    }

    (*(coData->responseCompleteCallback))
        (requestStatus, s3ErrorDetails, coData->callbackData);

    simplexml_deinitialize(&(coData->simpleXml));

    free(coData);
}


void S3_copy_object(const S3BucketContext *bucketContext, const char *key,
                    const char *destinationBucket, const char *destinationKey,
                    const S3PutProperties *putProperties,
                    int64_t *lastModifiedReturn, int eTagReturnSize,
                    char *eTagReturn, S3RequestContext *requestContext,
                    int timeoutMs,
                    const S3ResponseHandler *handler, void *callbackData)
{
    /* Use the range copier with 0 length */
    S3_copy_object_range(bucketContext, key,
                         destinationBucket, destinationKey,
                         0, NULL, // No multipart
                         0, 0, // No length => std. copy of < 5GB
                         putProperties,
                         lastModifiedReturn, eTagReturnSize,
                         eTagReturn, requestContext,
                         timeoutMs,
                         handler, callbackData);
}


void S3_copy_object_range(const S3BucketContext *bucketContext, const char *key,
                          const char *destinationBucket,
                          const char *destinationKey, const int partNo,
                          const char *uploadId, const unsigned long startOffset,
                          const unsigned long count,
                          const S3PutProperties *putProperties,
                          int64_t *lastModifiedReturn, int eTagReturnSize,
                          char *eTagReturn, S3RequestContext *requestContext,
                          int timeoutMs,
                          const S3ResponseHandler *handler, void *callbackData)
{
    // Create the callback data
    CopyObjectData *data =
        (CopyObjectData *) malloc(sizeof(CopyObjectData));
    if (!data) {
        (*(handler->completeCallback))(S3StatusOutOfMemory, 0, callbackData);
        return;
    }

    simplexml_initialize(&(data->simpleXml), &copyObjectXmlCallback, data);

    data->responsePropertiesCallback = handler->propertiesCallback;
    data->responseCompleteCallback = handler->completeCallback;
    data->callbackData = callbackData;

    data->lastModifiedReturn = lastModifiedReturn;
    data->eTagReturnSize = eTagReturnSize;
    data->eTagReturn = eTagReturn;
    if (data->eTagReturnSize && data->eTagReturn) {
        data->eTagReturn[0] = 0;
    }
    data->eTagReturnLen = 0;
    string_buffer_initialize(data->lastModified);

    // If there's a sequence ID > 0 then add a subResource, OTW pass in NULL
    char queryParams[512];
    char *qp = NULL;
    if (partNo > 0) {
        snprintf(queryParams, 512, "partNumber=%d&uploadId=%s", partNo, uploadId);
        qp = queryParams;
    }

    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeCOPY,                          // httpRequestType
        { bucketContext->hostName,                    // hostName
          destinationBucket ? destinationBucket :
          bucketContext->bucketName,                  // bucketName
          bucketContext->protocol,                    // protocol
          bucketContext->uriStyle,                    // uriStyle
          bucketContext->accessKeyId,                 // accessKeyId
          bucketContext->secretAccessKey,             // secretAccessKey
          bucketContext->securityToken,               // securityToken
          bucketContext->authRegion },                // authRegion
        destinationKey ? destinationKey : key,        // key
        qp,                                           // queryParams
        0,                                            // subResource
        bucketContext->bucketName,                    // copySourceBucketName
        key,                                          // copySourceKey
        0,                                            // getConditions
        startOffset,                                  // startByte
        count,                                        // byteCount
        putProperties,                                // putProperties
        &copyObjectPropertiesCallback,                // propertiesCallback
        0,                                            // toS3Callback
        0,                                            // toS3CallbackTotalSize
        &copyObjectDataCallback,                      // fromS3Callback
        &copyObjectCompleteCallback,                  // completeCallback
        data,                                         // callbackData
        timeoutMs                                     // timeoutMs
    };

    // Perform the request
    request_perform(&params, requestContext);
}


// get object ----------------------------------------------------------------

void S3_get_object(const S3BucketContext *bucketContext, const char *key,
                   const S3GetConditions *getConditions,
                   uint64_t startByte, uint64_t byteCount,
                   S3RequestContext *requestContext,
                   int timeoutMs,
                   const S3GetObjectHandler *handler, void *callbackData)
{
    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeGET,                           // httpRequestType
        { bucketContext->hostName,                    // hostName
          bucketContext->bucketName,                  // bucketName
          bucketContext->protocol,                    // protocol
          bucketContext->uriStyle,                    // uriStyle
          bucketContext->accessKeyId,                 // accessKeyId
          bucketContext->secretAccessKey,             // secretAccessKey
          bucketContext->securityToken,               // securityToken
          bucketContext->authRegion },                // authRegion
        key,                                          // key
        0,                                            // queryParams
        0,                                            // subResource
        0,                                            // copySourceBucketName
        0,                                            // copySourceKey
        getConditions,                                // getConditions
        startByte,                                    // startByte
        byteCount,                                    // byteCount
        0,                                            // putProperties
        handler->responseHandler.propertiesCallback,  // propertiesCallback
        0,                                            // toS3Callback
        0,                                            // toS3CallbackTotalSize
        handler->getObjectDataCallback,               // fromS3Callback
        handler->responseHandler.completeCallback,    // completeCallback
        callbackData,                                 // callbackData
        timeoutMs                                     // timeoutMs
    };

    // Perform the request
    request_perform(&params, requestContext);
}


// head object ---------------------------------------------------------------

void S3_head_object(const S3BucketContext *bucketContext, const char *key,
                    S3RequestContext *requestContext,
                    int timeoutMs,
                    const S3ResponseHandler *handler, void *callbackData)
{
    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeHEAD,                          // httpRequestType
        { bucketContext->hostName,                    // hostName
          bucketContext->bucketName,                  // bucketName
          bucketContext->protocol,                    // protocol
          bucketContext->uriStyle,                    // uriStyle
          bucketContext->accessKeyId,                 // accessKeyId
          bucketContext->secretAccessKey,             // secretAccessKey
          bucketContext->securityToken,               // securityToken
          bucketContext->authRegion },                // authRegion
        key,                                          // key
        0,                                            // queryParams
        0,                                            // subResource
        0,                                            // copySourceBucketName
        0,                                            // copySourceKey
        0,                                            // getConditions
        0,                                            // startByte
        0,                                            // byteCount
        0,                                            // putProperties
        handler->propertiesCallback,                  // propertiesCallback
        0,                                            // toS3Callback
        0,                                            // toS3CallbackTotalSize
        0,                                            // fromS3Callback
        handler->completeCallback,                    // completeCallback
        callbackData,                                 // callbackData
        timeoutMs                                     // timeoutMs
    };

    // Perform the request
    request_perform(&params, requestContext);
}


// delete object --------------------------------------------------------------

void S3_delete_object(const S3BucketContext *bucketContext, const char *key,
                      S3RequestContext *requestContext,
                      int timeoutMs,
                      const S3ResponseHandler *handler, void *callbackData)
{
    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeDELETE,                        // httpRequestType
        { bucketContext->hostName,                    // hostName
          bucketContext->bucketName,                  // bucketName
          bucketContext->protocol,                    // protocol
          bucketContext->uriStyle,                    // uriStyle
          bucketContext->accessKeyId,                 // accessKeyId
          bucketContext->secretAccessKey,             // secretAccessKey
          bucketContext->securityToken,               // securityToken
          bucketContext->authRegion },                // authRegion
        key,                                          // key
        0,                                            // queryParams
        0,                                            // subResource
        0,                                            // copySourceBucketName
        0,                                            // copySourceKey
        0,                                            // getConditions
        0,                                            // startByte
        0,                                            // byteCount
        0,                                            // putProperties
        handler->propertiesCallback,                  // propertiesCallback
        0,                                            // toS3Callback
        0,                                            // toS3CallbackTotalSize
        0,                                            // fromS3Callback
        handler->completeCallback,                    // completeCallback
        callbackData,                                 // callbackData
        timeoutMs                                     // timeoutMs
    };

    // Perform the request
    request_perform(&params, requestContext);
}

// delete multiple objects --------------------------------------------------------------

// Assuming average overhead of 28 chars per key; maximum 1000 keys supported
#define MULTI_DELETE_XML_DOC_MAXSIZE ((S3_MAX_KEY_SIZE + 28) * 1000)

typedef struct DeleteMultipleObjectsData
{
    char md5Base64[MD5_BASE64_BUFFER_LENGTH];

    // bodge to add Content-Type header
    S3PutProperties putProperties;

    S3ResponsePropertiesCallback *responsePropertiesCallback;
    S3ResponseCompleteCallback *responseCompleteCallback;
    void *callbackData;

    string_buffer(deleteXmlDocument, MULTI_DELETE_XML_DOC_MAXSIZE);
    int  deleteXmlDocumentBytesWritten;

    string_buffer(deleteResponseXmlDocument, MULTI_DELETE_XML_DOC_MAXSIZE);

    int keysCount;;

    DeleteMultipleObjectSingleResult **results;
    int *resultsLen;

    int *errorCount;
} DeleteMultipleObjectsData;

static int deleteMultipleObjectDataCallback(int bufferSize, char *buffer, void *callbackData)
{
    DeleteMultipleObjectsData *dmoData = (DeleteMultipleObjectsData *) callbackData;

    int remaining = (dmoData->deleteXmlDocumentLen - dmoData->deleteXmlDocumentBytesWritten);
debug_printf("bufferSize: %d", bufferSize);
    int toCopy = bufferSize > remaining ? remaining : bufferSize;

    if (!toCopy) {
        return 0;
    }

    memcpy(buffer, &(dmoData->deleteXmlDocument
                     [dmoData->deleteXmlDocumentBytesWritten]), toCopy);

    dmoData->deleteXmlDocumentBytesWritten += toCopy;

    return toCopy;
}

static S3Status deleteMultipleObjectPropertiesCallback
    (const S3ResponseProperties *responseProperties, void *callbackData)
{
    DeleteMultipleObjectsData *dmoData = (DeleteMultipleObjectsData *) callbackData;

debug_printf("dmoData->responsePropertiesCallback %p", dmoData->responsePropertiesCallback);

    return (*(dmoData->responsePropertiesCallback))
        (responseProperties, dmoData->callbackData);
}

static S3Status deleteMultipleObjectResponseDataCallback(int bufferSize, const char *buffer,
                                   void *callbackData)
{
    DeleteMultipleObjectsData *dmoData = (DeleteMultipleObjectsData *) callbackData;

    int fit;

    string_buffer_append(dmoData->deleteResponseXmlDocument, buffer, bufferSize, fit);

    _return((fit ? S3StatusOK : S3StatusXmlDocumentTooLarge));
}

static S3Status convertDeleteMultipleObjectXmlCallback(const char *elementPath,
                                      const char *data, int dataLen,
                                      void *callbackData)
{
debug_printf("convertDeleteMultipleObjectXmlCallback: elementPath:%s, data:%.*s", elementPath, dataLen, data);
    DeleteMultipleObjectsData *dmoData = (DeleteMultipleObjectsData *) callbackData;
    static int errorCodeLen = 0;
    static char errorCode[100] = { '\0' };

    if (data) {
        if (dmoData->results) {
            int fit;
            int deletedNode = (0 == strcmp(elementPath, "DeleteResult/Deleted/Key"));
            int errorNode = (0 == strcmp(elementPath, "DeleteResult/Error/Key"));
            if (deletedNode || errorNode) {
                string_buffer_append(dmoData->results[*(dmoData->resultsLen)]->key, data, dataLen, fit);
                if (!fit)
                    return S3StatusKeyTooLong;
            }
            else if(!strcmp(elementPath, "DeleteResult/Error/Code")) {
                dmoData->results[*(dmoData->resultsLen)]->status = S3StatusErrorUnknown;
                string_buffer_append(errorCode, data, dataLen, fit);
                if (fit) {
                    if (!strcmp(errorCode, "AccessDenied"))
                        dmoData->results[*(dmoData->resultsLen)]->status = S3StatusErrorAccessDenied;
                    else if (!strcmp(errorCode, "InternalError"))
                        dmoData->results[*(dmoData->resultsLen)]->status = S3StatusErrorInternalError;
                }
            }
        }
    } else {
        if (dmoData->results) {
            int deletedNode = (0 == strcmp(elementPath, "DeleteResult/Deleted"));
            int errorNode = (0 == strcmp(elementPath, "DeleteResult/Error"));
            if (deletedNode || errorNode) {

                (*(dmoData->resultsLen))++;

                if (errorNode && dmoData->errorCount)
                    (*(dmoData->errorCount))++;

                if (*(dmoData->resultsLen) < dmoData->keysCount) {
debug_printf("Init: dmoData->resultsLen: %d", *(dmoData->resultsLen));
debug_printf("Init: dmoData->results[*(dmoData->resultsLen)]->key: %p", dmoData->results[*(dmoData->resultsLen)]->key);
                    string_buffer_initialize(dmoData->results[*(dmoData->resultsLen)]->key);
                    dmoData->results[*(dmoData->resultsLen)]->status = errorNode? S3StatusErrorUnknown: S3StatusOK;

                    string_buffer_initialize(errorCode);
                }
            }
        }
        else if(dmoData->errorCount && !strcmp(elementPath, "DeleteResult/Error")) {
            (*(dmoData->errorCount))++;
        }
    }

       return S3StatusOK;
}

S3Status S3_convert_delete_multiple_response(DeleteMultipleObjectsData *dmoData)
{
    if (dmoData->errorCount || dmoData->results) {
        // kill valgrind warning
        string_buffer_initialize(dmoData->results[0]->key);

        // Use a simplexml parser
        SimpleXml simpleXml;
        simplexml_initialize(&simpleXml, &convertDeleteMultipleObjectXmlCallback, dmoData);

        S3Status status = simplexml_add(&simpleXml, dmoData->deleteResponseXmlDocument, dmoData->deleteResponseXmlDocumentLen);

        simplexml_deinitialize(&simpleXml);

        return status;
    }

    return S3StatusOK;
}

static void deleteMultipleObjectCompleteCallback(S3Status requestStatus,
                                   const S3ErrorDetails *s3ErrorDetails,
                                   void *callbackData)
{
    DeleteMultipleObjectsData *dmoData = (DeleteMultipleObjectsData *) callbackData;

    if (requestStatus == S3StatusOK) {
        // Parse the document
        requestStatus = S3_convert_delete_multiple_response(dmoData);
    }

    (*(dmoData->responseCompleteCallback))
        (requestStatus, s3ErrorDetails, dmoData->callbackData);

    free(dmoData);
}

S3Status generateDeleteMultipleObjectsXmlDocument(DeleteMultipleObjectsData *dmoData, int keysCount, const char *keys[])
{
#define append(fmt, ...)                                        \
    do {                                                        \
        dmoData->deleteXmlDocumentLen += snprintf                       \
            (&(dmoData->deleteXmlDocument[dmoData->deleteXmlDocumentLen]),             \
             sizeof(dmoData->deleteXmlDocument) - dmoData->deleteXmlDocumentLen - 1, \
             fmt, __VA_ARGS__);                                 \
        if ((unsigned int)dmoData->deleteXmlDocumentLen >= sizeof(dmoData->deleteXmlDocument)) {   \
            _return(S3StatusXmlDocumentTooLarge);                 \
        } \
    } while (0)

    append("%s", "<Delete><Quiet>false</Quiet>");
    int i = 0;
    for (; i < keysCount; ++i) {
        append("<Object><Key>%s</Key></Object>", keys[i]);
    }
    append("%s", "</Delete>");

    debug_printf("dmoData->deleteXmlDocumentLen: %*s", dmoData->deleteXmlDocumentLen, dmoData->deleteXmlDocument);

    return S3StatusOK;
}

void S3_delete_multiple_objects(const S3BucketContext *bucketContext,
                      int keysCount, const char *keys[],
                      DeleteMultipleObjectSingleResult **results, int *resultsLen, int *errorCount,
                      S3RequestContext *requestContext,
                      int timeoutMs,
                      const S3ResponseHandler *handler, void *callbackData)
{
#ifdef __APPLE__
    /* This request requires calculating MD5 sum.
     * MD5 sum requires OpenSSL library, which is not used on Apple.
     * TODO Implement some MD5+Base64 caculation on Apple
     */
    (*(handler->completeCallback))(S3StatusNotSupported, 0, callbackData);
    return;
#else
    DeleteMultipleObjectsData *dmoData = (DeleteMultipleObjectsData *) malloc(sizeof(DeleteMultipleObjectsData));
    if (!dmoData) {
        (*(handler->completeCallback))(S3StatusOutOfMemory, 0, callbackData);
        return;
    }

    dmoData->responsePropertiesCallback = handler->propertiesCallback;
    dmoData->responseCompleteCallback = handler->completeCallback;
    dmoData->callbackData = callbackData;

    dmoData->errorCount = errorCount;

    if (resultsLen && results) {
        dmoData->results = results;
        dmoData->resultsLen = resultsLen;

        // initialise the result length to 0
        *resultsLen = 0;
    } else {
        dmoData->results = 0;
        dmoData->resultsLen = 0;
    }

    dmoData->keysCount = keysCount;

    string_buffer_initialize(dmoData->deleteXmlDocument);
    dmoData->deleteXmlDocumentBytesWritten = 0;

    string_buffer_initialize(dmoData->deleteResponseXmlDocument);

    *errorCount = 0;

    S3Status status = generateDeleteMultipleObjectsXmlDocument(dmoData, keysCount, keys);
    if (status != S3StatusOK) {
        free(dmoData);
        (*(handler->completeCallback))(status, 0, callbackData);
        return;
    }

    // md5 for the request data

    generate_content_md5(dmoData->deleteXmlDocument, dmoData->deleteXmlDocumentLen, dmoData->md5Base64, sizeof(dmoData->md5Base64));

    dmoData->putProperties = (S3PutProperties){ "application/xml", dmoData->md5Base64, 0, 0, 0, 0, 0, 0, 0, 0 };

    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypePOST,                  // httpRequestType
        { bucketContext->hostName                   , // hostname
          bucketContext->bucketName,                  // bucketName
          bucketContext->protocol,                    // protocol
          bucketContext->uriStyle,                    // uriStyle
          bucketContext->accessKeyId,                 // accessKeyId
          bucketContext->secretAccessKey,             // secretAccessKey
          bucketContext->securityToken,               // securityToken
          bucketContext->authRegion },                // authRegion
        0,                                            // key
        0,                                            // queryParams
        "delete",                                     // subResource
        0,                                            // copySourceBucketName
        0,                                            // copySourceKey
        0,                                            // getConditions
        0,                                            // startByte
        0,                                            // byteCount
        &dmoData->putProperties,                      // putProperties
        &deleteMultipleObjectPropertiesCallback,      // propertiesCallback
        &deleteMultipleObjectDataCallback,            // toS3Callback
        dmoData->deleteXmlDocumentLen,                // toS3CallbackTotalSize
        &deleteMultipleObjectResponseDataCallback,    // fromS3Callback
        &deleteMultipleObjectCompleteCallback,        // completeCallback
        dmoData,                                      // callbackData
        timeoutMs                                     // timeoutMs
    };

    // Perform the request
    request_perform(&params, requestContext);
#endif
}
