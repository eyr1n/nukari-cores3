#pragma once

#include <Arduino.h>

class BufferedMic {
public:
  BufferedMic(uint32_t sample_rate, size_t chunk_frames);

  void init();
  bool begin();
  bool record(int16_t *data);
  void end();

private:
  uint32_t sample_rate_;
  size_t chunk_frames_;
  int16_t *buf_;
  QueueHandle_t filled_chunks_;
};
