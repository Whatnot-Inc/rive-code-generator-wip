#pragma once

#include "types.h"
#include "utils/no_op_factory.hpp"

#include <vector>

// Captures byte sizes of embedded image assets during File::import().
// CDN-hosted assets never call decodeImage, so only embedded images are counted.
class LintingFactory : public rive::NoOpFactory
{
public:
    std::vector<size_t> embeddedImageSizes;

    rive::rcp<rive::RenderImage> decodeImage(rive::Span<const uint8_t> data) override
    {
        embeddedImageSizes.push_back(data.size());
        return nullptr;
    }
};

std::vector<LintViolation> runLint(const std::vector<RiveFileData>& files,
                                    const LintConfig& config);

int outputLintViolations(const std::vector<LintViolation>& violations,
                          const LintConfig& config);
