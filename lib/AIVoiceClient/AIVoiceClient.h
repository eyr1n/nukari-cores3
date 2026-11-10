#pragma once

#include <memory>
#include <variant>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AudioFileSource.h>
#include <tl/expected.hpp>

class AIVoiceClient {
public:
  struct ConnectionError {};
  struct ResponseError {};
  struct HTTPError {
    int code;
  };
  using Error = std::variant<ConnectionError, HTTPError, ResponseError>;

  AIVoiceClient(const char *client_id, const char *client_secret);

  tl::expected<JsonDocument, Error> getPresets();
  tl::expected<JsonDocument, Error> getPreset(const String &name);
  tl::expected<std::unique_ptr<AudioFileSource>, Error> synthesize(JsonVariantConst request);

private:
  const char *client_id_;
  const char *client_secret_;

  tl::expected<JsonDocument, Error> getJson(const String &path);
};
