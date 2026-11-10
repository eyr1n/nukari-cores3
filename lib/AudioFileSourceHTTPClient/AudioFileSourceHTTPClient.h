#pragma once

#include <memory>

#include <AudioFileSource.h>
#include <HTTPClient.h>
#include <NetworkClient.h>

class AudioFileSourceHTTPClient : public AudioFileSource {
public:
  AudioFileSourceHTTPClient(std::unique_ptr<NetworkClient> client, std::unique_ptr<HTTPClient> http);

  uint32_t read(void *data, uint32_t len) override;
  uint32_t readNonBlock(void *data, uint32_t len) override;
  bool isOpen() override;
  uint32_t getSize() override;
  uint32_t getPos() override;

private:
  std::unique_ptr<NetworkClient> client_;
  std::unique_ptr<HTTPClient> http_;
  int size_;
  int pos_ = 0;
};
