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
    Request *request = (Request *) obj;
    (void) request;

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


S3Status S3_list_service(S3Protocol protocol, const char *accessKeyId,
                         const char *secretAccessKey,
                         S3RequestContext *requestContext,
                         S3ListServiceHandler *handler, void *callbackData)
{
    // Compose the x-amz- headers.  Do this first since it may fail and we
    // want to fail before doing any real work.
    XAmzHeaders xAmzHeaders;
    S3Status status = request_compose_x_amz_headers(&xAmzHeaders, 0);
    if (status != S3StatusOK) {
        return status;
    }

    // Get a Request from the pool
    Request *request;

    status = request_get(&(handler->responseHandler), callbackData, &request);

    if (status != S3StatusOK) {
        return status;
    }

    // Set the request-specific curl options

    // Write function
    if (curl_easy_setopt(request->curl, CURLOPT_WRITEFUNCTION,
                         write_function) != CURLE_OK) {
        request_release(request);
        return S3StatusFailure;
    }

    // Compose the URL
    char url[64];
    snprintf(url, sizeof(url), "%s://%s/", 
             (protocol == S3ProtocolHTTP) ? "http" : "https", HOSTNAME);
    curl_easy_setopt(request->curl, CURLOPT_URL, url);

    // Canonicalize the x-amz- headers
    char canonicalizedAmzHeaders[MAX_AMZ_HEADER_SIZE];
    canonicalize_amz_headers(canonicalizedAmzHeaders, &xAmzHeaders);

    // Canonicalize the resource
    char canonicalizedResource[MAX_CANONICALIZED_RESOURCE_SIZE];
    canonicalize_resource(canonicalizedResource, S3UriStylePath, 0, "/", 0);
                                                    
    // Compute the Authorization header
    char authHeader[1024];
    if ((status = auth_header_snprintf
         (authHeader, sizeof(authHeader), accessKeyId, secretAccessKey,
          "GET", 0, 0, canonicalizedAmzHeaders, canonicalizedResource))
        != S3StatusOK) {
        request_release(request);
        return status;
    }
                                       
    // Add the Authorization header
    request->headers = curl_slist_append(request->headers, authHeader);

    // Add the x-amz- headers
    int i;
    for (i = 0; i < xAmzHeaders.count; i++) {
        request->headers = curl_slist_append
            (request->headers, xAmzHeaders.headers[i]);
    }

    // If there is a request context, just add the curl_easy to the curl_multi
    if (requestContext) {
        return request_multi_add(request, requestContext);
    }
    // Else, run the curl_easy to completion
    else {
        request_easy_perform(request);
        return S3StatusOK;
    }
}
