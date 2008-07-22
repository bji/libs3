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
#include <stdlib.h>
#include <sys/select.h>
#include "request.h"
#include "request_context.h"


S3Status S3_create_request_context(S3RequestContext **requestContextReturn)
{
    *requestContextReturn = 
        (S3RequestContext *) malloc(sizeof(S3RequestContext));
    
    if (!*requestContextReturn) {
        return S3StatusOutOfMemory;
    }
    
    if (!((*requestContextReturn)->curlm = curl_multi_init())) {
        free(*requestContextReturn);
        return S3StatusOutOfMemory;
    }

    (*requestContextReturn)->requests = 0;

    return S3StatusOK;
}


void S3_destroy_request_context(S3RequestContext *requestContext)
{
    curl_multi_cleanup(requestContext->curlm);

    // For each request in the context, call back its done method with
    // 'interrupted' status
    Request *r = requestContext->requests, *rFirst = r;
    
    if (r) do {
        r->status = S3StatusInterrupted;
        Request *rNext = r->next;
        request_finish(r);
        r = rNext;
    } while (r != rFirst);

    free(requestContext);
}


S3Status S3_runall_request_context(S3RequestContext *requestContext)
{
    int requestsRemaining;
    do {
        fd_set readfds, writefds, exceptfds;
        int maxfd;
        S3Status status = S3_get_request_context_fdsets
            (requestContext, &readfds, &writefds, &exceptfds, &maxfd);
        if (status != S3StatusOK) {
            return status;
        }
        select(maxfd + 1, &readfds, &writefds, &exceptfds, 0);
        status = S3_runonce_request_context(requestContext,
                                            &requestsRemaining);
        if (status != S3StatusOK) {
            return status;
        }
    } while (requestsRemaining);
    
    return S3StatusOK;
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
        case CURLM_CALL_MULTI_PERFORM:
            break;
        case CURLM_OUT_OF_MEMORY:
            return S3StatusOutOfMemory;
        default:
            return S3StatusInternalError;
        }

        CURLMsg *msg;
        int junk;
        while ((msg = curl_multi_info_read(requestContext->curlm, &junk))) {
            if ((msg->msg != CURLMSG_DONE) ||
                (curl_multi_remove_handle(requestContext->curlm, 
                                          msg->easy_handle) != CURLM_OK)) {
                return S3StatusInternalError;
            }
            Request *request;
            if (curl_easy_getinfo(msg->easy_handle, CURLOPT_PRIVATE, 
                                  (char *) &request) != CURLE_OK) {
                return S3StatusInternalError;
            }
            // Remove the request from the list of requests
            if (request->prev == request->next) {
                // It was the only one on the list
                requestContext->requests = 0;
            }
            else {
                // It doesn't matter what the order of them are, so just in
                // case request was at the head of the list, put the one after
                // request to the head of the list
                requestContext->requests = request->next;
                request->prev->next = request->next;
                request->next->prev = request->prev;
            }
            if ((msg->data.result != CURLE_OK) &&
                (request->status == S3StatusOK)) {
                request->status = request_curl_code_to_status
                    (msg->data.result);
            }
            // Finish the request, ensuring that all callbacks have been made,
            // and also releases the request
            request_finish(request);
        }
    } while (status == CURLM_CALL_MULTI_PERFORM);

    return S3StatusOK;
}

S3Status S3_get_request_context_fdsets(S3RequestContext *requestContext,
                                       fd_set *readFdSet, fd_set *writeFdSet,
                                       fd_set *exceptFdSet, int *maxFd)
{
    return ((curl_multi_fdset(requestContext->curlm, readFdSet, writeFdSet,
                              exceptFdSet, maxFd) == CURLM_OK) ?
            S3StatusOK : S3StatusInternalError);
}
