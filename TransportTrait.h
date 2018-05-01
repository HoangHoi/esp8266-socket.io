#ifndef TransportTrait_H_
#define TransportTrait_H_

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

class TransportTrait
{
public:
    virtual ~TransportTrait()
    {
    }

    virtual std::unique_ptr<WiFiClient> create()
    {
        return std::unique_ptr<WiFiClient>(new WiFiClient());
    }

    virtual bool verify(WiFiClient& client, const char* host)
    {
        (void)client;
        (void)host;
        return true;
    }
};

class TLSTrait : public TransportTrait
{
public:
    TLSTrait(const String& fingerprint) :
        _fingerprint(fingerprint)
    {
    }

    std::unique_ptr<WiFiClient> create() override
    {
        return std::unique_ptr<WiFiClient>(new WiFiClientSecure());
    }

    bool verify(WiFiClient& client, const char* host) override
    {
        auto wcs = static_cast<WiFiClientSecure&>(client);
        return wcs.verify(_fingerprint.c_str(), host);
    }

protected:
    String _fingerprint;
};

// class TransportTrait;
typedef std::unique_ptr<TransportTrait> TransportTraitPtr;

#endif // TransportTrait_H_