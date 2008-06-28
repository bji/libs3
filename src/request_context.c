/** **************************************************************************
 * request_context.c
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

#include <curl/curl.h>
#include "private.h"


S3Status S3_create_request_context(S3RequestContext **requestContextReturn,
                                   S3Protocol protocol)
{
    return ((*requestContextReturn = (S3RequestContext *) curl_multi_init()) ?
            S3StatusOK : S3StatusFailedToCreateRequestContext);
}


void S3_destroy_request_context(S3RequestContext *requestContext)
{
    curl_multi_cleanup(requestContext->curlm);
}


S3Status S3_runall_request_context(S3RequestContext *requestContext)
{
    int requestsRemaining;
    S3Status status;

    do {
        S3_runonce_request_context(requestContext, &requestsRemaining);
    } while ((status == S3StatusOK) && requestsRemaining);

    return status;
}


S3Status S3_runonce_request_context(S3RequestContext *requestContext, 
                                    int *requestsRemainingReturn)
{
    CURLMcode status;

    do {
        status = curl_multi_perform(requestContext->curlm,
                                    requestsRemainingReturn);
        
        CURLMsg *msg;
        int junk;
        while ((msg = curl_multi_info_read(requestContext->curlm, &junk))) {
            if ((msg->msg != CURLMSG_DONE) ||
                (curl_multi_remove_handle(requestContext->curlm, 
                                          msg->easy_handle) != CURLM_OK)) {
                return S3StatusFailure;
            }
            Request *request;
            if (curl_easy_getinfo(msg->easy_handle, CURLOPT_PRIVATE, 
                                  (char **) &request) != CURLE_OK) {
                return S3StatusFailure;
            }
            // Make response complete callback
            S3Status status;
            switch (msg->data.result) {
            case CURLE_OK:
                status = S3StatusOK;
                break;
                // xxx todo fill the rest in
            default:
                status = S3StatusFailure;
                break;
            }
            // Finish the request, ensuring that all callbacks have been made,
            // and also releases the request
            request_finish(request, status);
        }
    } while (status == CURLM_CALL_MULTI_PERFORM);

    switch (status) {
    case CURLM_OK:
        return S3StatusOK;
    case CURLM_OUT_OF_MEMORY:
        return S3StatusOutOfMemory;
    default:
        return S3StatusFailure;
    }
}
