#include <algorithm>
#include <atomic>
#include <cstring>
#include <utility>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Avatar.h>
#include <ESPmDNS.h>
#include <M5Unified.h>
#include <ServoEasing.hpp>
#include <WebServer.h>
#include <WiFi.h>

#include "Conversation.h"
#include "NukariFace.h"
#include "Speech.h"
#include "secrets.h"

namespace {

constexpr char MDNS_HOSTNAME[] = "nukari-cores3";
constexpr uint8_t SPEAKER_VOLUME = 150;

constexpr int SERVO_X_PIN = 1;
constexpr int SERVO_Y_PIN = 2;
constexpr int SERVO_X_MIN = 0;
constexpr int SERVO_X_MAX = 180;
constexpr int SERVO_X_CENTER = 90;
constexpr int SERVO_Y_MIN = 0;
constexpr int SERVO_Y_MAX = 180;
constexpr int SERVO_Y_CENTER = 90;

ServoEasing servo_x;
ServoEasing servo_y;

m5avatar::Avatar avatar;
std::atomic<m5avatar::Expression> face_expression = m5avatar::Expression::Sleepy;
std::atomic<const char *> face_text = "";

Conversation conversation;
Speech speech;

WebServer server;

m5avatar::Expression expressionOf(float joy, float anger, float sorrow) {
  auto [value, expression] =
      std::max({std::pair{joy, m5avatar::Expression::Happy}, std::pair{anger, m5avatar::Expression::Angry},
                std::pair{sorrow, m5avatar::Expression::Sad}},
               [](const auto &a, const auto &b) { return a.first < b.first; });
  return value > 0.0f ? expression : m5avatar::Expression::Neutral;
}

void setFace(m5avatar::Expression expression, const char *text = "") {
  face_expression = expression;
  face_text = text;
}

void faceTask(void *args) {
  auto avatar = static_cast<m5avatar::DriveContext *>(args)->getAvatar();
  const char *current_text = "";

  while (true) {
    m5avatar::Expression expression = face_expression;
    if (expression != avatar->getExpression()) {
      avatar->setExpression(expression);
    }
    const char *text = face_text;
    if (std::strcmp(text, current_text) != 0) {
      avatar->setSpeechText(text);
      current_text = text;
    }
    float level = std::min(1.0f, speech.getRMS() / 4000.0f);
    avatar->setMouthOpenRatio(level * (2.0f - level));
    delay(10);
  }
}

void handleSpeak() {
  JsonDocument request;
  if (deserializeJson(request, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "text/plain", "リクエストが不正です");
    return;
  }
  const char *text = request["text"];
  if (!text) {
    server.send(400, "text/plain", "リクエストが不正です");
    return;
  }
  const float joy = request["joy"] | 0.0f;
  const float anger = request["anger"] | 0.0f;
  const float sorrow = request["sorrow"] | 0.0f;
  setFace(m5avatar::Expression::Doubt);
  auto source = speech.synthesize(text, joy, anger, sorrow);
  if (!source) {
    server.send(500, "text/plain", "再生できませんでした");
    return;
  }
  setFace(expressionOf(joy, anger, sorrow));
  speech.play(std::move(source));
  server.send(200, "text/plain", "再生しました");
}

void handleLook() {
  JsonDocument request;
  if (deserializeJson(request, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "text/plain", "リクエストが不正です");
    return;
  }
  uint16_t duration_ms = std::clamp(request["duration_ms"] | 1000, 0, 10000);
  servo_x.setEaseToD(std::clamp(request["x"] | SERVO_X_CENTER, SERVO_X_MIN, SERVO_X_MAX), duration_ms);
  servo_y.setEaseToD(std::clamp(request["y"] | SERVO_Y_CENTER, SERVO_Y_MIN, SERVO_Y_MAX), duration_ms);
  synchronizeAllServosStartAndWaitForAllServosToStop();
  server.send(200, "text/plain", "サーボを動かしました");
}

} // namespace

void setup() {
  auto cfg = M5.config();
  cfg.output_power = false;
  M5.begin(cfg);
  M5.Speaker.setVolume(SPEAKER_VOLUME);
  M5.Speaker.begin();

  servo_x.attach(SERVO_X_PIN, SERVO_X_CENTER);
  servo_y.attach(SERVO_Y_PIN, SERVO_Y_CENTER);
  servo_x.setEasingType(EASE_QUADRATIC_IN_OUT);
  servo_y.setEasingType(EASE_QUADRATIC_IN_OUT);

  WiFi.setHostname(MDNS_HOSTNAME);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  MDNS.begin(MDNS_HOSTNAME);

  avatar.setFace(new NukariFace());
  avatar.setColorPalette(NukariColorPalette());
  avatar.setSpeechFont(&fonts::efontJA_16);
  avatar.start(16, 4096, 2048);
  avatar.addTask(faceTask, "face");

  conversation.begin();
  speech.begin();

  server.on("/speak", HTTP_POST, handleSpeak);
  server.on("/look", HTTP_POST, handleLook);
  server.begin();
}

void loop() {
  M5.update();
  server.handleClient();
  delay(10);

  if (WiFi.status() != WL_CONNECTED) {
    setFace(m5avatar::Expression::Sleepy);
    return;
  }

  if (!M5.Touch.getDetail().wasClicked()) {
    setFace(m5avatar::Expression::Neutral);
    return;
  }

  setFace(m5avatar::Expression::Neutral, "聞き取り中…");
  auto recording = conversation.record();
  if (!recording) {
    switch (recording.error()) {
    case Conversation::Error::NotHeard:
      setFace(m5avatar::Expression::Sad, "聞き取れません");
      break;
    case Conversation::Error::Failed:
      setFace(m5avatar::Expression::Sad, "エラー");
      break;
    }
    delay(2000);
    return;
  }

  setFace(m5avatar::Expression::Doubt);
  auto turn = conversation.ask(*recording);
  if (!turn) {
    switch (turn.error()) {
    case Conversation::Error::NotHeard:
      setFace(m5avatar::Expression::Sad, "聞き取れません");
      break;
    case Conversation::Error::Failed:
      setFace(m5avatar::Expression::Sad, "エラー");
      break;
    }
    delay(2000);
    return;
  }

  auto source = speech.synthesize(turn->said.c_str(), turn->joy, turn->anger, turn->sorrow);
  if (!source) {
    setFace(m5avatar::Expression::Sad, "エラー");
    delay(2000);
    return;
  }
  setFace(expressionOf(turn->joy, turn->anger, turn->sorrow));
  speech.play(std::move(source));
}
