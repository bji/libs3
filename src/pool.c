/** **************************************************************************
 * pool.c
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
#include "libs3.h"
#include "private.h"

#define USER_AGENT_SIZE 256
#define POOL_SIZE 8

static char userAgentG[USER_AGENT_SIZE];

static struct S3Mutex *poolMutexG;

static CurlRequest *poolG[POOL_SIZE];

static int poolCountG;


static S3Status initialize_curl_handle(PrivateData *privateData, CURL *handle)
{
    CURLcode status;
    
#define curl_easy_setopt_safe(opt, val)                              \
    if ((status = curl_easy_setopt(handle, opt, val)) != CURLE_OK) { \
        return S3StatusFailedToInitializeRequest;                    \
    }

    // Debugging only
    // curl_easy_setopt(handle, CURLOPT_VERBOSE, 1);

    // Always set the PrivateData in the private data
    curl_easy_setopt_safe(CURLOPT_PRIVATE, privateData);

    // Always set the headers callback and its data
    curl_easy_setopt_safe(CURLOPT_HEADERFUNCTION, &curl_header_func);
    curl_easy_setopt_safe(CURLOPT_HEADERDATA, privateData);

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

    // A safety valve for servers that go bananas with redirects
    curl_easy_setopt_safe(CURLOPT_MAXREDIRS, 10);

    // Set the User-Agent; maybe Amazon will track these?
    // Mozilla/4.0 (compatible; fuses3 X.Y; PLATFORM (possibly Private))
    curl_easy_setopt_safe(CURLOPT_USERAGENT, userAgentG);

    // Ask S3 to tell us file times, which we can use for fallbacks if we
    // don't get x-amz-meta-mtime
    curl_easy_setopt_safe(CURLOPT_FILETIME, 1);

    // Set the low speed limit and time; we abort transfers that stay at
    // less than 1K per second for more than 60 seconds.
    // xxx todo - make these configurable
    // xxx todo - allow configurable max send and receive speed
    curl_easy_setopt_safe(CURLOPT_LOW_SPEED_LIMIT, 1024);
    curl_easy_setopt_safe(CURLOPT_LOW_SPEED_TIME, 60);

    // Tell Curl to keep up to POOL_SIZE connections open at once
    curl_easy_setopt_safe(CURLOPT_MAXCONNECTS, POOL_SIZE);

    return S3StatusOK;
}


static void curl_request_destroy(CurlRequest *curlRequest)
{
    if (curlRequest->headers) {
        curl_slist_free_all(curlRequest->headers);
        curlRequest->headers = 0;
    }
    
    curl_easy_cleanup(curlRequest->curl);

    free(curlRequest);
}


static S3Status curl_request_create(PrivateData *privateData,
                                    CurlRequest **curlRequestReturn)
{
    CurlRequest *curlRequest = *curlRequestReturn = 
        (CurlRequest *) malloc(sizeof(CurlRequest));
    if (!curlRequest) {
        return S3StatusFailedToCreateRequest;
    }

    if (!(curlRequest->curl = curl_easy_init())) {
        free(*curlRequestReturn);
        return S3StatusFailedToInitializeRequest;
    }

    curlRequest->headers = 0;

    S3Status status = initialize_curl_handle(privateData, curlRequest->curl);
    if (status != S3StatusOK) {
        curl_request_destroy(curlRequest);
        return status;
    }

    return S3StatusOK;
}


static S3Status curl_request_reinitialize(PrivateData *privateData,
                                          CurlRequest *curlRequest)
{
    if (curlRequest->headers) {
        curl_slist_free_all(curlRequest->headers);
        curlRequest->headers = 0;
    }

    return initialize_curl_handle(privateData, curlRequest->curl);
}


S3Status pool_initialize(const char *userAgentInfo)
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


void pool_deinitialize()
{
    mutex_destroy(poolMutexG);

    while (poolCountG--) {
        curl_request_destroy(poolG[poolCountG]);
    }
}


S3Status pool_get(PrivateData *privateData, CurlRequest **curlRequestReturn)
{
    *curlRequestReturn = 0;
    
    // Try to get one from the pool.  We hold the lock for the shortest time
    // possible here.
    mutex_lock(poolMutexG);

    if (poolCountG) {
        *curlRequestReturn = poolG[poolCountG--];
    }
    
    mutex_unlock(poolMutexG);

    // If we got something from the pool, then reinitialize it and return it
    S3Status status;
    if (*curlRequestReturn) {
        status = curl_request_reinitialize(privateData, *curlRequestReturn);
        if (status != S3StatusOK) {
            curl_request_destroy(*curlRequestReturn);
            return status;
        }
        return S3StatusOK;
    }
    else {
        // If there were none available in the pool, create one and return it
        return curl_request_create(privateData, curlRequestReturn);
    }
}


void pool_release(CurlRequest *curlRequest)
{
    // Release the private data
    PrivateData *pd = 0;
    if ((curl_easy_getinfo(curlRequest->curl, CURLOPT_PRIVATE, 
                           (char **) &pd) == CURLE_OK) && pd) {
        free(pd);
    }
    // Else very weird, ignore

    mutex_lock(poolMutexG);

    // If the pool is full, destroy this one
    if (poolCountG == POOL_SIZE) {
        mutex_unlock(poolMutexG);
        curl_request_destroy(curlRequest);
    }
    // Else put this one at the front of the pool; we do this because we want
    // the most-recently-used curl handle to be re-used on the next request,
    // to maximize our chances of re-using a TCP connection before its HTTP
    // keep-alive times out
    else {
        poolG[poolCountG++] = curlRequest;
        // Reset the CURL handle for reuse
        curl_easy_reset(curlRequest->curl);
        // And delete whatever headers were added
        curl_slist_free_all(curlRequest->headers);
        curlRequest->headers = 0;
        mutex_unlock(poolMutexG);
    }
}
