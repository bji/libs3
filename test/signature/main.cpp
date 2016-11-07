#include <iostream>
#include <fstream>
#include <string>
#include <curl/curl.h>
#include <request.h>
#include <libs3.h>
#include <sstream>
#include <iomanip>
#include <stdlib.h>

using namespace std;

string url_encode(const string &value) {
    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        string::value_type c = (*i);

            // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~'
            || c == '/' || c == '%' || c == '?' || c == '&' || c == '=') {
            escaped << c;
            continue;
        }

            // Any other characters are percent-encoded
        escaped << uppercase;
        escaped << '%' << setw(2) << int((unsigned char) c);
        escaped << nouppercase;
    }

    return escaped.str();
}

#include <openssl/sha.h>

string sha256(const string str)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

struct RequestComputedValues;

void createMockObjs(const std::string &httpVerb,
                    const std::string &uri,
                    const std::string &payloadhash,
                    struct curl_slist *headers,
                    Request **request,
                    RequestParams **params,
                    RequestComputedValues **values);
void destroyMockObjs(Request *, RequestParams *, RequestComputedValues *);


struct RequestComputedValues;
extern "C" {
    S3Status compose_auth4_header(Request *request,
                                  const RequestParams *params,
                                  const RequestComputedValues *values);

    S3Status calculate_hmac_sha256(char *buf, int maxlen,int *plen,
                                   const char *key, int keylen,
                                   const char *msg, int msglen);
}

#if 0

void print_sha256(const char *hash, int len)
{
    for (int i = 0; i < len; i++) {
        printf("%02x", (unsigned char)hash[i]);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    char kdate[32],kregion[32],kservice[32],ksigning[32];
    int len = 0;
    string secret = "AWS4wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY";
    string date = "20150830";
    calculate_hmac_sha256(kdate, 32, &len, secret.c_str(), secret.length(),
                          date.c_str(), 8);
    print_sha256(kdate, 32);
    len = 0;
    calculate_hmac_sha256(kregion, 32, &len, kdate, 32, "us-east-1", 9);
    print_sha256(kregion, 32);
}

#endif
#if 1
int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cout << "Usage: ./signature xxx.req\n";
        return -1;
    }
    S3_initialize(NULL, S3_INIT_ALL | S3_INIT_SIGNATURE_V4, NULL);
    S3_set_region_name("us-east-1");
    std::ifstream reqfile(argv[1]);
    std::string httpVerb, uri, httpVersion;
    reqfile >> httpVerb >> uri >> httpVersion;
    while (httpVersion != "HTTP/1.1") {
        reqfile >> httpVersion;
    }
    uri = std::string("https://s3.amazonaws.com") + url_encode(uri);
    std::string line;
    std::string header;
    struct curl_slist *headerList = NULL;
    std::getline(reqfile, line);
    while (std::getline(reqfile, line)) {
        if (line.empty()) break;
        if (line[0] == ' ' || line[0] == '\t') {
            header += "\r\n";
            header += line;
        } else {
            if (!header.empty())
                headerList = curl_slist_append(headerList, header.c_str());
            header = line;
        }
    }
    if (!header.empty())
        headerList = curl_slist_append(headerList, header.c_str());
    std::string shahash;
    if (reqfile.bad()) {
        std::cout << "Failed to read request file: " << argv[1] << std::endl;
        curl_slist_free_all(headerList);
        return -1;
    } else if (!reqfile.eof()) {
        std::string content((std::istreambuf_iterator<char>(reqfile)),
                            std::istreambuf_iterator<char>());
        shahash = sha256(content);
    }
#ifdef DEBUG
    std::cout << uri << std::endl;
    for (struct curl_slist *itr = headerList; itr != NULL; itr = itr->next) {
        std::cout << "HEADER::" << itr->data << "::\n";
    }
#endif
    Request *request;
    RequestParams *params;
    RequestComputedValues *values;
    createMockObjs(httpVerb, uri, shahash, headerList, &request, &params,
                   &values);
    S3Status ret = compose_auth4_header(request, params, values);
    if (ret != S3StatusOK) {
        std::cout << "Failed to generate authorization header!" << std::endl;
        std::cout << "Error is " << S3_get_status_name(ret) << std::endl;
    }
    struct curl_slist *itr = request->headers;
    while (itr->next) itr = itr->next;
    std::cout << (char *)(&itr->data[15]);
    destroyMockObjs(request, params, values);
    curl_slist_free_all(headerList);
    S3_deinitialize();
    return 0;
}
#endif
