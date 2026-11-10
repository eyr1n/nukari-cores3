#include "Speech.h"

#include <cmath>
#include <utility>

#include <ArduinoJson.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorWAV.h>
#include <M5Unified.h>

#include <AIVoiceClient.h>

#include "secrets.h"

namespace {

constexpr char AIVOICE_PRESET[] = "結月 ゆかり";
constexpr uint32_t SAMPLE_RATE = 24000;
constexpr size_t BUFFER_SIZE = 2 * SAMPLE_RATE * sizeof(int16_t);
constexpr uint32_t SILENCE_MS = 100;

} // namespace

Speech::Speech() : output_(&M5.Speaker) {}

void Speech::begin() {
  output_.setup();
  output_.setMonitor(
      [](void *arg, const int16_t *stereo, size_t frames) {
        auto &rms = static_cast<Speech *>(arg)->rms_;
        if (frames == 0) {
          rms = 0.0f;
          return;
        }
        float sum = 0.0f;
        for (size_t i = 0; i < frames; ++i) {
          const float sample = stereo[i * 2];
          sum += sample * sample;
        }
        rms = std::sqrt(sum / frames);
      },
      this);
  buffer_ = static_cast<uint8_t *>(ps_malloc(BUFFER_SIZE));
}

std::unique_ptr<AudioFileSource> Speech::synthesize(const char *text, float joy, float anger, float sorrow) {
  JsonDocument request;
  request["text"] = text;
  request["preset"]["PresetName"] = AIVOICE_PRESET;
  request["sample_rate"] = SAMPLE_RATE;
  JsonArray styles = request["preset"]["Styles"].to<JsonArray>();
  styles[0]["Name"] = "J";
  styles[0]["Value"] = joy;
  styles[1]["Name"] = "A";
  styles[1]["Value"] = anger;
  styles[2]["Name"] = "S";
  styles[2]["Value"] = sorrow;

  AIVoiceClient aivoice(CF_ACCESS_CLIENT_ID, CF_ACCESS_CLIENT_SECRET);
  auto source = aivoice.synthesize(request);
  if (!source) {
    return nullptr;
  }
  return std::move(*source);
}

void Speech::play(std::unique_ptr<AudioFileSource> source) {
  AudioFileSourceBuffer buffered_source(source.get(), buffer_, BUFFER_SIZE);
  AudioGeneratorWAV wav;
  if (!wav.begin(&buffered_source, &output_)) {
    return;
  }
  int16_t silence[2] = {0, 0};
  for (uint32_t i = 0; i < SAMPLE_RATE * SILENCE_MS / 1000; ++i) {
    while (!output_.ConsumeSample(silence)) {
    }
  }
  while (wav.loop()) {
  }
}

float Speech::getRMS() const { return rms_; }
