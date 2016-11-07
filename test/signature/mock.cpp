#include <request.h>
#include <curl/curl.h>
#include <string>
#include <string.h>
#include <libs3.h>
#include <iostream>

typedef struct RequestComputedValues
{
    // All x-amz- headers, in normalized form (i.e. NAME: VALUE, no other ws)
    char *amzHeaders[S3_MAX_METADATA_COUNT + 2]; // + 2 for acl and date

    // The number of x-amz- headers
    int amzHeadersCount;

    // Storage for amzHeaders (the +256 is for x-amz-acl and x-amz-date)
    char amzHeadersRaw[COMPACTED_METADATA_BUFFER_SIZE + 256 + 1];

    // Canonicalized x-amz- headers
    string_multibuffer(canonicalizedAmzHeaders,
                       COMPACTED_METADATA_BUFFER_SIZE + 256 + 1);

    // URL-Encoded key
    char urlEncodedKey[MAX_URLENCODED_KEY_SIZE + 1];

    // Canonicalized resource
    char canonicalizedResource[MAX_CANONICALIZED_RESOURCE_SIZE + 1];

    // Cache-Control header (or empty)
    char cacheControlHeader[128];

    // Content-Type header (or empty)
    char contentTypeHeader[128];

    // Content-MD5 header (or empty)
    char md5Header[128];

    // Content-Disposition header (or empty)
    char contentDispositionHeader[128];

    // Content-Encoding header (or empty)
    char contentEncodingHeader[128];

    // Expires header (or empty)
    char expiresHeader[128];

    // If-Modified-Since header
    char ifModifiedSinceHeader[128];

    // If-Unmodified-Since header
    char ifUnmodifiedSinceHeader[128];

    // If-Match header
    char ifMatchHeader[128];

    // If-None-Match header
    char ifNoneMatchHeader[128];

    // Range header
    char rangeHeader[128];

    // Authorization header
    char authorizationHeader[128];

    // Host header
    char hostHeader[128];

    // Time stamp is ISO 8601 format: 'yyyymmddThhmmssZ'
    char timestamp[32];

    // The signed headers
    char signedHeaders[COMPACTED_METADATA_BUFFER_SIZE];
} RequestComputedValues;

HttpRequestType http_verb_to_type(const std::string &verb)
{
    if (verb == "POST") return HttpRequestTypePOST;
    if (verb == "GET") return HttpRequestTypeGET;
    if (verb == "HEAD") return HttpRequestTypeHEAD;
    if (verb == "PUT") return HttpRequestTypePUT;
    return HttpRequestTypeDELETE;
}

const char *emptysha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

void createMockObjs(const std::string &httpVerb,
                    const std::string &uri,
                    const std::string &payloadhash,
                    struct curl_slist *headers,
                    Request **request,
                    RequestParams **params,
                    RequestComputedValues **values)
{
    Request *req = new Request;
    strncpy(req->uri, uri.c_str(), sizeof(req->uri));
    req->headers = headers;
    *request = req;
    S3PutProperties *putProperties = new S3PutProperties;
    if (payloadhash.empty())
        putProperties->md5 = emptysha256;
    else
        putProperties->md5 = payloadhash.c_str();
    RequestParams *par = new RequestParams;
    par->httpRequestType = http_verb_to_type(httpVerb);
    par->putProperties = putProperties;
    par->bucketContext.accessKeyId = "AKIDEXAMPLE";
    par->bucketContext.secretAccessKey = "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY";
    *params = par;
    RequestComputedValues *val = new RequestComputedValues;
    const char *timestamp = NULL;
    for (struct curl_slist *itr = headers; itr != NULL; itr = itr->next) {
        if (!strncmp(itr->data, "X-Amz-Date:", 11)) {
            timestamp = itr->data + 11;
            break;
        }
    }
    if (timestamp) {
        memcpy(val->timestamp, timestamp, 17);
    }
    *values = val;
}

void destroyMockObjs(Request *request,
                     RequestParams *params,
                     RequestComputedValues *values)
{
    delete request;
    delete params->putProperties;
    delete params;
    delete values;
}
