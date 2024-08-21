//secretes.h

const char* mqtt_server = "***-ats.iot.us-west-2.amazonaws.com";

// Amazon Root CA 1
static const char AWS_CERT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

// Device Certificate
static const char AWS_CERT_CRT[] PROGMEM = R"KEY(
-----BEGIN CERTIFICATE-----
MIIDWjCCAkKgAwIBAgIVAKvw11NXnp0708SI2HKzH9BEicM/MA0GCSqGSIb3DQEB
3L3oMFT4izxL8tiaw5xaWpfBQo6c8mnh3oHdWFMDNhcIdzoQ4ja2CJIScBL06EEv
hzFRf0yqHKk6WYJP1j9QVXU6J/BtkVgLxJXAG5dtbMwhr2AA+Y6+en0ihGeN+Q==
-----END CERTIFICATE-----
)KEY";

// Device Private Key
static const char AWS_CERT_PRIVATE[] PROGMEM = R"KEY(
-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEAzGtJIy18Nnu70v4WQ9u7mPZr3f3R9Stc8FTk1HKYBottxMqs
Bx4srQKBgQCjb2WqglrEK/y5grA/AS9AfL6An0M4GZIc4c+K9rIL839rHdBUjUZD
zPpXX6MBkB8/ncc/w+WkSOh+Fs7PrIRZSxVVGTxQD8AblofO98slf7FGH8XT6/MO
o5X9ixjJc0U4DpeyrK0yTy6PgfXPtcoJef2b4EFDV9HFWoNuN2UniQ==
-----END RSA PRIVATE KEY-----
)KEY";

//Enter the name of your AWS IoT thing, MyNewESP
