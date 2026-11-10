#include "Conversation.h"

#include <algorithm>

#include <ArduinoJson.h>
#include <mbedtls/base64.h>

#include <GeminiClient.h>

#include "secrets.h"

namespace {

constexpr int SAMPLE_RATE = 16000;
constexpr int CHUNK_MS = 30;
constexpr int CHUNK_FRAMES = SAMPLE_RATE * CHUNK_MS / 1000;
constexpr int MAX_RECORDING_MS = 10000;
constexpr int MAX_RECORDING_CHUNKS = MAX_RECORDING_MS / CHUNK_MS;
constexpr int MIN_SPEECH_MS = 128;
constexpr int MIN_NOISE_MS = 1000;
constexpr int SPEECH_TIMEOUT_MS = 5000;

constexpr size_t RECORDING_BUFFER_SIZE = MAX_RECORDING_CHUNKS * CHUNK_FRAMES * sizeof(int16_t);
constexpr size_t BASE64_BUFFER_SIZE = (RECORDING_BUFFER_SIZE + 2) / 3 * 4 + 1;
constexpr size_t HISTORY_SIZE = 10;

constexpr char MODEL[] = "gemini-3.5-flash-lite";

constexpr char RESPONSE_SCHEMA[] =
    R"({"type":"OBJECT","properties":{)"
    R"("heard":{"type":"STRING","nullable":true,"description":"音声から聞き取った発言、聞き取れなければnull"},)"
    R"("said":{"type":"STRING","description":"発言に対する返事"},)"
    R"("joy":{"type":"NUMBER","minimum":0,"maximum":1,"description":"返事に込める喜びの度合い"},)"
    R"("anger":{"type":"NUMBER","minimum":0,"maximum":1,"description":"返事に込める怒りの度合い"},)"
    R"("sorrow":{"type":"NUMBER","minimum":0,"maximum":1,"description":"返事に込める悲しみの度合い"}},)"
    R"("required":["heard","said","joy","anger","sorrow"]})";

} // namespace

Conversation::Conversation() : mic_(SAMPLE_RATE, CHUNK_FRAMES) {}

void Conversation::begin() {
  mic_.init();
  vad_ = vad_create_with_param(VAD_MODE_1, SAMPLE_RATE, CHUNK_MS, MIN_SPEECH_MS, MIN_NOISE_MS);
  recording_buffer_ = static_cast<int16_t *>(ps_malloc(RECORDING_BUFFER_SIZE));
  base64_buffer_ = static_cast<char *>(ps_malloc(BASE64_BUFFER_SIZE));
}

tl::expected<std::span<const int16_t>, Conversation::Error> Conversation::record() {
  if (!mic_.begin()) {
    return tl::make_unexpected(Error::Failed);
  }
  vad_reset_trigger(vad_);
  int16_t *recording_end = nullptr;
  for (int i = 0; i < MAX_RECORDING_CHUNKS; ++i) {
    int16_t *chunk = recording_buffer_ + i * CHUNK_FRAMES;
    if (!mic_.record(chunk)) {
      mic_.end();
      return tl::make_unexpected(Error::Failed);
    }
    if (vad_process_with_trigger(vad_, chunk) == VAD_SPEECH) {
      recording_end = chunk + CHUNK_FRAMES;
    } else if (recording_end || (i + 1) * CHUNK_MS >= SPEECH_TIMEOUT_MS) {
      break;
    }
  }
  mic_.end();
  if (!recording_end) {
    return tl::make_unexpected(Error::NotHeard);
  }
  return std::span<const int16_t>(recording_buffer_, recording_end);
}

tl::expected<Conversation::Turn, Conversation::Error> Conversation::ask(std::span<const int16_t> recording) {
  // システムプロンプトの埋め込み
  JsonDocument request;
  request["systemInstruction"]["parts"][0]["text"] = GEMINI_SYSTEM_PROMPT;
  JsonObject config = request["generationConfig"].to<JsonObject>();
  config["responseMimeType"] = "application/json";
  deserializeJson(config["responseSchema"].to<JsonVariant>(), RESPONSE_SCHEMA);

  // 履歴の埋め込み
  JsonArray contents = request["contents"].to<JsonArray>();
  for (const auto &turn : history_) {
    JsonObject user = contents.add<JsonObject>();
    user["role"] = "user";
    user["parts"][0]["text"] = turn.heard;
    JsonObject model = contents.add<JsonObject>();
    model["role"] = "model";
    model["parts"][0]["text"] = turn.said;
  }

  // 音声の埋め込み
  size_t base64_size;
  mbedtls_base64_encode(reinterpret_cast<unsigned char *>(base64_buffer_), BASE64_BUFFER_SIZE, &base64_size,
                        reinterpret_cast<const unsigned char *>(recording.data()), recording.size_bytes());
  JsonObject user = contents.add<JsonObject>();
  user["role"] = "user";
  auto audio_pcm = user["parts"][0]["inlineData"].to<JsonObject>();
  audio_pcm["mimeType"] = String("audio/pcm;rate=") + SAMPLE_RATE;
  audio_pcm["data"] = base64_buffer_;

  // Geminiに聞く
  GeminiClient gemini(GEMINI_API_KEY, MODEL);
  auto response = gemini.generateContent(request);
  if (!response) {
    return tl::make_unexpected(Error::Failed);
  }

  // 応答の取り出し
  const char *candidate = (*response)["candidates"][0]["content"]["parts"][0]["text"];
  JsonDocument output;
  if (deserializeJson(output, candidate) != DeserializationError::Ok) {
    return tl::make_unexpected(Error::Failed);
  }
  if (output["heard"].isNull()) {
    return tl::make_unexpected(Error::NotHeard);
  }

  Turn turn;
  turn.heard = output["heard"].as<String>();
  turn.said = output["said"].as<String>();
  turn.joy = std::clamp(output["joy"] | 0.0f, 0.0f, 1.0f);
  turn.anger = std::clamp(output["anger"] | 0.0f, 0.0f, 1.0f);
  turn.sorrow = std::clamp(output["sorrow"] | 0.0f, 0.0f, 1.0f);

  history_.push_back(turn);
  if (history_.size() > HISTORY_SIZE) {
    history_.pop_front();
  }
  return turn;
}
