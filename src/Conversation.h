#pragma once

#include <deque>
#include <span>

#include <Arduino.h>
#include <esp_vad.h>
#include <tl/expected.hpp>

#include <BufferedMic.h>

class Conversation {
public:
  struct Turn {
    String heard;
    String said;
    float joy;
    float anger;
    float sorrow;
  };

  enum class Error {
    NotHeard,
    Failed,
  };

  Conversation();

  void begin();
  tl::expected<std::span<const int16_t>, Error> record();
  tl::expected<Turn, Error> ask(std::span<const int16_t> recording);

private:
  BufferedMic mic_;
  vad_handle_t vad_;
  int16_t *recording_buffer_;
  char *base64_buffer_;
  std::deque<Turn> history_;
};
