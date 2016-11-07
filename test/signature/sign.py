import hmac, hashlib

def sign(key, msg):
    return hmac.new(key, msg.encode("utf-8"), hashlib.sha256).digest()

def getSignatureKey(key, dateStamp, regionName, serviceName):
    kDate = sign(("AWS4" + key).encode("utf-8"), dateStamp)
    print kDate.encode("hex")
    kRegion = sign(kDate, regionName)
    print kRegion.encode("hex")
    kService = sign(kRegion, serviceName)
    print kService.encode("hex")
    kSigning = sign(kService, "aws4_request")
    return kSigning

print getSignatureKey("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY",
                      "20150830",
                      "us-east-1",
                      "service").encode("hex")
                      
