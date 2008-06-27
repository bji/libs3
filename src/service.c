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
#include "libs3.h"
#include "private.h"


static size_t write_function(void *ptr, size_t size, size_t nmemb,
                             void *stream)
{
    PrivateData *pd = (PrivateData *) stream;
    (void) pd;

    char *data = (char *) malloc((size * nmemb) + 1);
    memcpy(data, ptr, size * nmemb);
    data[size * nmemb] = 0;

    printf("data: %s\n", data);

    free(data);

    return (size * nmemb);
}

S3Status S3_list_service(const char *accessKeyId, const char *secretAccessKey,
                         S3RequestContext *requestContext,
                         S3ListServiceHandler *handler, void *callbackData)
{
    PrivateData *privateData = 
        create_private_data(&(handler->requestHandler),
                            handler->listServiceCallback,
                            0, 0, 0, callbackData);

    if (!privateData) {
        return S3StatusOutOfMemory;
    }

    // Get a CurlRequest from the pool
    CurlRequest *request;

    S3Status status = pool_get(privateData, &request);

    if (status != S3StatusOK) {
        free(privateData);
        return status;
    }

    // Set the request-specific curl options

    // Write function
    if (curl_easy_setopt(request->curl, CURLOPT_WRITEFUNCTION,
                         write_function) != CURLE_OK) {
        pool_release(request);
        return S3StatusFailure;
    }

    // Compose the URL
    

    // If there is a request context, just add the curl_easy to the curl_multi
    if (requestContext) {
        return handle_multi_request(request, requestContext);
    }
    // Else, run the curl_easy to completion
    else {
        return handle_easy_request(request);
    }
}
