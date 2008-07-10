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
#include "request.h"
#include "request_context.h"


S3Status S3_create_request_context(S3RequestContext **requestContextReturn)
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
    // This should use the socket stuff to watch the fds and run only
    // when there is data ready
    return S3StatusFailure;
}


S3Status S3_runonce_request_context(S3RequestContext *requestContext, 
                                    int *requestsRemainingReturn)
{
    CURLMcode status;

    do {
        status = curl_multi_perform(requestContext->curlm,
                                    requestsRemainingReturn);

        switch (status) {
        case CURLM_OK:
            break;
        case CURLM_OUT_OF_MEMORY:
            return S3StatusOutOfMemory;
        default:
            return S3StatusFailure;
        }

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
            switch (msg->data.result) {
            case CURLE_OK:
                request->status = S3StatusOK;
                break;
                // xxx todo fill the rest in
            default:
                request->status = S3StatusFailure;
                break;
            }
            // Finish the request, ensuring that all callbacks have been made,
            // and also releases the request
            request_finish(request);
        }
    } while (status == CURLM_CALL_MULTI_PERFORM);

    return S3StatusOK;
}
