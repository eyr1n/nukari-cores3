#include "GeminiClient.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>

namespace {

constexpr char BASE_URL[] = "https://generativelanguage.googleapis.com/v1beta/models/";
constexpr uint16_t TIMEOUT_MS = 30000;

} // namespace

GeminiClient::GeminiClient(const char *api_key, const char *model) : api_key_(api_key), model_(model) {}

tl::expected<JsonDocument, GeminiClient::Error> GeminiClient::generateContent(JsonVariantConst request) {
  String body;
  serializeJson(request, body);

  NetworkClientSecure client;
  client.useBuiltinCACertBundle();
  HTTPClient http;
  http.setTimeout(TIMEOUT_MS);
  if (!http.begin(client, String(BASE_URL) + model_ + ":generateContent")) {
    return tl::make_unexpected(ConnectionError{});
  }
  http.addHeader("x-goog-api-key", api_key_);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(reinterpret_cast<uint8_t *>(const_cast<char *>(body.c_str())), body.length());
  if (code != HTTP_CODE_OK) {
    return tl::make_unexpected(HTTPError{code});
  }

  JsonDocument doc;
  if (deserializeJson(doc, http.getString()) != DeserializationError::Ok) {
    return tl::make_unexpected(ResponseError{});
  }
  return doc;
}
