/** **************************************************************************
 * request.c
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
#include <sys/utsname.h>
#include "private.h"

#define USER_AGENT_SIZE 256
#define POOL_SIZE 8

//
static char userAgentG[USER_AGENT_SIZE];

static struct S3Mutex *poolMutexG;

static Request *poolG[POOL_SIZE];

static int poolCountG;


static size_t curl_header_func(void *ptr, size_t size, size_t nmemb, void *data)
{
    size_t len = size * nmemb;
    char *header = (char *) ptr;
    Request *request = (Request *) data;
    S3ResponseHeaders *responseHeaders = &(request->responseHeaders);

    // Curl might call back the header function after the body has been
    // received, for 'chunked encoded' contents.  We don't handle this as of
    // yet, and it's not clear that it would ever be useful.
    if (request->headersCallbackMade) {
        return len;
    }

    // The header must end in \r\n, so we can set the \r to 0 to terminate it
    header[len - 2] = 0;
    
    // Find the colon to split the header up
    char *colon = header;
    while (*colon && (*colon != ':')) {
        colon++;
    }
    
    int namelen = colon - header;

    if (!strncmp(header, "RequestId", namelen)) {
        (void) responseHeaders;
    }
    else if (!strncmp(header, "RequestId2", namelen)) {
    }
    else if (!strncmp(header, "ContentType", namelen)) {
    }
    else if (!strncmp(header, "ContentLength", namelen)) {
    }
    else if (!strncmp(header, "Server", namelen)) {
    }
    else if (!strncmp(header, "ETag", namelen)) {
    }
    else if (!strncmp(header, "LastModified", namelen)) {
    }
    else if (!strncmp(header, "x-amz-meta-", 
                      (namelen > strlen("x-amz-meta-") ? 
                       strlen("x-amz-meta-") : namelen))) {
        
    }
    // Else if it is an empty header, then it's the last header
    else if (!header[0]) {
        
    }

    return len;
}


static S3Status initialize_curl_handle(CURL *handle, Request *request)
{
    CURLcode status;
    
#define curl_easy_setopt_safe(opt, val)                                 \
    if ((status = curl_easy_setopt(handle, opt, val)) != CURLE_OK) {    \
        return S3StatusFailedToInitializeRequest;                       \
    }

    // Debugging only
    // curl_easy_setopt(handle, CURLOPT_VERBOSE, 1);

    // Always set the PrivateData in the private data
    curl_easy_setopt_safe(CURLOPT_PRIVATE, request);

    // Always set the headers callback and its data
    curl_easy_setopt_safe(CURLOPT_HEADERFUNCTION, &curl_header_func);

    // Curl docs suggest that this is necessary for multithreaded code.
    // However, it also points out that DNS timeouts will not be honored
    // during DNS lookup, which can be worked around by using the c-ares
    // library, which we do not do yet.
    curl_easy_setopt_safe(CURLOPT_NOSIGNAL, 1);

    // Turn off Curl's built-in progress meter
    curl_easy_setopt_safe(CURLOPT_NOPROGRESS, 0);

    // xxx todo - support setting the proxy for Curl to use (can't use https
    // for proxies though)

    // xxx todo - support setting the network interface for Curl to use

    // I think this is useful - we don't need interactive performance, we need
    // to complete large operations quickly
    curl_easy_setopt_safe(CURLOPT_TCP_NODELAY, 1);
    
    // Don't use Curl's 'netrc' feature
    curl_easy_setopt_safe(CURLOPT_NETRC, CURL_NETRC_IGNORED);

    // Follow any redirection directives that S3 sends
    curl_easy_setopt_safe(CURLOPT_FOLLOWLOCATION, 1);

    // A safety valve in case S3 goes bananas with redirects
    curl_easy_setopt_safe(CURLOPT_MAXREDIRS, 10);

    // Set the User-Agent; maybe Amazon will track these?
    curl_easy_setopt_safe(CURLOPT_USERAGENT, userAgentG);

    // Set the low speed limit and time; we abort transfers that stay at
    // less than 1K per second for more than 15 seconds.
    // xxx todo - make these configurable
    // xxx todo - allow configurable max send and receive speed
    curl_easy_setopt_safe(CURLOPT_LOW_SPEED_LIMIT, 1024);
    curl_easy_setopt_safe(CURLOPT_LOW_SPEED_TIME, 15);

    // Tell Curl to keep up to POOL_SIZE connections open at once
    curl_easy_setopt_safe(CURLOPT_MAXCONNECTS, POOL_SIZE);

    return S3StatusOK;
}


static void request_destroy(Request *request)
{
    if (request->headers) {
        curl_slist_free_all(request->headers);
    }
    
    curl_easy_cleanup(request->curl);

    free(request);
}


static S3Status request_initialize(Request *request,
                                        S3ResponseHandler *handler,
                                        void *callbackData)
{
    // This must be done before any error is returned
    request->headers = 0;

    S3Status status = initialize_curl_handle(request->curl, request);
    if (status != S3StatusOK) {
        return status;
    }

    request->callbackData = callbackData;

    request->metaHeaderStringsLen = 0;

    request->headersCallback = handler->headersCallback;

    request->responseHeaders.requestId = 0;
    
    request->responseHeaders.requestId2 = 0;

    request->responseHeaders.contentType = 0;

    request->responseHeaders.contentLength = 0;

    request->responseHeaders.server = 0;

    request->responseHeaders.eTag = 0;

    request->responseHeaders.lastModified = 0;

    request->responseHeaders.metaHeadersCount = 0;

    request->responseHeaders.metaHeaders = request->metaHeaders;
    
    request->headersCallbackMade = 0;

    request->completeCallback = handler->completeCallback;

    request->receivedS3Error = 0;

    return S3StatusOK;
}


static S3Status request_create(S3ResponseHandler *handler,
                                    void *callbackData,
                                    Request **requestReturn)
{
    Request *request = (Request *) malloc(sizeof(Request));

    if (!request) {
        return S3StatusFailedToCreateRequest;
    }

    if (!(request->curl = curl_easy_init())) {
        free(request);
        return S3StatusFailedToInitializeRequest;
    }

    S3Status status = 
        request_initialize(request, handler, callbackData);

    if (status != S3StatusOK) {
        request_destroy(request);
        return status;
    }

    *requestReturn = request;

    return S3StatusOK;
}


static S3Status request_reinitialize(S3ResponseHandler *handler,
                                          void *callbackData,
                                          Request *request)
{
    // Reset the CURL handle for reuse
    curl_easy_reset(request->curl);

    // Free the headers
    if (request->headers) {
        curl_slist_free_all(request->headers);
    }

    return request_initialize(request, handler, callbackData);
}


S3Status request_api_initialize(const char *userAgentInfo)
{
    if (!(poolMutexG = mutex_create())) {
        return S3StatusFailedToCreateMutex;
    }

    poolCountG = 0;

    if (!userAgentInfo || !*userAgentInfo) {
        userAgentInfo = "Unknown";
    }

    char platform[96];
    struct utsname utsn;
    if (uname(&utsn)) {
        strncpy(platform, "Unknown", sizeof(platform));
        // Because strncpy doesn't always zero terminate
        platform[sizeof(platform) - 1] = 0;
    }
    else {
        snprintf(platform, sizeof(platform), "%s %s", utsn.sysname, 
                 utsn.machine);
    }

    snprintf(userAgentG, sizeof(userAgentG), 
             "Mozilla/4.0 (Compatible; %s; libs3 %d.%d; %s)",
             userAgentInfo, LIBS3_VER_MAJOR, LIBS3_VER_MINOR, platform);
    
    return S3StatusOK;
}


void request_api_deinitialize()
{
    mutex_destroy(poolMutexG);

    while (poolCountG--) {
        request_destroy(poolG[poolCountG]);
    }
}


S3Status request_get(S3ResponseHandler *handler, void *callbackData,
                          Request **requestReturn)
{
    Request *request = 0;
    
    // Try to get one from the pool.  We hold the lock for the shortest time
    // possible here.
    mutex_lock(poolMutexG);

    if (poolCountG) {
        request = poolG[poolCountG--];
    }
    
    mutex_unlock(poolMutexG);

    // If we got something from the pool, then reinitialize it and return it
    if (request) {
        S3Status status = request_reinitialize
            (handler, callbackData, request);

        if (status != S3StatusOK) {
            request_destroy(request);
            return status;
        }

        *requestReturn = request;

        return S3StatusOK;
    }
    else {
        // If there were none available in the pool, create one and return it
        return request_create(handler, callbackData, requestReturn);
    }
}


void request_release(Request *request)
{
    mutex_lock(poolMutexG);

    // If the pool is full, destroy this one
    if (poolCountG == POOL_SIZE) {
        mutex_unlock(poolMutexG);
        request_destroy(request);
    }
    // Else put this one at the front of the pool; we do this because we want
    // the most-recently-used curl handle to be re-used on the next request,
    // to maximize our chances of re-using a TCP connection before its HTTP
    // keep-alive times out
    else {
        poolG[poolCountG++] = request;
        mutex_unlock(poolMutexG);
    }
}


S3Status request_multi_add(Request *request, 
                                S3RequestContext *requestContext)
{
    switch (curl_multi_add_handle(requestContext->curlm, request->curl)) {
    case CURLM_OK:
        return S3StatusOK;
        // xxx todo - more specific errors
    default:
        request_release(request);
        return S3StatusFailure;
    }
}


void request_easy_perform(Request *request)
{
    CURLcode code = curl_easy_perform(request->curl);

    S3Status status;
    switch (code) {
    case CURLE_OK:
        status = S3StatusOK;
        // xxx todo - more specific errors
    default:
        status = S3StatusFailure;
    }

    // Finish the request, ensuring that all callbacks have been made, and
    // also releases the request
    request_finish(request, status);
}


void request_finish(Request *request, S3Status status)
{
    if (!request->headersCallbackMade) {
        (*(request->headersCallback))(&(request->responseHeaders),
                                      request->callbackData);
    }

    if (!request->completeCallbackMade) {
        // Figure out the HTTP response code
        int httpResponseCode = 0;

        (void) curl_easy_getinfo
            (request->curl, CURLINFO_RESPONSE_CODE, &httpResponseCode);

        (*(request->completeCallback))
            (status, httpResponseCode, 
             request->receivedS3Error ? &(request->s3Error) : 0,
             request->callbackData);
    }

    request_release(request);
}
