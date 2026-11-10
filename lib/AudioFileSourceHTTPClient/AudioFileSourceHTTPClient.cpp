#include "AudioFileSourceHTTPClient.h"

#include <algorithm>
#include <utility>

AudioFileSourceHTTPClient::AudioFileSourceHTTPClient(std::unique_ptr<NetworkClient> client,
                                                     std::unique_ptr<HTTPClient> http)
    : client_(std::move(client)), http_(std::move(http)), size_(http_->getSize()) {}

uint32_t AudioFileSourceHTTPClient::read(void *data, uint32_t len) {
  if (size_ > 0) {
    len = std::min<uint32_t>(len, size_ - pos_);
  }
  if (len == 0) {
    return 0;
  }
  NetworkClient *stream = http_->getStreamPtr();
  if (!stream) {
    return 0;
  }

  uint32_t remaining = len;
  unsigned long start = millis();
  while (stream->connected() && remaining > 0 && millis() - start < stream->getTimeout()) {
    int read = stream->read(static_cast<uint8_t *>(data) + (len - remaining), remaining);
    if (read > 0) {
      remaining -= read;
      pos_ += read;
    } else {
      delay(2);
    }
  }
  return len - remaining;
}

uint32_t AudioFileSourceHTTPClient::readNonBlock(void *data, uint32_t len) {
  if (size_ > 0) {
    len = std::min<uint32_t>(len, size_ - pos_);
  }
  if (len == 0) {
    return 0;
  }
  NetworkClient *stream = http_->getStreamPtr();
  if (!stream) {
    return 0;
  }

  int avail = stream->available();
  if (avail <= 0) {
    return 0;
  }
  int read = stream->read(static_cast<uint8_t *>(data), std::min<uint32_t>(len, avail));
  if (read <= 0) {
    return 0;
  }
  pos_ += read;
  return read;
}

bool AudioFileSourceHTTPClient::isOpen() { return http_->connected(); }

uint32_t AudioFileSourceHTTPClient::getSize() { return size_; }

uint32_t AudioFileSourceHTTPClient::getPos() { return pos_; }
