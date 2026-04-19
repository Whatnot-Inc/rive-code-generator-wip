#pragma once

#include "types.h"
#include "utils/no_op_factory.hpp"

#include "rive/assets/file_asset.hpp"
#include "rive/file_asset_loader.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct EmbeddedImageInfo
{
    size_t byteSize;
    std::string detectedExtension; // sniffed from magic bytes; empty if unrecognized
};

// Observes asset resolution during File::import to capture size and actual format
// of embedded images. loadContents receives the asset name and in-band bytes:
//   inBandBytes.size() > 0  → embedded (bytes are in the .riv file)
//   inBandBytes.size() == 0 → referenced or CDN (app/runtime supplies them)
// Returns false so normal decode proceeds via the factory.
class LintingAssetLoader : public rive::FileAssetLoader
{
public:
    // asset.name() → embedded info; absent for referenced/CDN images.
    std::unordered_map<std::string, EmbeddedImageInfo> embeddedImages;

    bool loadContents(rive::FileAsset& asset,
                      rive::Span<const uint8_t> inBandBytes,
                      rive::Factory* factory) override
    {
        if (inBandBytes.size() > 0)
        {
            embeddedImages[asset.name()] = {inBandBytes.size(), detectFormat(inBandBytes)};
        }
        return false;
    }

private:
    static std::string detectFormat(rive::Span<const uint8_t> data)
    {
        if (data.size() >= 12 &&
            data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
            data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P')
        {
            return "webp";
        }
        if (data.size() >= 4 &&
            data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')
        {
            return "png";
        }
        if (data.size() >= 3 &&
            data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
        {
            return "jpeg";
        }
        if (data.size() >= 4 &&
            data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && data[3] == '8')
        {
            return "gif";
        }
        return "";
    }
};

std::vector<LintViolation> runLint(const std::vector<RiveFileData>& files,
                                    const LintConfig& config);

int outputLintViolations(const std::vector<LintViolation>& violations,
                          const LintConfig& config);
