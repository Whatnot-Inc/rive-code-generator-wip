#include "lint.h"
#include "rive_file_parser.h"
#include "template_renderer.h"
#include "types.h"

#include "CLIUTILS/CLI11.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static const std::string kGeneratedFileName = "rive_generated";

int main(int argc, char* argv[])
{
    CLI::App app{"Rive Code Generator"};

    std::string inputPath;
    std::string outputFilePath;
    std::string templatePath;
    Language language = Language::Dart;
    TemplateEngine templateEngine = TemplateEngine::Mustache;
    bool ignorePrivate = false;

    bool lintMode = false;
    std::vector<std::string> allowedExtensions;
    size_t maxAssetSize = 200 * 1024;
    std::string lintFormat = "text";

    app.add_option("-i,--input",
                   inputPath,
                   "Path to Rive file or directory containing Rive files")
        ->required()
        ->check(CLI::ExistingFile | CLI::ExistingDirectory);

    app.add_option("-o,--output", outputFilePath, "Output file path");

    app.add_option("-t,--template", templatePath, "Custom template file path");

    app.add_option("-l,--language",
                   language,
                   "Programming language for code generation")
        ->transform(CLI::CheckedTransformer(
            std::map<std::string, Language>{{"dart", Language::Dart},
                                            {"js", Language::JavaScript}},
            CLI::ignore_case));

    app.add_option("-e,--engine",
                   templateEngine,
                   "Template engine to use (mustache or inja)")
        ->transform(CLI::CheckedTransformer(
            std::map<std::string, TemplateEngine>{{"mustache", TemplateEngine::Mustache},
                                                   {"inja", TemplateEngine::Inja}},
            CLI::ignore_case));

    app.add_flag("--ignore-private",
                 ignorePrivate,
                 "Skip artboards, animations, state machines, and properties starting with 'internal', 'private', or '_'");

    app.add_flag("--lint",
                 lintMode,
                 "Validate assets and artboards; exits non-zero on violations. "
                 "Can be combined with -o for lint + codegen, or used alone (omit -o).");

    app.add_option("--allowed-extensions",
                   allowedExtensions,
                   "Allowed image file extensions, e.g. webp or webp,avif (default: webp)")
        ->delimiter(',');

    app.add_option("--max-asset-size",
                   maxAssetSize,
                   "Maximum embedded asset size in bytes (default: 204800 = 200 KB)");

    app.add_option("--lint-format",
                   lintFormat,
                   "Lint output format: text (default) or json")
        ->transform(CLI::IsMember({"text", "json"}));

    CLI11_PARSE(app, argc, argv)

    if (outputFilePath.empty() && !lintMode)
    {
        std::cerr << "Error: -o/--output is required when not running in --lint mode."
                  << std::endl;
        return 1;
    }

    // Load template (only needed for codegen)
    std::string templateStr;
    if (!outputFilePath.empty())
    {
        auto tmpl = loadTemplate(templatePath, language);
        if (!tmpl)
            return 1;
        templateStr = *tmpl;
    }

    std::vector<std::string> riveFiles = findRiveFiles(inputPath);
    if (riveFiles.empty())
    {
        std::cerr << "No .riv files found in the specified path." << std::endl;
        return 1;
    }

    std::vector<RiveFileData> riveFileDataList;
    for (const auto& riv_file : riveFiles)
    {
        auto result = processRiveFile(riv_file, ignorePrivate);
        if (result)
            riveFileDataList.push_back(*result);
    }

    if (lintMode)
    {
        LintConfig lintConfig;
        lintConfig.allowedExtensions = allowedExtensions.empty()
                                           ? std::vector<std::string>{"webp"}
                                           : allowedExtensions;
        lintConfig.maxAssetBytes = maxAssetSize;
        lintConfig.jsonOutput = (lintFormat == "json");

        auto violations = runLint(riveFileDataList, lintConfig);
        int lintResult = outputLintViolations(violations, lintConfig);

        if (outputFilePath.empty())
            return lintResult;

        if (lintResult != 0)
            return lintResult;
    }

    auto result = renderTemplate(templateStr, kGeneratedFileName, riveFileDataList, templateEngine);
    if (!result)
        return 1;

    std::cout << "Rive: output_file_path = " << outputFilePath << std::endl;

    std::filesystem::path output_path(outputFilePath);
    if (output_path.is_relative() && output_path.parent_path().empty())
        output_path = std::filesystem::current_path() / output_path;

    std::filesystem::create_directories(output_path.parent_path());

    std::ofstream output_file(output_path);
    if (!output_file.is_open())
    {
        std::cerr << "Error: Unable to open output file: " << output_path << std::endl;
        return 1;
    }
    output_file << *result;
    output_file.close();

    std::cout << "File generated successfully: " << output_path << std::endl;
    return 0;
}
