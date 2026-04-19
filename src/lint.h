#pragma once

#include "types.h"
#include "utils/no_op_factory.hpp"

#include <string>
#include <vector>

struct EmbeddedImageInfo
{
    size_t byteSize;
    std::string detectedExtension; // sniffed from magic bytes; empty if unrecognized
};

// Captures size and actual format of embedded image assets during File::import().
// CDN-hosted assets never call decodeImage, so only embedded images are captured.
// The detected extension takes precedence over asset->fileExtension() for lint checks,
// because fileExtension() reflects the source filename, not the embedded format.
class LintingFactory : public rive::NoOpFactory
{
public:
    std::vector<EmbeddedImageInfo> embeddedImages;

    rive::rcp<rive::RenderImage> decodeImage(rive::Span<const uint8_t> data) override
    {
        embeddedImages.push_back({data.size(), detectFormat(data)});
        return nullptr;
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
