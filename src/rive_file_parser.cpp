#include "rive_file_parser.h"
#include "string_utils.h"

#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/assets/audio_asset.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/generated/animation/state_machine_bool_base.hpp"
#include "rive/generated/animation/state_machine_number_base.hpp"
#include "rive/generated/animation/state_machine_trigger_base.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/viewmodel/data_enum.hpp"
#include "rive/viewmodel/data_enum_value.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
#include "rive/viewmodel/viewmodel_property_enum.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

// ---------------------------------------------------------------------------
// File discovery
// ---------------------------------------------------------------------------

std::vector<std::string> findRiveFiles(const std::string& path)
{
    std::vector<std::string> riveFiles;

    if (std::filesystem::is_regular_file(path))
    {
        if (std::filesystem::path(path).extension() == ".riv")
        {
            riveFiles.push_back(path);
        }
        return riveFiles;
    }

    if (std::filesystem::is_directory(path))
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".riv")
            {
                riveFiles.push_back(entry.path().string());
            }
        }
    }

    return riveFiles;
}

// ---------------------------------------------------------------------------
// File loading
// ---------------------------------------------------------------------------

rive::rcp<rive::File> openFile(const char name[], rive::Factory& factory)
{
    FILE* f = fopen(name, "rb");
    if (!f)
    {
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    auto length = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> bytes(length);

    if (fread(bytes.data(), 1, length, f) != length)
    {
        printf("Failed to read file into bytes array\n");
        fclose(f);
        return nullptr;
    }

    fclose(f);
    return rive::File::import(bytes, &factory);
}

// ---------------------------------------------------------------------------
// Filtering
// ---------------------------------------------------------------------------

static bool shouldIncludeElement(const std::string& name, bool ignorePrivate)
{
    if (!ignorePrivate)
    {
        return true;
    }

    if (!name.empty() && name[0] == '_')
    {
        return false;
    }

    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    if (lowerName.rfind("internal", 0) == 0 || lowerName.rfind("private", 0) == 0)
    {
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Artboard helpers
// ---------------------------------------------------------------------------

template <typename T = rive::Component>
static void findAll(std::vector<T*>& results, rive::ArtboardInstance* artboard)
{
    for (auto object : artboard->objects())
    {
        if (object != nullptr && object->is<T>())
        {
            results.push_back(static_cast<T*>(object));
        }
    }
}

static std::vector<std::string> getAnimationsFromArtboard(rive::ArtboardInstance* artboard,
                                                            bool ignorePrivate = false)
{
    std::vector<std::string> animations;
    auto animationCount = artboard->animationCount();
    for (int i = 0; i < animationCount; i++)
    {
        auto animation = artboard->animationAt(i);
        std::string animationName = animation->name();

        if (!shouldIncludeElement(animationName, ignorePrivate))
        {
            continue;
        }

        animations.push_back(animationName);
    }
    return animations;
}

static std::vector<std::pair<std::string, std::vector<InputInfo>>>
getStateMachinesFromArtboard(rive::ArtboardInstance* artboard, bool ignorePrivate = false)
{
    std::vector<std::pair<std::string, std::vector<InputInfo>>> stateMachines;
    auto stateMachineCount = artboard->stateMachineCount();
    for (int i = 0; i < stateMachineCount; i++)
    {
        auto stateMachine = artboard->stateMachineAt(i);
        std::string stateMachineName = stateMachine->name();

        if (!shouldIncludeElement(stateMachineName, ignorePrivate))
        {
            continue;
        }

        std::vector<InputInfo> inputs;
        auto inputCount = stateMachine->inputCount();
        for (int j = 0; j < inputCount; j++)
        {
            auto input = stateMachine->input(j);

            std::string inputType;
            std::string defaultValue;

            switch (input->inputCoreType())
            {
                case rive::StateMachineNumberBase::typeKey:
                {
                    auto smiNumberInput = static_cast<rive::SMINumber*>(input);
                    inputType = "number";
                    defaultValue = std::to_string(smiNumberInput->value());
                    break;
                }
                case rive::StateMachineTriggerBase::typeKey:
                {
                    inputType = "trigger";
                    defaultValue = "false";
                    break;
                }
                case rive::StateMachineBoolBase::typeKey:
                {
                    auto smiBoolInput = static_cast<rive::SMIBool*>(input);
                    inputType = "boolean";
                    defaultValue = smiBoolInput->value() ? "true" : "false";
                    break;
                }
                default:
                {
                    inputType = "unknown";
                    defaultValue = "";
                    break;
                }
            }

            inputs.push_back({input->name(), inputType, defaultValue});
        }

        stateMachines.emplace_back(stateMachineName, inputs);
    }
    return stateMachines;
}

static std::vector<TextValueRunInfo> getTextValueRunsFromArtboard(
    rive::ArtboardInstance* artboard)
{
    std::vector<rive::TextValueRun*> textValueRuns;
    std::vector<TextValueRunInfo> textValueRunsInfo;

    findAll<rive::TextValueRun>(textValueRuns, artboard);

    for (auto textValueRun : textValueRuns)
    {
        if (!textValueRun->name().empty())
        {
            textValueRunsInfo.push_back({textValueRun->name(), textValueRun->text()});
        }
    }
    return textValueRunsInfo;
}

static std::vector<NestedTextValueRunInfo> getNestedTextValueRunPathsFromArtboard(
    rive::ArtboardInstance* artboard,
    const std::string& currentPath = "")
{
    std::vector<NestedTextValueRunInfo> nestedTextValueRunsInfo;
    auto count = artboard->nestedArtboards().size();

    if (!currentPath.empty())
    {
        auto textRuns = getTextValueRunsFromArtboard(artboard);
        for (const auto& textRun : textRuns)
        {
            nestedTextValueRunsInfo.push_back({textRun.name, currentPath});
        }
    }

    for (int i = 0; i < count; i++)
    {
        auto nested = artboard->nestedArtboards()[i];
        auto nestedName = nested->name();
        if (!nestedName.empty())
        {
            std::string newPath = currentPath.empty()
                                       ? nested->name()
                                       : currentPath + "/" + nested->name();

            auto nestedArtboardInst = nested->artboardInstance();
            if (nestedArtboardInst)
            {
                auto nestedResults =
                    getNestedTextValueRunPathsFromArtboard(nestedArtboardInst, newPath);
                nestedTextValueRunsInfo.insert(nestedTextValueRunsInfo.end(),
                                               nestedResults.begin(),
                                               nestedResults.end());
            }
        }
    }

    return nestedTextValueRunsInfo;
}

// ---------------------------------------------------------------------------
// Asset extraction
// ---------------------------------------------------------------------------

static std::string dataTypeToString(rive::DataType type)
{
    switch (type)
    {
        case rive::DataType::none:            return "none";
        case rive::DataType::string:          return "string";
        case rive::DataType::number:          return "number";
        case rive::DataType::boolean:         return "boolean";
        case rive::DataType::color:           return "color";
        case rive::DataType::list:            return "list";
        case rive::DataType::enumType:        return "enum";
        case rive::DataType::trigger:         return "trigger";
        case rive::DataType::viewModel:       return "viewModel";
        case rive::DataType::integer:         return "integer";
        case rive::DataType::symbolListIndex: return "symbolListIndex";
        case rive::DataType::assetImage:      return "assetImage";
        default:                              return "unknown";
    }
}

static std::vector<AssetInfo> getAssetsFromFile(rive::File* file,
                                                 const std::vector<EmbeddedImageInfo>& embeddedImages)
{
    std::vector<AssetInfo> assetsInfo;
    std::unordered_set<std::string> usedAssetNames;
    size_t embeddedImageIndex = 0;

    auto assets = file->assets();
    for (auto asset : assets)
    {
        std::string assetType;
        switch (asset->coreType())
        {
            case rive::ImageAsset::typeKey: assetType = "image"; break;
            case rive::FontAsset::typeKey:  assetType = "font";  break;
            case rive::AudioAsset::typeKey: assetType = "audio"; break;
            default:                        assetType = "unknown"; break;
        }

        auto assetName = asset->name();
        auto uniqueAssetName = makeUnique(assetName, usedAssetNames);

        // For embedded images, use the format sniffed from magic bytes rather than
        // asset->fileExtension(), which reflects the source filename and may not match
        // the actual embedded format (e.g. PNG source re-encoded as webp in the editor).
        size_t embeddedByteSize = 0;
        std::string fileExtension = asset->fileExtension();
        if (assetType == "image" && asset->cdnUuidStr().empty())
        {
            if (embeddedImageIndex < embeddedImages.size())
            {
                const auto& info = embeddedImages[embeddedImageIndex++];
                embeddedByteSize = info.byteSize;
                if (!info.detectedExtension.empty())
                    fileExtension = info.detectedExtension;
            }
        }

        assetsInfo.push_back(AssetInfo{uniqueAssetName,
                                       assetType,
                                       fileExtension,
                                       std::to_string(asset->assetId()),
                                       asset->cdnUuidStr(),
                                       asset->cdnBaseUrl(),
                                       embeddedByteSize});
    }
    return assetsInfo;
}

// ---------------------------------------------------------------------------
// processRiveFile
// ---------------------------------------------------------------------------

std::optional<RiveFileData> processRiveFile(const std::string& riveFilePath, bool ignorePrivate)
{
    if (std::filesystem::is_empty(riveFilePath))
    {
        std::cerr << "Error: Rive file is empty: " << riveFilePath << std::endl;
        return std::nullopt;
    }

    LintingFactory factory;
    auto riveFile = openFile(riveFilePath.c_str(), factory);
    if (!riveFile)
    {
        std::cerr << "Error: Failed to parse Rive file: " << riveFilePath << std::endl;
        return std::nullopt;
    }

    std::filesystem::path path(riveFilePath);
    std::string fileNameWithoutExtension = path.stem().string();
    std::vector<AssetInfo> assets = getAssetsFromFile(riveFile.get(), factory.embeddedImages);

    RiveFileData fileData;
    fileData.rivOriginalFileName = fileNameWithoutExtension;
    fileData.rivPascalCase = toPascalCase(fileNameWithoutExtension);
    fileData.rivCameCase = toCamelCase(fileNameWithoutExtension);
    fileData.riveSnakeCase = toSnakeCase(fileNameWithoutExtension);
    fileData.rivKebabCase = toKebabCase(fileNameWithoutExtension);
    fileData.assets = assets;

    // Process enums
    const auto& fileEnums = riveFile->enums();
    for (auto* dataEnum : fileEnums)
    {
        if (dataEnum)
        {
            EnumInfo enumInfo;
            enumInfo.name = dataEnum->enumName();
            const auto& values = dataEnum->values();
            for (const auto* value : values)
            {
                enumInfo.values.push_back({value->key()});
            }
            fileData.enums.push_back(enumInfo);
        }
    }

    std::unordered_set<std::string> usedEnumNames;
    std::unordered_map<uint32_t, std::string> viewModelIdToName;

    // Process view models
    for (size_t i = 0; i < riveFile->viewModelCount(); i++)
    {
        auto viewModel = riveFile->viewModelByIndex(i);
        if (viewModel)
        {
            std::cout << "Processing ViewModel " << i << ": " << viewModel->name() << std::endl;

            viewModelIdToName[i] = viewModel->name();

            if (!shouldIncludeElement(viewModel->name(), ignorePrivate))
            {
                std::cout << "  Skipping ViewModel (private): " << viewModel->name() << std::endl;
                continue;
            }

            std::cout << "  Including ViewModel: " << viewModel->name() << std::endl;
            ViewModelInfo viewModelInfo;
            viewModelInfo.name = viewModel->name();
            std::cout << "  Getting properties..." << std::endl;
            auto propertiesData = viewModel->properties();
            std::cout << "  Found " << propertiesData.size() << " properties" << std::endl;

            for (const auto& property : propertiesData)
            {
                if (!shouldIncludeElement(property.name, ignorePrivate))
                {
                    continue;
                }

                if (property.type == rive::DataType::viewModel)
                {
                    auto vmInstance = viewModel->createInstance();
                    if (!vmInstance)
                    {
                        std::cerr << "Warning: Failed to create instance for ViewModel: "
                                  << viewModel->name() << std::endl;
                        continue;
                    }

                    auto nestedViewModel = vmInstance->propertyViewModel(property.name);
                    if (!nestedViewModel)
                    {
                        std::cerr << "Warning: Failed to get property ViewModel: "
                                  << property.name << std::endl;
                        continue;
                    }

                    auto nestedInstance = nestedViewModel->instance();
                    if (!nestedInstance)
                    {
                        std::cerr << "Warning: Failed to get nested ViewModel instance for: "
                                  << property.name << std::endl;
                        continue;
                    }

                    auto vm = nestedInstance->viewModel();
                    if (!vm)
                    {
                        std::cerr << "Warning: Failed to get nested ViewModel for: "
                                  << property.name << std::endl;
                        continue;
                    }

                    if (!shouldIncludeElement(vm->name(), ignorePrivate))
                    {
                        continue;
                    }

                    viewModelInfo.properties.push_back(
                        {property.name, dataTypeToString(property.type), vm->name(), ""});
                }
                else if (property.type == rive::DataType::enumType)
                {
                    auto vmi = riveFile->createViewModelInstance(viewModel->name());
                    if (!vmi)
                    {
                        std::cerr << "Warning: Failed to create ViewModel instance for: "
                                  << viewModel->name() << std::endl;
                        continue;
                    }

                    auto propertyValue = vmi->propertyValue(property.name);
                    if (!propertyValue)
                    {
                        std::cerr << "Warning: Failed to get property value for: "
                                  << property.name << std::endl;
                        continue;
                    }

                    auto enum_instance = static_cast<rive::ViewModelInstanceEnum*>(propertyValue);
                    if (!enum_instance)
                    {
                        std::cerr << "Warning: Failed to cast to ViewModelInstanceEnum for: "
                                  << property.name << std::endl;
                        continue;
                    }

                    auto vmProperty = enum_instance->viewModelProperty();
                    if (!vmProperty)
                    {
                        std::cerr << "Warning: Failed to get viewModelProperty for: "
                                  << property.name << std::endl;
                        continue;
                    }

                    auto enumProperty = vmProperty->as<rive::ViewModelPropertyEnum>();
                    if (!enumProperty)
                    {
                        std::cerr << "Warning: Failed to cast to ViewModelPropertyEnum for: "
                                  << property.name << std::endl;
                        continue;
                    }

                    auto dataEnum = enumProperty->dataEnum();
                    if (!dataEnum)
                    {
                        std::cerr << "Warning: Failed to get dataEnum for: "
                                  << property.name << std::endl;
                        continue;
                    }

                    auto enumName = dataEnum->enumName();
                    usedEnumNames.insert(enumName);

                    uint32_t defaultIndex = enum_instance->propertyValue();
                    std::string defaultValue = "";
                    if (defaultIndex < dataEnum->values().size())
                    {
                        defaultValue = dataEnum->values()[defaultIndex]->key();
                    }

                    viewModelInfo.properties.push_back(
                        {property.name, dataTypeToString(property.type), enumName, defaultValue});
                }
                else
                {
                    auto vmi = riveFile->createViewModelInstance(viewModel->name());
                    if (!vmi)
                    {
                        std::cerr << "Warning: Failed to create ViewModel instance for: "
                                  << viewModel->name() << std::endl;
                        continue;
                    }

                    std::string defaultValue = "";

                    if (property.type == rive::DataType::boolean)
                    {
                        auto pv = vmi->propertyValue(property.name);
                        if (pv)
                        {
                            auto b = static_cast<rive::ViewModelInstanceBoolean*>(pv);
                            if (b) defaultValue = b->propertyValue() ? "true" : "false";
                        }
                    }
                    else if (property.type == rive::DataType::number)
                    {
                        auto pv = vmi->propertyValue(property.name);
                        if (pv)
                        {
                            auto n = static_cast<rive::ViewModelInstanceNumber*>(pv);
                            if (n) defaultValue = std::to_string(n->propertyValue());
                        }
                    }
                    else if (property.type == rive::DataType::string)
                    {
                        auto pv = vmi->propertyValue(property.name);
                        if (pv)
                        {
                            auto s = static_cast<rive::ViewModelInstanceString*>(pv);
                            if (s) defaultValue = s->propertyValue();
                        }
                    }
                    else if (property.type == rive::DataType::color)
                    {
                        auto pv = vmi->propertyValue(property.name);
                        if (pv)
                        {
                            auto c = static_cast<rive::ViewModelInstanceColor*>(pv);
                            if (c)
                            {
                                std::stringstream ss;
                                ss << "0x" << std::hex << std::uppercase << std::setfill('0')
                                   << std::setw(8) << static_cast<unsigned int>(c->propertyValue());
                                defaultValue = ss.str();
                            }
                        }
                    }
                    // assetImage, trigger and other types don't have extractable default values

                    viewModelInfo.properties.push_back(
                        {property.name, dataTypeToString(property.type), "", defaultValue});
                }
            }

            fileData.viewmodels.push_back(viewModelInfo);
            std::cout << "  Finished processing ViewModel: " << viewModel->name() << std::endl;
        }
    }

    std::cout << "Finished processing all ViewModels. Total included: "
              << fileData.viewmodels.size() << std::endl;

    // Filter enums to only those used by non-filtered ViewModels
    if (ignorePrivate && !fileData.viewmodels.empty())
    {
        std::vector<EnumInfo> filteredEnums;
        for (const auto& enumInfo : fileData.enums)
        {
            if (usedEnumNames.count(enumInfo.name) > 0)
            {
                filteredEnums.push_back(enumInfo);
            }
        }
        fileData.enums = filteredEnums;
    }

    std::cout << "Extracting default relationship chain..." << std::endl;

    auto defaultArtboard = riveFile->artboard();
    fileData.hasDefaults = (defaultArtboard != nullptr);

    if (fileData.hasDefaults)
    {
        fileData.defaultArtboardName = defaultArtboard->name();
        std::cout << "  Default artboard: " << fileData.defaultArtboardName << std::endl;

        auto defaultArtboardInstance = defaultArtboard->instance();
        if (defaultArtboardInstance)
        {
            auto defaultStateMachineInstance = defaultArtboardInstance->defaultStateMachine();
            if (defaultStateMachineInstance)
            {
                fileData.defaultStateMachineName = defaultStateMachineInstance->name();
                std::cout << "  Default state machine: "
                          << fileData.defaultStateMachineName << std::endl;
            }
        }

        auto defaultViewModelRuntime = riveFile->defaultArtboardViewModel(defaultArtboard);
        if (defaultViewModelRuntime)
        {
            fileData.defaultViewModelName = defaultViewModelRuntime->name();
            std::cout << "  Default ViewModel: " << fileData.defaultViewModelName << std::endl;
        }
    }

    std::cout << "Processing artboards..." << std::endl;

    std::unordered_set<std::string> usedArtboardNames;
    auto artboardCount = riveFile->artboardCount();
    std::cout << "Total artboard count: " << artboardCount << std::endl;

    for (int i = 0; i < artboardCount; i++)
    {
        auto artboard = riveFile->artboardAt(i);
        std::string artboardName = artboard->name();
        std::cout << "Processing artboard " << i << ": " << artboardName << std::endl;

        if (!shouldIncludeElement(artboardName, ignorePrivate))
        {
            std::cout << "  Skipping artboard (private): " << artboardName << std::endl;
            continue;
        }

        std::string artboardPascalCase = toPascalCase(artboardName);
        std::string artboardCameCase = toCamelCase(artboardName);
        std::string artboardSnakeCase = toSnakeCase(artboardName);
        std::string artboardKebabCase = toKebabCase(artboardName);

        artboardCameCase = makeUnique(artboardCameCase, usedArtboardNames);

        std::cout << "  Getting animations..." << std::endl;
        auto animations = getAnimationsFromArtboard(artboard.get(), ignorePrivate);
        std::cout << "  Getting state machines..." << std::endl;
        auto stateMachines = getStateMachinesFromArtboard(artboard.get(), ignorePrivate);
        std::cout << "  Getting text value runs..." << std::endl;
        auto textValueRuns = getTextValueRunsFromArtboard(artboard.get());
        std::cout << "  Getting nested text value runs..." << std::endl;
        auto nestedTextValueRuns = getNestedTextValueRunPathsFromArtboard(artboard.get());
        std::cout << "  Done getting artboard data" << std::endl;

        bool isDefault = (i == 0);
        uint32_t artboardViewModelId = artboard->viewModelId();

        std::string viewModelName = "";
        bool hasViewModel = false;
        auto vmIt = viewModelIdToName.find(artboardViewModelId);
        if (vmIt != viewModelIdToName.end())
        {
            viewModelName = vmIt->second;
            for (const auto& vm : fileData.viewmodels)
            {
                if (vm.name == viewModelName)
                {
                    hasViewModel = true;
                    break;
                }
            }
        }

        std::string defaultStateMachineName = "";
        bool hasDefaultStateMachine = false;
        auto artboardInstance = artboard->instance();
        if (artboardInstance)
        {
            auto defaultStateMachine = artboardInstance->defaultStateMachine();
            if (defaultStateMachine)
            {
                defaultStateMachineName = defaultStateMachine->name();
                hasDefaultStateMachine = true;
            }
        }

        fileData.artboards.push_back({artboardName,
                                       artboardPascalCase,
                                       artboardCameCase,
                                       artboardSnakeCase,
                                       artboardKebabCase,
                                       animations,
                                       stateMachines,
                                       textValueRuns,
                                       nestedTextValueRuns,
                                       isDefault,
                                       artboardViewModelId,
                                       viewModelName,
                                       hasViewModel,
                                       defaultStateMachineName,
                                       hasDefaultStateMachine});
    }

    return fileData;
}
