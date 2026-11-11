#include "AIVoiceClient.h"

#include <utility>

#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include <AudioFileSourceHTTPClient.h>

namespace {

constexpr char BASE_URL[] = "https://aivoice.eyrin.jp";
constexpr uint16_t TIMEOUT_MS = 30000;

} // namespace

AIVoiceClient::AIVoiceClient(const char *client_id, const char *client_secret)
    : client_id_(client_id), client_secret_(client_secret) {}

tl::expected<JsonDocument, AIVoiceClient::Error> AIVoiceClient::getVoices() { return getJson("/voices"); }

tl::expected<std::unique_ptr<AudioFileSource>, AIVoiceClient::Error>
AIVoiceClient::synthesize(JsonVariantConst request) {
  String body;
  serializeJson(request, body);

  auto client = std::make_unique<NetworkClientSecure>();
  client->useBuiltinCACertBundle();
  auto http = std::make_unique<HTTPClient>();
  http->setTimeout(TIMEOUT_MS);
  if (!http->begin(*client, String(BASE_URL) + "/synthesize")) {
    return tl::make_unexpected(ConnectionError{});
  }
  http->addHeader("CF-Access-Client-Id", client_id_);
  http->addHeader("CF-Access-Client-Secret", client_secret_);
  http->addHeader("Content-Type", "application/json");

  int code = http->POST(body);
  if (code != HTTP_CODE_OK) {
    return tl::make_unexpected(HTTPError{code});
  }
  return std::make_unique<AudioFileSourceHTTPClient>(std::move(client), std::move(http));
}

tl::expected<JsonDocument, AIVoiceClient::Error> AIVoiceClient::getJson(const String &path) {
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();
  HTTPClient http;
  http.setTimeout(TIMEOUT_MS);
  if (!http.begin(client, String(BASE_URL) + path)) {
    return tl::make_unexpected(ConnectionError{});
  }
  http.addHeader("CF-Access-Client-Id", client_id_);
  http.addHeader("CF-Access-Client-Secret", client_secret_);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    return tl::make_unexpected(HTTPError{code});
  }

  JsonDocument doc;
  if (deserializeJson(doc, http.getString()) != DeserializationError::Ok) {
    return tl::make_unexpected(ResponseError{});
  }
  return doc;
}
