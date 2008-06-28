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


static size_t write_function(void *ptr, size_t size, size_t nmemb, void *obj)
{
    Request *curlRequest = (Request *) obj;
    (void) curlRequest;

    int len = size * nmemb;

    if (len == 0) {
        return 0;
    }

    char *str = (char *) ptr;

    char c = str[len - 1];
    str[len - 1] = 0;
    
    printf("data: %s", str);
    if (c) {
        printf("%c\n", c);
    }

    return len;
}


S3Status S3_list_service(const char *accessKeyId, const char *secretAccessKey,
                         S3RequestContext *requestContext,
                         S3ListServiceHandler *handler, void *callbackData)
{
    // Get a Request from the pool
    Request *curlRequest;

    S3Status status = request_get
        (&(handler->responseHandler), callbackData, &curlRequest);

    if (status != S3StatusOK) {
        return status;
    }

    // Set the request-specific curl options

    // Write function
    if (curl_easy_setopt(curlRequest->curl, CURLOPT_WRITEFUNCTION,
                         write_function) != CURLE_OK) {
        request_release(curlRequest);
        return S3StatusFailure;
    }

    // Compose the URL
    

    // If there is a request context, just add the curl_easy to the curl_multi
    if (requestContext) {
        return request_multi_add(curlRequest, requestContext);
    }
    // Else, run the curl_easy to completion
    else {
        request_easy_perform(curlRequest);
        return S3StatusOK;
    }
}
