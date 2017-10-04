/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/opensslidentity.h"
#include "rtc_base/gunit.h"
#include "rtc_base/openssl.h"
#include "rtc_base/ptr_util.h"

using rtc::OpenSSLCertificate;
using rtc::SSLCertChain;

namespace {
const char kCert1[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIB8TCCAZugAwIBAgIJAL9GDdi6iSRZMA0GCSqGSIb3DQEBCwUAMFQxCzAJBgNV\n"
    "BAYTAlVTMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX\n"
    "aWRnaXRzIFB0eSBMdGQxDTALBgNVBAMMBFRFU1QwHhcNMTcwOTI3MTgwMzQ5WhcN\n"
    "MjcwOTI1MTgwMzQ5WjBUMQswCQYDVQQGEwJVUzETMBEGA1UECAwKU29tZS1TdGF0\n"
    "ZTEhMB8GA1UECgwYSW50ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMQ0wCwYDVQQDDARU\n"
    "RVNUMFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAMgT+ilZ4v5mKjZ+JWmNjPJZ4C6o\n"
    "T3y9+/0SRRW6+hlDrVcxOcmOsZlTDLotBBBrN2P0faUA/A4suPvHVQJVG40CAwEA\n"
    "AaNQME4wHQYDVR0OBBYEFM1kyOTdSRaP/1WI+IlNtsBE/B4+MB8GA1UdIwQYMBaA\n"
    "FM1kyOTdSRaP/1WI+IlNtsBE/B4+MAwGA1UdEwQFMAMBAf8wDQYJKoZIhvcNAQEL\n"
    "BQADQQABvqSHEQCo6vgZCJj6sCoDGe0i0eKeIcvKFxED8V0XideZYJe1631sjTf6\n"
    "rEMVuoAszWVBiIRlhfL2Ng7d2lFs\n"
    "-----END CERTIFICATE-----\n";
const char kCert2[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIB8zCCAZ2gAwIBAgIJAM/U3cfUNJArMA0GCSqGSIb3DQEBCwUAMFUxCzAJBgNV\n"
    "BAYTAlVTMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX\n"
    "aWRnaXRzIFB0eSBMdGQxDjAMBgNVBAMMBVRFU1QyMB4XDTE3MDkyNzE4MDQxOFoX\n"
    "DTI3MDkyNTE4MDQxOFowVTELMAkGA1UEBhMCVVMxEzARBgNVBAgMClNvbWUtU3Rh\n"
    "dGUxITAfBgNVBAoMGEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDEOMAwGA1UEAwwF\n"
    "VEVTVDIwXDANBgkqhkiG9w0BAQEFAANLADBIAkEAxBhQ0F+T8ykg8qve7un4wso3\n"
    "8xWs1sCCIVaXmEbBL1boY33wFwcu+/e8ux+4QhMzoivd+1MH2vlKEyZ+06uNMwID\n"
    "AQABo1AwTjAdBgNVHQ4EFgQUsIfPUvDOqAbTVVRhaFvOiDz0NAgwHwYDVR0jBBgw\n"
    "FoAUsIfPUvDOqAbTVVRhaFvOiDz0NAgwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0B\n"
    "AQsFAANBAKjdk11ufKiL4glzBKDpO3VGUGTbvSgftgD53DYbFzFKpxlXosO9BClR\n"
    "bblOKyeuExziGR0hAQZVgiZFL+66gYw=\n"
    "-----END CERTIFICATE-----\n";

const char kCert3[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIEUjCCAjqgAwIBAgIBAjANBgkqhkiG9w0BAQsFADCBljELMAkGA1UEBhMCVVMx\n"
    "EzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxFDAS\n"
    "BgNVBAoMC0dvb2dsZSwgSW5jMQwwCgYDVQQLDANHVFAxFzAVBgNVBAMMDnRlbGVw\n"
    "aG9ueS5nb29nMR0wGwYJKoZIhvcNAQkBFg5ndHBAZ29vZ2xlLmNvbTAeFw0xNzA5\n"
    "MjYwNDA5MDNaFw0yMDA2MjIwNDA5MDNaMGQxCzAJBgNVBAYTAlVTMQswCQYDVQQI\n"
    "DAJDQTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzEXMBUGA1UECgwOdGVsZXBob255\n"
    "Lmdvb2cxFzAVBgNVBAMMDnRlbGVwaG9ueS5nb29nMIGfMA0GCSqGSIb3DQEBAQUA\n"
    "A4GNADCBiQKBgQDJXWeeU1v1+wlqkVobzI3aN7Uh2iVQA9YCdq5suuabtiD/qoOD\n"
    "NKpmQqsx7WZGGWSZTDFEBaUpvIK7Hb+nzRqk6iioPCFOFuarm6GxO1xVneImMuE6\n"
    "tuWb3YZPr+ikChJbl11y5UcSbg0QsbeUc+jHl5umNvrL85Y+z8SP0rxbBwIDAQAB\n"
    "o2AwXjAdBgNVHQ4EFgQU7tdZobqlN8R8V72FQnRxmqq8tKswHwYDVR0jBBgwFoAU\n"
    "5GgKMUtcxkQ2dJrtNR5YOlIAPDswDwYDVR0TAQH/BAUwAwEB/zALBgNVHQ8EBAMC\n"
    "AQYwDQYJKoZIhvcNAQELBQADggIBADObh9Z+z14FmP9zSenhFtq7hFnmNrSkklk8\n"
    "eyYWXKfOuIriEQQBZsz76ZcnzStih8Rj+yQ0AXydk4fJ5LOwC2cUqQBar17g6Pd2\n"
    "8g4SIL4azR9WvtiSvpuGlwp25b+yunaacDne6ebnf/MUiiKT5w61Xo3cEPVfl38e\n"
    "/Up2l0bioid5enUTmg6LY6RxDO6tnZQkz3XD+nNSwT4ehtkqFpHYWjErj0BbkDM2\n"
    "hiVc/JsYOZn3DmuOlHVHU6sKwqh3JEyvHO/d7DGzMGWHpHwv2mCTJq6l/sR95Tc2\n"
    "GaQZgGDVNs9pdEouJCDm9e/PbQWRYhnat82PTkXx/6mDAAwdZlIi/pACzq8K4p7e\n"
    "6hF0t8uKGnXJubHPXxlnJU6yxZ0yWmivAGjwWK4ur832gKlho4jeMDhiI/T3QPpl\n"
    "iMNsIvxRhdD+GxJkQP1ezayw8s+Uc9KwKglrkBSRRDLCJUfPOvMmXLUDSTMX7kp4\n"
    "/Ak1CA8dVLJIlfEjLBUuvAttlP7+7lsKNgxAjCxZkWLXIyGULzNPQwVWkGfCbrQs\n"
    "XyMvSbFsSIb7blV7eLlmf9a+2RprUUkc2ALXLLCI9YQXmxm2beBfMyNmmebwBJzT\n"
    "B0OR+5pFFNTJPoNlqpdrDsGrDu7JlUtk0ZLZzYyKXbgy2qXxfd4OWzXXjxpLMszZ\n"
    "LDIpOAkj\n"
    "-----END CERTIFICATE-----\n";
}  // namespace

class OpenSSLCertificateTest : public testing::Test {
 public:
  OpenSSLCertificateTest() {}
  ~OpenSSLCertificateTest() {}

  void SetUp() override {
    leaf_cert = rtc::WrapUnique<OpenSSLCertificate>(
        OpenSSLCertificate::FromPEMString(kCert3));
    int_cert1 = rtc::WrapUnique<OpenSSLCertificate>(
        OpenSSLCertificate::FromPEMString(kCert1));
    int_cert2 = rtc::WrapUnique<OpenSSLCertificate>(
        OpenSSLCertificate::FromPEMString(kCert2));
    x509s = sk_X509_new_null();
    sk_X509_push(x509s, leaf_cert->x509());
    sk_X509_push(x509s, int_cert1->x509());
    sk_X509_push(x509s, int_cert2->x509());
  }
  std::unique_ptr<OpenSSLCertificate> leaf_cert;
  std::unique_ptr<OpenSSLCertificate> int_cert1;
  std::unique_ptr<OpenSSLCertificate> int_cert2;

  STACK_OF(X509) * x509s;
};

TEST_F(OpenSSLCertificateTest, OneCert) {
  auto leaf_cert = rtc::WrapUnique<OpenSSLCertificate>(
      OpenSSLCertificate::FromPEMString(kCert3));
  std::unique_ptr<SSLCertChain> chain = leaf_cert->GetChain();
  EXPECT_EQ(chain, nullptr);
}

TEST_F(OpenSSLCertificateTest, ThreeCert) {
  auto certificate =
      rtc::WrapUnique<OpenSSLCertificate>(new OpenSSLCertificate(x509s));
  std::unique_ptr<SSLCertChain> chain = certificate->GetChain();
  ASSERT_EQ(chain->GetSize(), (size_t)2);
  ASSERT_EQ(chain->Get(0).ToPEMString(), kCert1);
  ASSERT_EQ(chain->Get(1).ToPEMString(), kCert2);
}

TEST_F(OpenSSLCertificateTest, CompareCert) {
  auto cert1 =
      rtc::WrapUnique<OpenSSLCertificate>(new OpenSSLCertificate(x509s));
  auto cert2 =
      rtc::WrapUnique<OpenSSLCertificate>(new OpenSSLCertificate(x509s));
  auto cert3 = rtc::WrapUnique<OpenSSLCertificate>(
      new OpenSSLCertificate(sk_X509_value(x509s, 0)));
  ASSERT_TRUE(*cert1 == *cert2);
  ASSERT_TRUE(*cert1 != *cert3);
}

TEST_F(OpenSSLCertificateTest, ToPEMString) {
  auto cert1 =
      rtc::WrapUnique<OpenSSLCertificate>(new OpenSSLCertificate(x509s));
  auto cert2 = rtc::WrapUnique<OpenSSLCertificate>(
      new OpenSSLCertificate(sk_X509_value(x509s, 0)));
  ASSERT_EQ(cert1->ToPEMString(), std::string(kCert3) + kCert1 + kCert2);
  ASSERT_EQ(cert2->ToPEMString(), kCert3);
}

TEST_F(OpenSSLCertificateTest, FromPEMString) {
  auto cert1 = rtc::WrapUnique<OpenSSLCertificate>(
      OpenSSLCertificate::FromPEMString(kCert1));
  auto chain_cert2 = rtc::WrapUnique<OpenSSLCertificate>(
      OpenSSLCertificate::FromPEMString(std::string(kCert1) + kCert2));
  ASSERT_EQ(cert1->ToPEMString(), kCert1);
  ASSERT_EQ(chain_cert2->GetChain()->GetSize(), (size_t)1);
  ASSERT_EQ(chain_cert2->GetChain()->Get(0).ToPEMString(), kCert2);
}
