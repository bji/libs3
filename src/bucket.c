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


static S3Status testBucketDataCallback(int bufferSize, const char *buffer,
                                       void *callbackData)
{
    TestBucketData *tbData = (TestBucketData *) callbackData;

    return simplexml_add(&(tbData->simpleXml), buffer, bufferSize);
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


static int createBucketDataCallback(int bufferSize, char *buffer, 
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


// list bucket ----------------------------------------------------------------

typedef struct ListBucketContents
{
    string_buffer(key, 1024);
    string_buffer(lastModified, 256);
    string_buffer(eTag, 256);
    string_buffer(size, 24);
    string_buffer(ownerId, 256);
    string_buffer(ownerDisplayName, 256);
} ListBucketContents;


static void initialize_list_bucket_contents(ListBucketContents *contents)
{
    string_buffer_initialize(contents->key);
    string_buffer_initialize(contents->lastModified);
    string_buffer_initialize(contents->eTag);
    string_buffer_initialize(contents->size);
    string_buffer_initialize(contents->ownerId);
    string_buffer_initialize(contents->ownerDisplayName);
}

// We read up to 32 Contents at a time
#define MAX_CONTENTS 32
// We read up to 8 CommonPrefixes at a time
#define MAX_COMMON_PREFIXES 8

typedef struct ListBucketData
{
    SimpleXml simpleXml;

    S3ResponseHeadersCallback *responseHeadersCallback;
    S3ListBucketCallback *listBucketCallback;
    S3ResponseCompleteCallback *responseCompleteCallback;
    void *callbackData;

    string_buffer(isTruncated, 64);
    string_buffer(nextMarker, 1024);

    int contentsCount;
    ListBucketContents contents[MAX_CONTENTS];

    int commonPrefixesCount;
    char commonPrefixes[MAX_COMMON_PREFIXES][1024];
    int commonPrefixLens[MAX_COMMON_PREFIXES];
} ListBucketData;


static void initialize_list_bucket_data(ListBucketData *lbData)
{
    lbData->contentsCount = 0;
    initialize_list_bucket_contents(lbData->contents);
    lbData->commonPrefixesCount = 0;
    lbData->commonPrefixes[0][0] = 0;
    lbData->commonPrefixLens[0] = 0;
}


static S3Status make_list_bucket_callback(ListBucketData *lbData)
{
    int i;

    // Convert IsTruncated
    int isTruncated = (!strcmp(lbData->isTruncated, "true") ||
                       !strcmp(lbData->isTruncated, "1")) ? 1 : 0;

    // Convert the contents
    S3ListBucketContent contents[lbData->contentsCount];

    int contentsCount = lbData->contentsCount;
    for (i = 0; i < contentsCount; i++) {
        S3ListBucketContent *contentDest = &(contents[i]);
        ListBucketContents *contentSrc = &(lbData->contents[i]);
        contentDest->key = contentSrc->key;
        contentDest->lastModified = parseIso8601Time(contentSrc->lastModified);
        contentDest->eTag = contentSrc->eTag;
        contentDest->size = parseUnsignedInt(contentSrc->size);
        contentDest->ownerId = contentSrc->ownerId[0] ?contentSrc->ownerId : 0;
        contentDest->ownerDisplayName = (contentSrc->ownerDisplayName[0] ?
                                         contentSrc->ownerDisplayName : 0);
    }

    // Make the common prefixes array
    int commonPrefixesCount = lbData->commonPrefixesCount;
    char *commonPrefixes[commonPrefixesCount];
    for (i = 0; i < commonPrefixesCount; i++) {
        commonPrefixes[i] = lbData->commonPrefixes[i];
    }

    return (*(lbData->listBucketCallback))
        (isTruncated, lbData->nextMarker,
         contentsCount, contents, commonPrefixesCount, 
         (const char **) commonPrefixes, lbData->callbackData);
}


static S3Status listBucketXmlCallback(const char *elementPath, const char *data,
                                      int dataLen, void *callbackData)
{
    ListBucketData *lbData = (ListBucketData *) callbackData;

    int fit;

    if (data) {
        if (!strcmp(elementPath, "ListBucketResult/IsTruncated")) {
            string_buffer_append(lbData->isTruncated, data, dataLen, fit);
        }
        else if (!strcmp(elementPath, "ListBucketResult/NextMarker")) {
            string_buffer_append(lbData->nextMarker, data, dataLen, fit);
        }
        else if (!strcmp(elementPath, "ListBucketResult/Contents/Key")) {
            ListBucketContents *contents = 
                &(lbData->contents[lbData->contentsCount]);
            string_buffer_append(contents->key, data, dataLen, fit);
        }
        else if (!strcmp(elementPath, 
                         "ListBucketResult/Contents/LastModified")) {
            ListBucketContents *contents = 
                &(lbData->contents[lbData->contentsCount]);
            string_buffer_append(contents->lastModified, data, dataLen, fit);
        }
        else if (!strcmp(elementPath, "ListBucketResult/Contents/ETag")) {
            ListBucketContents *contents = 
                &(lbData->contents[lbData->contentsCount]);
            string_buffer_append(contents->eTag, data, dataLen, fit);
        }
        else if (!strcmp(elementPath, "ListBucketResult/Contents/Size")) {
            ListBucketContents *contents = 
                &(lbData->contents[lbData->contentsCount]);
            string_buffer_append(contents->size, data, dataLen, fit);
        }
        else if (!strcmp(elementPath, "ListBucketResult/Contents/Owner/ID")) {
            ListBucketContents *contents = 
                &(lbData->contents[lbData->contentsCount]);
            string_buffer_append(contents->ownerId, data, dataLen, fit);
        }
        else if (!strcmp(elementPath, 
                         "ListBucketResult/Contents/Owner/DisplayName")) {
            ListBucketContents *contents = 
                &(lbData->contents[lbData->contentsCount]);
            string_buffer_append
                (contents->ownerDisplayName, data, dataLen, fit);
        }
        else if (!strcmp(elementPath, "ListBucketResult/CommonPrefix/Prefix")) {
            int which = lbData->commonPrefixesCount;
            lbData->commonPrefixLens[which] +=
                snprintf(lbData->commonPrefixes[which],
                         sizeof(lbData->commonPrefixes[which]) -
                         lbData->commonPrefixLens[which] - 1,
                         "%.*s", dataLen, data);
        }
    }
    else {
        if (!strcmp(elementPath, "ListBucketResult/Contents")) {
            // Finished a Contents
            lbData->contentsCount++;
            if (lbData->contentsCount == MAX_CONTENTS) {
                // Make the callback
                S3Status status = make_list_bucket_callback(lbData);
                if (status != S3StatusOK) {
                    return status;
                }
                initialize_list_bucket_data(lbData);
            }
            else {
                // Initialize the next one
                initialize_list_bucket_contents
                    (&(lbData->contents[lbData->contentsCount]));
            }
        }
        else if (!strcmp(elementPath, "ListBucketResult/CommonPrefix/Prefix")) {
            // Finished a Prefix
            lbData->commonPrefixesCount++;
            if (lbData->commonPrefixesCount == MAX_COMMON_PREFIXES) {
                // Make the callback
                S3Status status = make_list_bucket_callback(lbData);
                if (status != S3StatusOK) {
                    return status;
                }
                initialize_list_bucket_data(lbData);
            }
            else {
                // Initialize the next one
                lbData->commonPrefixes[lbData->commonPrefixesCount][0] = 0;
                lbData->commonPrefixLens[lbData->commonPrefixesCount] = 0;
            }
        }
    }

    return S3StatusOK;
}


static S3Status listBucketHeadersCallback
    (const S3ResponseHeaders *responseHeaders, void *callbackData)
{
    ListBucketData *lbData = (ListBucketData *) callbackData;
    
    return (*(lbData->responseHeadersCallback))
        (responseHeaders, lbData->callbackData);
}


static S3Status listBucketDataCallback(int bufferSize, const char *buffer, 
                                       void *callbackData)
{
    ListBucketData *lbData = (ListBucketData *) callbackData;
    
    return simplexml_add(&(lbData->simpleXml), buffer, bufferSize);
}


static void listBucketCompleteCallback(S3Status requestStatus, 
                                       int httpResponseCode, 
                                       const S3ErrorDetails *s3ErrorDetails,
                                       void *callbackData)
{
    ListBucketData *lbData = (ListBucketData *) callbackData;

    // Make the callback if there is anything
    if (lbData->contentsCount || lbData->commonPrefixesCount) {
        make_list_bucket_callback(lbData);
    }

    free(lbData);
}


void S3_list_bucket(S3BucketContext *bucketContext, const char *prefix,
                    const char *marker, const char *delimiter, int maxkeys,
                    S3RequestContext *requestContext,
                    S3ListBucketHandler *handler, void *callbackData)
{
    // Compose the query params
    string_buffer(queryParams, 4096);
    string_buffer_initialize(queryParams);
    
#define safe_append(name, value)                                        \
    do {                                                                \
        int fit;                                                        \
        string_buffer_append(queryParams, &sep, 1, fit);                \
        if (!fit) {                                                     \
            (*(handler->responseHandler.completeCallback))              \
                (S3StatusQueryParamsTooLong, 0, 0, callbackData);       \
            return;                                                     \
        }                                                               \
        string_buffer_append(queryParams, name "=",                     \
                             sizeof(name "=") - 1, fit);                \
        if (!fit) {                                                     \
            (*(handler->responseHandler.completeCallback))              \
                (S3StatusQueryParamsTooLong, 0, 0, callbackData);       \
            return;                                                     \
        }                                                               \
        sep = '&';                                                      \
        char encoded[3 * 1024];                                         \
        if (!urlEncode(encoded, value, 1024)) {                         \
            (*(handler->responseHandler.completeCallback))              \
                (S3StatusQueryParamsTooLong, 0, 0, callbackData);       \
        }                                                               \
        string_buffer_append(queryParams, encoded, strlen(encoded),     \
                             fit);                                      \
        if (!fit) {                                                     \
            (*(handler->responseHandler.completeCallback))              \
                (S3StatusQueryParamsTooLong, 0, 0, callbackData);       \
            return;                                                     \
        }                                                               \
    } while (0)


    char sep = '?';
    if (prefix) {
        safe_append("prefix", prefix);
    }
    if (marker) {
        safe_append("marker", marker);
    }
    if (delimiter) {
        safe_append("delimiter", delimiter);
    }
    if (maxkeys) {
        char maxKeysString[64];
        snprintf(maxKeysString, sizeof(maxKeysString), "%d", maxkeys);
        safe_append("max-keys", maxKeysString);
    }

    ListBucketData *lbData = (ListBucketData *) malloc(sizeof(ListBucketData));

    if (!lbData) {
        (*(handler->responseHandler.completeCallback))
            (S3StatusOutOfMemory, 0, 0, callbackData);
        return;
    }

    S3Status status = simplexml_initialize
        (&(lbData->simpleXml), &listBucketXmlCallback, lbData);
    if (status != S3StatusOK) {
        free(lbData);
        (*(handler->responseHandler.completeCallback))
            (status, 0, 0, callbackData);
        return;
    }
    
    lbData->responseHeadersCallback = handler->responseHandler.headersCallback;
    lbData->listBucketCallback = handler->listBucketCallback;
    lbData->responseCompleteCallback = 
        handler->responseHandler.completeCallback;
    lbData->callbackData = callbackData;

    string_buffer_initialize(lbData->isTruncated);
    string_buffer_initialize(lbData->nextMarker);
    initialize_list_bucket_data(lbData);

    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeGET,                 // httpRequestType
        bucketContext->protocol,            // protocol
        bucketContext->uriStyle,            // uriStyle
        bucketContext->bucketName,          // bucketName
        0,                                  // key
        queryParams[0] ? queryParams : 0,   // queryParams
        0,                                  // subResource
        bucketContext->accessKeyId,         // accessKeyId
        bucketContext->secretAccessKey,     // secretAccessKey
        0,                                  // requestHeaders
        &listBucketHeadersCallback,         // headersCallback
        0,                                  // toS3Callback
        0,                                  // toS3CallbackTotalSize
        &listBucketDataCallback,            // fromS3Callback
        &listBucketCompleteCallback,        // completeCallback
        lbData                              // callbackData
    };

    // Perform the request
    request_perform(&params, requestContext);
}
