#pragma once

#include <variant>

#include <ArduinoJson.h>
#include <tl/expected.hpp>

class GeminiClient {
public:
  struct ConnectionError {};
  struct ResponseError {};
  struct HTTPError {
    int code;
  };
  using Error = std::variant<ConnectionError, HTTPError, ResponseError>;

  GeminiClient(const char *api_key, const char *model);

  tl::expected<JsonDocument, Error> generateContent(JsonVariantConst request);

private:
  const char *api_key_;
  const char *model_;
};
