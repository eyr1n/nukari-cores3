#include "BufferedMic.h"

#include <cstdlib>
#include <cstring>

#include <M5Unified.h>

namespace {

constexpr size_t NUM_OF_CHUNKS = 4;

} // namespace

BufferedMic::BufferedMic(uint32_t sample_rate, size_t chunk_frames)
    : sample_rate_(sample_rate), chunk_frames_(chunk_frames) {}

void BufferedMic::init() {
  buf_ = static_cast<int16_t *>(std::malloc(NUM_OF_CHUNKS * chunk_frames_ * sizeof(int16_t)));
  filled_chunks_ = xQueueCreate(NUM_OF_CHUNKS, sizeof(int16_t *));

  M5.Mic.setBufferReleaseCallback(this, [](void *args, void *data, size_t) {
    auto self = static_cast<BufferedMic *>(args);
    auto chunk = static_cast<int16_t *>(data);
    xQueueSend(self->filled_chunks_, &chunk, 0);

    int16_t *next = chunk + self->chunk_frames_;
    if (next == self->buf_ + NUM_OF_CHUNKS * self->chunk_frames_) {
      next = self->buf_;
    }
    M5.Mic.record(next, self->chunk_frames_, self->sample_rate_);
  });
}

bool BufferedMic::begin() {
  M5.Speaker.end();
  if (!M5.Mic.begin()) {
    M5.Speaker.begin();
    return false;
  }

  xQueueReset(filled_chunks_);
  if (!M5.Mic.record(buf_, chunk_frames_, sample_rate_)) {
    end();
    return false;
  }
  return true;
}

bool BufferedMic::record(int16_t *data) {
  int16_t *chunk;
  if (!xQueueReceive(filled_chunks_, &chunk, pdMS_TO_TICKS(NUM_OF_CHUNKS * chunk_frames_ * 1000 / sample_rate_))) {
    return false;
  }
  std::memcpy(data, chunk, chunk_frames_ * sizeof(int16_t));
  return true;
}

void BufferedMic::end() {
  M5.Mic.end();
  M5.Speaker.begin();
}
