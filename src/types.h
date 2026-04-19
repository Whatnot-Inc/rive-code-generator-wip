#pragma once

#include <cstddef>
#include <string>
#include <vector>

enum class CaseStyle
{
    Camel,
    Pascal,
    Snake,
    Kebab,
};

enum class Language
{
    Dart,
    JavaScript
};

enum class TemplateEngine
{
    Mustache,
    Inja
};

struct InputInfo
{
    std::string name;
    std::string type;
    std::string defaultValue;
};

struct TextValueRunInfo
{
    std::string name;
    std::string defaultValue;
};

struct NestedTextValueRunInfo
{
    std::string name;
    std::string path;
};

struct AssetInfo
{
    std::string name;
    std::string type;
    std::string fileExtension;
    std::string assetId;
    std::string cdnUuid;
    std::string cdnBaseUrl;
    size_t embeddedByteSize = 0; // 0 for CDN-hosted assets or non-image types
};

struct EnumValueInfo
{
    std::string key;
};

struct EnumInfo
{
    std::string name;
    std::vector<EnumValueInfo> values;
};

struct PropertyInfo
{
    std::string name;
    std::string type;
    std::string backingName;
    std::string defaultValue;
};

struct ViewModelInfo
{
    std::string name;
    std::vector<PropertyInfo> properties;
};

struct ArtboardData
{
    std::string artboardName;
    std::string artboardPascalCase;
    std::string artboardCameCase;
    std::string artboardSnakeCase;
    std::string artboardKebabCase;
    std::vector<std::string> animations;
    std::vector<std::pair<std::string, std::vector<InputInfo>>> stateMachines;
    std::vector<TextValueRunInfo> textValueRuns;
    std::vector<NestedTextValueRunInfo> nestedTextValueRuns;

    bool isDefault;
    uint32_t viewModelId;
    std::string viewModelName;
    bool hasViewModel;
    std::string defaultStateMachineName;
    bool hasDefaultStateMachine;
};

struct RiveFileData
{
    std::string rivOriginalFileName;
    std::string rivPascalCase;
    std::string rivCameCase;
    std::string riveSnakeCase;
    std::string rivKebabCase;
    std::vector<ArtboardData> artboards;
    std::vector<AssetInfo> assets;
    std::vector<EnumInfo> enums;
    std::vector<ViewModelInfo> viewmodels;

    std::string defaultArtboardName;
    std::string defaultStateMachineName;
    std::string defaultViewModelName;
    bool hasDefaults;
};

struct LintConfig
{
    std::vector<std::string> allowedExtensions = {"webp"};
    size_t maxAssetBytes = 200 * 1024; // 200 KB
    bool jsonOutput = false;
};

struct LintViolation
{
    enum class Type
    {
        DisallowedAssetFormat,
        OversizedAsset,
        NoStateMachine,
    };
    Type type;
    std::string fileName;
    std::string name;   // asset name for asset violations, artboard name for SM violations
    std::string detail; // file extension for format violations, byte count for size violations
};
