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

#include <stdlib.h>
#include <string.h>
#include "private.h"


static size_t write_callback(void *data, size_t s, size_t n, void *req)
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
    
    printf("data: %s", str);
    if (c) {
        printf("%c\n", c);
    }

    return len;
}


S3Status S3_list_service(S3Protocol protocol, const char *accessKeyId,
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
        0                                   // curlReadCallback
    };

    // Perform the request
    return request_perform(&params, requestContext);
}
