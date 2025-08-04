// DecoderManager.h

#pragma once
#include <memory>
#include <string>
#include "TVMDecoder.h"

class DecoderManager {
public:
    static std::shared_ptr<TVMDecoder::Decoder> CreateDecoder(const std::string& name,
                                                              const std::string& path,
                                                              bool enableLogging = true);
};
