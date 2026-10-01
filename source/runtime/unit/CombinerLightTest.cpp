#include "PluginIndexMapping.h"
#include "libespm/Browser.h"
#include "libespm/CombineBrowser.h"
#include "libespm/Combiner.h"
#include "libespm/Loader.h"

#include <catch2/catch_all.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

template <class T>
void AppendValue(std::vector<uint8_t>& out, const T& value)
{
  const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(value));
}

void AppendBytes(std::vector<uint8_t>& out, const void* data, size_t size)
{
  if (size == 0) {
    return;
  }

  const auto* bytes = static_cast<const uint8_t*>(data);
  out.insert(out.end(), bytes, bytes + size);
}

void AppendField(std::vector<uint8_t>& fields, const char* type,
                 const void* data, uint16_t size)
{
  AppendBytes(fields, type, 4);
  AppendValue(fields, size);
  AppendBytes(fields, data, size);
}

void AppendRecord(std::vector<uint8_t>& plugin, const char* type,
                  uint32_t flags, uint32_t id,
                  const std::vector<uint8_t>& fields = {})
{
  AppendBytes(plugin, type, 4);
  AppendValue(plugin, static_cast<uint32_t>(fields.size()));
  AppendValue(plugin, flags);
  AppendValue(plugin, id);
  AppendValue(plugin, uint32_t{ 0 });
  AppendValue(plugin, uint16_t{ 44 });
  AppendValue(plugin, uint16_t{ 0 });
  AppendBytes(plugin, fields.data(), fields.size());
}

std::vector<uint8_t> MakePlugin(uint32_t flags,
                                const std::vector<std::string>& masters,
                                const std::vector<uint32_t>& recordIds = {})
{
  std::vector<uint8_t> fields;
  struct Header
  {
    float version;
    int32_t numRecords;
    uint32_t nextObjectId;
  };
  const Header header{ 1.7f, static_cast<int32_t>(recordIds.size()), 0x800 };
  AppendField(fields, "HEDR", &header, sizeof(header));
  for (const auto& master : masters) {
    AppendField(fields, "MAST", master.c_str(),
                static_cast<uint16_t>(master.size() + 1));
    const uint64_t data = 0;
    AppendField(fields, "DATA", &data, sizeof(data));
  }

  std::vector<uint8_t> plugin;
  AppendRecord(plugin, "TES4", flags, 0, fields);
  for (uint32_t recordId : recordIds) {
    AppendRecord(plugin, "KYWD", 0, recordId);
  }
  return plugin;
}

struct TestPlugin
{
  TestPlugin(uint32_t flags, const std::vector<std::string>& masters,
             const std::vector<uint32_t>& recordIds = {})
    : bytes(MakePlugin(flags, masters, recordIds))
    , browser(bytes.data(), bytes.size())
  {
  }

  std::vector<uint8_t> bytes;
  espm::Browser browser;
};

struct TempPluginDirectory
{
  TempPluginDirectory()
  {
    const auto suffix =
      std::chrono::steady_clock::now().time_since_epoch().count();
    path = std::filesystem::temp_directory_path() /
      ("skymp-combiner-light-" + std::to_string(suffix));
    std::filesystem::create_directories(path);
  }

  ~TempPluginDirectory()
  {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }

  std::filesystem::path Write(const std::string& name,
                              const std::vector<uint8_t>& bytes) const
  {
    const auto filePath = path / name;
    std::ofstream stream(filePath, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    stream.close();
    if (!stream) {
      throw std::runtime_error("unable to write synthetic plugin " + name);
    }
    return filePath;
  }

  std::filesystem::path path;
};

constexpr uint32_t kLightFlag = 0x00000200;

} // namespace

TEST_CASE("Combiner separates regular and light file namespaces",
          "[combiner-light]")
{
  TestPlugin skippedLight(kLightFlag, {}, { 0xFE000900 });
  TestPlugin regular0(0, {}, { 0x00000ABC });
  TestPlugin light0(kLightFlag, { "Regular0.esm" },
                    { 0xFE000801, 0x00000ABC });
  TestPlugin regular1(0, { "Regular0.esm", "Light0.esl" }, { 0x01001234 });
  TestPlugin light1(kLightFlag,
                    { "Regular0.esm", "Light0.esl", "Regular1.esm" },
                    { 0xFE001802, 0xFE000801, 0x01001234 });
  TestPlugin patch(
    0, { "Regular0.esm", "Light0.esl", "Regular1.esm", "Light1.esp" },
    { 0x02005678 });

  espm::Combiner combiner;
  const auto skipped = combiner.AddSource(&skippedLight.browser, "Skip.esl");
  const auto r0 = combiner.AddSource(&regular0.browser, "Regular0.esm");
  const auto l0 = combiner.AddSource(&light0.browser, "Light0.esl");
  const auto r1 = combiner.AddSource(&regular1.browser, "Regular1.esm");
  const auto l1 = combiner.AddSource(&light1.browser, "Light1.esp");
  const auto consumer = combiner.AddSource(&patch.browser, "Patch.esp");
  auto browser = combiner.Combine();

  REQUIRE(browser->ToRuntimeFormId(skipped, 0x900) == 0xFE000900);
  REQUIRE(browser->ToRuntimeFormId(r0, 0xABC) == 0x00000ABC);
  REQUIRE(browser->ToRuntimeFormId(l0, 0x801) == 0xFE001801);
  REQUIRE(browser->ToRuntimeFormId(r1, 0x1234) == 0x01001234);
  REQUIRE(browser->ToRuntimeFormId(l1, 0x802) == 0xFE002802);
  REQUIRE(browser->ToRuntimeFormId(consumer, 0x5678) == 0x02005678);

  // Runtime slots 1 and 2 deliberately differ from the consumer's local
  // light-master indices 0 and 1.
  REQUIRE(browser->ResolveReferencedFormId(consumer, 0xFE000801) ==
          0xFE001801);
  REQUIRE(browser->ResolveReferencedFormId(consumer, 0xFE001802) ==
          0xFE002802);
  REQUIRE(browser->ResolveReferencedFormId(consumer, 0x01001234) ==
          0x01001234);

  REQUIRE(browser->ToFileFormId(consumer, 0xFE001801) == 0xFE000801);
  REQUIRE(browser->ToFileFormId(consumer, 0xFE002802) == 0xFE001802);
  REQUIRE(browser->ToFileFormId(consumer, 0x01001234) == 0x01001234);
  REQUIRE(browser->ToFileFormId(l0, 0xFE001801) == 0xFE000801);
  REQUIRE(browser->ToFileFormId(l1, 0xFE002802) == 0xFE001802);

  REQUIRE_THROWS_AS(browser->ResolveReferencedFormId(consumer, 0xFE002001),
                    espm::ResolveError);
  REQUIRE(browser->ResolveReferencedFormId(consumer, 0xFF000123) ==
          0xFF000123);
}

TEST_CASE("Light overrides preserve the original runtime owner",
          "[combiner-light]")
{
  TestPlugin regular(0, {}, { 0x00000ABC });
  TestPlugin light(kLightFlag, { "Regular.esm" }, { 0xFE000801, 0x00000ABC });
  TestPlugin lightPatch(kLightFlag, { "Regular.esm", "Light.esl" },
                        { 0xFE000801 });

  espm::Combiner combiner;
  const auto regularSource =
    combiner.AddSource(&regular.browser, "Regular.esm");
  const auto lightSource = combiner.AddSource(&light.browser, "Light.esl");
  const auto patchSource =
    combiner.AddSource(&lightPatch.browser, "LightPatch.esp");
  auto browser = combiner.Combine();

  auto regularOverride = browser->LookupById(0x00000ABC);
  REQUIRE(regularOverride.rec);
  REQUIRE(regularOverride.GetSourceId() == lightSource);
  REQUIRE(regularOverride.ToGlobalId(regularOverride.rec->GetId()) ==
          0x00000ABC);

  auto lightOverride = browser->LookupById(0xFE000801);
  REQUIRE(lightOverride.rec);
  REQUIRE(lightOverride.GetSourceId() == patchSource);
  REQUIRE(lightOverride.ToGlobalId(lightOverride.rec->GetId()) == 0xFE000801);

  const auto allRegular = browser->LookupByIdAll(0x00000ABC);
  REQUIRE(allRegular.size() == 2);
  REQUIRE(allRegular.front().GetSourceId() == regularSource);
  REQUIRE(allRegular.back().GetSourceId() == lightSource);

  const auto distinct = browser->GetDistinctRecordsByType("KYWD");
  REQUIRE(distinct.size() == 2);
}

TEST_CASE("Combiner supports all 4096 light runtime slots", "[combiner-light]")
{
  TestPlugin light(kLightFlag, {});
  std::vector<std::string> names;
  names.reserve(4096);
  espm::Combiner combiner;
  for (size_t i = 0; i < 4096; ++i) {
    names.push_back("Light" + std::to_string(i) + ".esl");
    REQUIRE(combiner.AddSource(&light.browser, names.back().c_str()) == i);
  }

  TestPlugin consumer(0, names);
  const auto consumerSource =
    combiner.AddSource(&consumer.browser, "Consumer.esp");
  auto browser = combiner.Combine();

  REQUIRE(browser->GetSourceCount() == 4097);
  REQUIRE(browser->ToRuntimeFormId(0, 1) == 0xFE000001);
  REQUIRE(browser->ToRuntimeFormId(4095, 1) == 0xFEFFF001);
  REQUIRE(browser->ResolveReferencedFormId(consumerSource, 0xFEFFF001) ==
          0xFEFFF001);
  REQUIRE(browser->ToFileFormId(consumerSource, 0xFEFFF001) == 0xFEFFF001);
}

TEST_CASE("Combiner rejects a 4097th light runtime plugin", "[combiner-light]")
{
  TestPlugin light(kLightFlag, {});
  espm::Combiner combiner;
  for (size_t i = 0; i < 4097; ++i) {
    const auto name = "Light" + std::to_string(i) + ".esl";
    combiner.AddSource(&light.browser, name.c_str());
  }
  REQUIRE_THROWS_AS(combiner.Combine(), espm::Combiner::CombineError);
}

TEST_CASE("Loader metadata keeps PluginIndexMapping on runtime FormIDs",
          "[combiner-light]")
{
  const auto regular = MakePlugin(0, {});
  const auto lightEspfe =
    MakePlugin(kLightFlag, { "Regular.esm" }, { 0xFE000801 });
  const auto patch =
    MakePlugin(0, { "Regular.esm", "FlaggedLight.esp" }, { 0xFE000801 });

  TempPluginDirectory directory;
  std::vector<std::filesystem::path> paths{
    directory.Write("Regular.esm", regular),
    directory.Write("FlaggedLight.esp", lightEspfe),
    directory.Write("Patch.esp", patch),
  };

  espm::Loader loader(paths, nullptr,
                      espm::Loader::BufferType::AllocatedBuffer);
  PluginIndexMapping mapping;
  mapping.Build(loader.GetFileNames(), &loader);

  REQUIRE(mapping.GetPlugins().size() == 3);
  REQUIRE(mapping.GetPlugins()[1].kind == PluginKind::LightEspfe);
  REQUIRE(mapping.GetPlugins()[1].lightSlot == 0);
  REQUIRE(mapping.FormDescToRuntimeFormId(
            FormDesc(0x801, "FlaggedLight.esp")) == 0xFE000801);
  REQUIRE(mapping.RuntimeFormIdToFormDesc(0xFE000801) ==
          FormDesc(0x801, "FlaggedLight.esp"));
  REQUIRE(mapping.RuntimeFormIdToEspmId(0xFE000801) == 0xFE000801);
  REQUIRE(mapping.EspmIdToRuntimeFormId(0xFE000801) == 0xFE000801);
}

TEST_CASE("Combiner resolves real Bethesda plugins with interleaved ESL masters",
          "[combiner-light]")
{
  TestPlugin skyrim(0, {}, { 0x00000ABC });
  TestPlugin update(0, { "Skyrim.esm" }, { 0x01000123 });
  TestPlugin survival(kLightFlag, { "Skyrim.esm", "Update.esm" },
                      { 0x0200083F });
  TestPlugin advdsgs(0, { "Skyrim.esm", "Update.esm" }, { 0x02000456 });
  TestPlugin resourcePack(kLightFlag, { "Skyrim.esm", "Update.esm" },
                          { 0x02000022 });

  TestPlugin ussep(0,
                   { "Skyrim.esm", "Update.esm", "SurvivalMode.esl",
                     "AdvDSGS.esm", "ResourcePack.esl" },
                   { 0x05000806 });

  espm::Combiner combiner;
  const auto srcSkyrim = combiner.AddSource(&skyrim.browser, "Skyrim.esm");
  const auto srcUpdate = combiner.AddSource(&update.browser, "Update.esm");
  const auto srcSurvival =
    combiner.AddSource(&survival.browser, "SurvivalMode.esl");
  const auto srcAdvdsgs =
    combiner.AddSource(&advdsgs.browser, "AdvDSGS.esm");
  const auto srcResourcePack =
    combiner.AddSource(&resourcePack.browser, "ResourcePack.esl");
  const auto srcUssep = combiner.AddSource(&ussep.browser, "USSEP.esp");
  auto browser = combiner.Combine();

  REQUIRE(browser->ResolveRecordFormId(srcSurvival, 0x0200083F) == 0xFE00083F);
  REQUIRE(browser->ResolveReferencedFormId(srcSurvival, 0x0200083F) ==
          0xFE00083F);

  REQUIRE(browser->ResolveReferencedFormId(srcUssep, 0x00000ABC) == 0x00000ABC);
  REQUIRE(browser->ResolveReferencedFormId(srcUssep, 0x01000123) == 0x01000123);
  REQUIRE(browser->ResolveReferencedFormId(srcUssep, 0x0200083F) == 0xFE00083F);
  REQUIRE(browser->ResolveReferencedFormId(srcUssep, 0x03000456) == 0x02000456);
  REQUIRE(browser->ResolveReferencedFormId(srcUssep, 0x04000022) == 0xFE001022);
  REQUIRE(browser->ResolveReferencedFormId(srcUssep, 0x05000806) == 0x03000806);
  REQUIRE(browser->ResolveRecordFormId(srcUssep, 0x05000806) == 0x03000806);
}
