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
#include "private.h"

static S3Status xmlCallback(const char *elementPath, const char *data,
                            int dataLen, void *callbackData)
{
    Request *request = (Request *) callbackData;

    ListServiceXmlCallbackData *cbData = 
        (ListServiceXmlCallbackData *) 
        &(request->data.listServiceXmlCallbackData);

    if (data) {
        if (!strcmp(elementPath, "ListAllMyBucketsResult/Owner/ID")) {
            cbData->ownerIdLen += 
                snprintf(&(cbData->ownerId[cbData->ownerIdLen]), 
                         sizeof(cbData->ownerId) - cbData->ownerIdLen - 1,
                         "%.*s", dataLen, data) + 1;
        }
        else if (!strcmp(elementPath, 
                         "ListAllMyBucketsResult/Owner/DisplayName")) {
            cbData->ownerDisplayNameLen +=
                snprintf(&(cbData->ownerDisplayName
                           [cbData->ownerDisplayNameLen]),
                         sizeof(cbData->ownerDisplayName) - 
                         cbData->ownerDisplayNameLen - 1, 
                         "%.*s", dataLen, data);
        }
        else if (!strcmp(elementPath, 
                         "ListAllMyBucketsResult/Buckets/Bucket/Name")) {
            cbData->bucketNameLen += 
                snprintf(cbData->bucketName, 
                         sizeof(cbData->bucketName) - 
                         cbData->bucketNameLen - 1, 
                         "%.*s", dataLen, data);
        }
        else if (!strcmp
                 (elementPath, 
                  "ListAllMyBucketsResult/Buckets/Bucket/CreationDate")) {
            cbData->creationDateLen +=
                snprintf(cbData->creationDate, 
                         sizeof(cbData->creationDate) - 
                         cbData->creationDateLen - 1, 
                         "%.*s", dataLen, data);
        }
    }
    else {
        if (!strcmp(elementPath, "ListAllMyBucketsResult/Buckets/Bucket")) {
            // Convert the date into a struct tm
            int hasCreationDate = 0;
            time_t creationDate = 0;
            int millis = 0;
            // Parse date.  Assume ISO-8601 date format.
            //           1         2
            // 012345678901234567890
            // YYYY-MM-DDThh:mm:ss.sTZD
            if ((cbData->creationDateLen > 19) &&
                isdigit(cbData->creationDate[0]) &&
                isdigit(cbData->creationDate[1]) &&
                isdigit(cbData->creationDate[2]) &&
                isdigit(cbData->creationDate[3]) &&
                (cbData->creationDate[4] == '-') &&
                isdigit(cbData->creationDate[5]) &&
                isdigit(cbData->creationDate[6]) &&
                (cbData->creationDate[7] == '-') &&
                isdigit(cbData->creationDate[8]) &&
                isdigit(cbData->creationDate[9]) &&
                (cbData->creationDate[10] == 'T') &&
                isdigit(cbData->creationDate[11]) &&
                isdigit(cbData->creationDate[12]) &&
                (cbData->creationDate[13] == ':') &&
                isdigit(cbData->creationDate[14]) &&
                isdigit(cbData->creationDate[15]) &&
                (cbData->creationDate[16] == ':') &&
                isdigit(cbData->creationDate[17]) &&
                isdigit(cbData->creationDate[18])) {
                struct tm stm;
                memset(&stm, 0, sizeof(stm));
                stm.tm_sec = (((cbData->creationDate[17] - '0') * 10) +
                              (cbData->creationDate[18] - '0'));
                stm.tm_min = (((cbData->creationDate[14] - '0') * 10) +
                             (cbData->creationDate[15] - '0'));
                stm.tm_hour = (((cbData->creationDate[11] - '0') * 10) +
                               (cbData->creationDate[12] - '0'));
                stm.tm_mday = (((cbData->creationDate[8] - '0') * 10) +
                               (cbData->creationDate[9] - '0'));
                stm.tm_mon = ((((cbData->creationDate[5] - '0') * 10) +
                               (cbData->creationDate[6] - '0')) - 1);
                stm.tm_year = ((((cbData->creationDate[0] - '0') * 1000) +
                                ((cbData->creationDate[1] - '0') * 100) +
                                ((cbData->creationDate[2] - '0') * 10) + 
                                (cbData->creationDate[3] - '0')) - 1900);
                stm.tm_isdst = -1;
                
                creationDate = mktime(&stm);

                int tzStart = 19;
                if (cbData->creationDate[tzStart] == '.') {
                    tzStart++;
                    while (isdigit(cbData->creationDate[tzStart])) {
                        millis *= 10;
                        millis += cbData->creationDate[tzStart++] - '0';
                    }
                }
                else {
                    tzStart = 19;
                }

                if (((cbData->creationDate[tzStart] == '-') ||
                     (cbData->creationDate[tzStart] == '+')) &&
                    isdigit(cbData->creationDate[tzStart + 1]) &&
                    isdigit(cbData->creationDate[tzStart + 2]) &&
                    (cbData->creationDate[tzStart + 3] == ':') &&
                    isdigit(cbData->creationDate[tzStart + 4]) &&
                    isdigit(cbData->creationDate[tzStart + 5])) {
                    int tzOffsetMinutes = 
                        (((((cbData->creationDate[tzStart + 1] - '0') * 10) +
                           (cbData->creationDate[tzStart + 2] - '0')) * 60) +
                         ((cbData->creationDate[tzStart + 4] - '0') * 10) +
                         (cbData->creationDate[tzStart + 5] - '0'));
                    if (cbData->creationDate[tzStart] == '-') {
                        creationDate += (tzOffsetMinutes * 60);
                    }
                    else {
                        creationDate -= (tzOffsetMinutes * 60);
                    }
                }
                // Else it should be 'Z', but we just assume it is
                
                hasCreationDate = 1;
            }            

            // Make the callback - a bucket just finished
            request->status = (*(request->callback.listServiceCallback))
                (cbData->ownerId, cbData->ownerDisplayName,
                 cbData->bucketName, hasCreationDate ? creationDate : -1,
                 hasCreationDate ? millis : 0, request->callbackData);
            cbData->ownerId[0] = 0;
            cbData->ownerIdLen = 0;
            cbData->ownerDisplayName[0] = 0;
            cbData->ownerDisplayNameLen = 0;
            cbData->bucketName[0] = 0;
            cbData->bucketNameLen = 0;
            cbData->creationDate[0] = 0;
            cbData->creationDateLen = 0;
            return request->status;
        }
    }

    return S3StatusOK;
}


static size_t write_callback(void *data, size_t s, size_t n, void *req)
{
    Request *request = (Request *) req;

    if (!request->dataXmlParserInitialized) {
        if (simplexml_initialize(&(request->dataXmlParser),
                                 &xmlCallback, request) != S3StatusOK) {
            return 0;
        }
        request->dataXmlParserInitialized = 1;
        ListServiceXmlCallbackData *data = 
            (ListServiceXmlCallbackData *) 
            &(request->data.listServiceXmlCallbackData);
        data->ownerId[0] = 0;
        data->ownerIdLen = 0;
        data->ownerDisplayName[0] = 0;
        data->ownerDisplayNameLen = 0;
        data->bucketName[0] = 0;
        data->bucketNameLen = 0;
        data->creationDate[0] = 0;
        data->creationDateLen = 0;
    }

    // Feed the data to the xml parser
    return ((simplexml_add(&(request->dataXmlParser), 
                           (char *) data, s * n) == S3StatusOK) ?
            (s * n) : 0);
}


void S3_list_service(S3Protocol protocol, const char *accessKeyId,
                     const char *secretAccessKey,
                     S3RequestContext *requestContext,
                     S3ListServiceHandler *handler, void *callbackData)
{
    // Set up the RequestParams
    RequestParams params =
    {
        HttpRequestTypeGET,                 // httpRequestType
        protocol,                           // protocol
        S3UriStylePath,                     // uriStyle
        0,                                  // bucketName
        0,                                  // key
        0,                                  // queryParams
        0,                                  // subResource
        accessKeyId,                        // accessKeyId
        secretAccessKey,                    // secretAccessKey
        0,                                  // requestHeaders 
        &(handler->responseHandler),        // handler
        { handler->listServiceCallback },   // special callbacks
        callbackData,                       // callbackData
        &write_callback,                    // curlWriteCallback
        0,                                  // curlReadCallback
        0                                   // readSize
    };

    // Perform the request
    request_perform(&params, requestContext);
}


