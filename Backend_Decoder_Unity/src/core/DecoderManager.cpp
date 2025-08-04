// DecoderManager.cpp

#include "DecoderManager.h"
#include "TVMLogger.h"

std::shared_ptr<TVMDecoder::Decoder> DecoderManager::CreateDecoder(const std::string& name,
                                                                   const std::string& path,
                                                                   bool enableLogging) {
    TVMLogger::EnableLogging(enableLogging);
    auto decoder = std::make_shared<TVMDecoder::Decoder>(name, path);
    LOG_INFO("✅ New Decoder created!");
    return decoder;
}
