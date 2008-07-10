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

#include <string.h>
#include <stdlib.h>
#include "libs3.h"
#include "request.h"
#include "simplexml.h"

// test bucket ----------------------------------------------------------------

typedef struct TestBucketData
{
    SimpleXml simpleXml;

    S3ResponseHeadersCallback *responseHeadersCallback;
    S3ResponseCompleteCallback *responseCompleteCallback;
    void *callbackData;

    int locationConstraintReturnSize;
    char *locationConstraintReturn;

    string_buffer(locationConstraint, 256);
} TestBucketData;


static S3Status testBucketXmlCallback(const char *elementPath, const char *data,
                                      int dataLen, void *callbackData)
{
    TestBucketData *tbData = (TestBucketData *) callbackData;

    int fit;

    if (data && !strcmp(elementPath, "LocationConstraint")) {
        string_buffer_append(tbData->locationConstraint, data, dataLen, fit);
    }

    return S3StatusOK;
}


static S3Status testBucketHeadersCallback
    (const S3ResponseHeaders *responseHeaders, void *callbackData)
{
    TestBucketData *tbData = (TestBucketData *) callbackData;
    
    return (*(tbData->responseHeadersCallback))
        (responseHeaders, tbData->callbackData);
}


static int testBucketDataCallback(char *buffer, int bufferSize,
                                  void *callbackData)
{
    TestBucketData *tbData = (TestBucketData *) callbackData;

    return ((simplexml_add(&(tbData->simpleXml), buffer, 
                           bufferSize) == S3StatusOK) ? bufferSize : 0);
}


static void testBucketCompleteCallback(S3Status requestStatus, 
                                       int httpResponseCode, 
                                       const S3ErrorDetails *s3ErrorDetails,
                                       void *callbackData)
{
    TestBucketData *tbData = (TestBucketData *) callbackData;

    // Copy the location constraint into the return buffer
    snprintf(tbData->locationConstraintReturn, 
             tbData->locationConstraintReturnSize, "%s", 
             tbData->locationConstraint);

    (*(tbData->responseCompleteCallback))
        (requestStatus, httpResponseCode, s3ErrorDetails, 
         tbData->callbackData);

    simplexml_deinitialize(&(tbData->simpleXml));

    free(tbData);
}


void S3_test_bucket(S3Protocol protocol, S3UriStyle uriStyle,
                    const char *accessKeyId, const char *secretAccessKey,
                    const char *bucketName, int locationConstraintReturnSize,
                    char *locationConstraintReturn,
                    S3RequestContext *requestContext,
                    S3ResponseHandler *handler, void *callbackData)
{
    // Create the callback data
    TestBucketData *tbData = (TestBucketData *) malloc(sizeof(TestBucketData));
    if (!tbData) {
        (*(handler->completeCallback))
            (S3StatusOutOfMemory, 0, 0, callbackData);
        return;
    }

    S3Status status = simplexml_initialize
        (&(tbData->simpleXml), &testBucketXmlCallback, tbData);
    if (status != S3StatusOK) {
        free(tbData);
        (*(handler->completeCallback))(status, 0, 0, callbackData);
        return;
    }

    tbData->responseHeadersCallback = handler->headersCallback;
    tbData->responseCompleteCallback = handler->completeCallback;
    tbData->callbackData = callbackData;

    tbData->locationConstraintReturnSize = locationConstraintReturnSize;
    tbData->locationConstraintReturn = locationConstraintReturn;
    string_buffer_initialize(tbData->locationConstraint);

    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeGET,                 // httpRequestType
        protocol,                           // protocol
        uriStyle,                           // uriStyle
        bucketName,                         // bucketName
        0,                                  // key
        0,                                  // queryParams
        "?location",                        // subResource
        accessKeyId,                        // accessKeyId
        secretAccessKey,                    // secretAccessKey
        0,                                  // requestHeaders
        &testBucketHeadersCallback,         // headersCallback
        0,                                  // toS3Callback
        0,                                  // toS3CallbackTotalSize
        &testBucketDataCallback,            // fromS3Callback
        &testBucketCompleteCallback,        // completeCallback
        tbData                              // callbackData
    };

    // Perform the request
    request_perform(&params, requestContext);
}


// create bucket --------------------------------------------------------------

typedef struct CreateBucketData
{
    S3ResponseHeadersCallback *responseHeadersCallback;
    S3ResponseCompleteCallback *responseCompleteCallback;
    void *callbackData;

    char doc[1024];
    int docLen, docBytesWritten;
} CreateBucketData;                         
                            

static S3Status createBucketHeadersCallback
    (const S3ResponseHeaders *responseHeaders, void *callbackData)
{
    CreateBucketData *cbData = (CreateBucketData *) callbackData;
    
    return (*(cbData->responseHeadersCallback))
        (responseHeaders, cbData->callbackData);
}


static int createBucketDataCallback(char *buffer, int bufferSize,
                                    void *callbackData)
{
    CreateBucketData *cbData = (CreateBucketData *) callbackData;

    if (!cbData->docLen) {
        return 0;
    }

    int remaining = (cbData->docLen - cbData->docBytesWritten);

    int toCopy = bufferSize > remaining ? remaining : bufferSize;

    if (!toCopy) {
        return 0;
    }

    memcpy(buffer, &(cbData->doc[cbData->docBytesWritten]), toCopy);

    cbData->docBytesWritten += toCopy;

    return toCopy;
}


static void createBucketCompleteCallback(S3Status requestStatus, 
                                         int httpResponseCode, 
                                         const S3ErrorDetails *s3ErrorDetails,
                                         void *callbackData)
{
    CreateBucketData *cbData = (CreateBucketData *) callbackData;

    (*(cbData->responseCompleteCallback))
        (requestStatus, httpResponseCode, s3ErrorDetails, 
         cbData->callbackData);

    free(cbData);
}


void S3_create_bucket(S3Protocol protocol, const char *accessKeyId,
                      const char *secretAccessKey, const char *bucketName,
                      S3CannedAcl cannedAcl, const char *locationConstraint,
                      S3RequestContext *requestContext,
                      S3ResponseHandler *handler, void *callbackData)
{
    // Create the callback data
    CreateBucketData *cbData = 
        (CreateBucketData *) malloc(sizeof(CreateBucketData));
    if (!cbData) {
        (*(handler->completeCallback))
            (S3StatusOutOfMemory, 0, 0, callbackData);
        return;
    }

    cbData->responseHeadersCallback = handler->headersCallback;
    cbData->responseCompleteCallback = handler->completeCallback;
    cbData->callbackData = callbackData;

    if (locationConstraint) {
        cbData->docLen =
            snprintf(cbData->doc, sizeof(cbData->doc),
                     "<CreateBucketConfiguration><LocationConstraint>"
                     "%s</LocationConstraint></CreateBucketConfiguration>",
                     locationConstraint);
        cbData->docBytesWritten = 0;
    }
    else {
        cbData->docLen = 0;
    }
    
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
        &createBucketHeadersCallback,       // headersCallback
        &createBucketDataCallback,          // toS3Callback
        cbData->docLen,                     // toS3CallbackTotalSize
        0,                                  // fromS3Callback
        &createBucketCompleteCallback,      // completeCallback
        cbData                              // callbackData
    };

    // Perform the request
    request_perform(&params, requestContext);
}

                           
// delete bucket --------------------------------------------------------------

typedef struct DeleteBucketData
{
    S3ResponseHeadersCallback *responseHeadersCallback;
    S3ResponseCompleteCallback *responseCompleteCallback;
    void *callbackData;
} DeleteBucketData;


static S3Status deleteBucketHeadersCallback
    (const S3ResponseHeaders *responseHeaders, void *callbackData)
{
    DeleteBucketData *dbData = (DeleteBucketData *) callbackData;
    
    return (*(dbData->responseHeadersCallback))
        (responseHeaders, dbData->callbackData);
}


static void deleteBucketCompleteCallback(S3Status requestStatus, 
                                         int httpResponseCode, 
                                         const S3ErrorDetails *s3ErrorDetails,
                                         void *callbackData)
{
    DeleteBucketData *dbData = (DeleteBucketData *) callbackData;

    (*(dbData->responseCompleteCallback))
        (requestStatus, httpResponseCode, s3ErrorDetails, 
         dbData->callbackData);

    free(dbData);
}


void S3_delete_bucket(S3Protocol protocol, S3UriStyle uriStyle,
                      const char *accessKeyId, const char *secretAccessKey,
                      const char *bucketName, S3RequestContext *requestContext,
                      S3ResponseHandler *handler, void *callbackData)
{
    // Create the callback data
    DeleteBucketData *dbData = 
        (DeleteBucketData *) malloc(sizeof(DeleteBucketData));
    if (!dbData) {
        (*(handler->completeCallback))
            (S3StatusOutOfMemory, 0, 0, callbackData);
        return;
    }

    dbData->responseHeadersCallback = handler->headersCallback;
    dbData->responseCompleteCallback = handler->completeCallback;
    dbData->callbackData = callbackData;

    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeDELETE,              // httpRequestType
        protocol,                           // protocol
        uriStyle,                           // uriStyle
        bucketName,                         // bucketName
        0,                                  // key
        0,                                  // queryParams
        0,                                  // subResource
        accessKeyId,                        // accessKeyId
        secretAccessKey,                    // secretAccessKey
        0,                                  // requestHeaders 
        &deleteBucketHeadersCallback,       // headersCallback
        0,                                  // toS3Callback
        0,                                  // toS3CallbackTotalSize
        0,                                  // fromS3Callback
        &deleteBucketCompleteCallback,      // completeCallback
        dbData                              // callbackData
    };

    // Perform the request
    request_perform(&params, requestContext);
}


void S3_list_bucket(S3BucketContext *bucketContext, const char *prefix,
                    const char *marker, const char *delimiter, int maxkeys,
                    S3RequestContext *requestContext,
                    S3ListBucketHandler *handler, void *callbackData)
{
}
