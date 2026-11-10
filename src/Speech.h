#pragma once

#include <atomic>
#include <memory>

#include <Arduino.h>
#include <AudioFileSource.h>

#include <AudioOutputM5Speaker.h>

class Speech {
public:
  Speech();

  void begin();
  std::unique_ptr<AudioFileSource> synthesize(const char *text, float joy, float anger, float sorrow);
  void play(std::unique_ptr<AudioFileSource> source);
  float getRMS() const;

private:
  AudioOutputM5Speaker output_;
  uint8_t *buffer_;
  std::atomic<float> rms_ = 0.0f;
};
