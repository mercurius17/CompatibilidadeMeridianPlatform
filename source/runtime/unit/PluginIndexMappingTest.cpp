#include "PluginIndexMapping.h"
#include "FormDesc.h"
#include <catch2/catch_all.hpp>

TEST_CASE("PluginIndexMapping - DetectPluginKind", "[PluginIndexMapping]")
{
  using PK = PluginKind;

  REQUIRE(PluginIndexMapping::DetectPluginKind("Skyrim.esm", 1) == PK::Master);
  REQUIRE(PluginIndexMapping::DetectPluginKind("Update.esm", 0) == PK::Master);
  REQUIRE(PluginIndexMapping::DetectPluginKind("mod.esp", 0) ==
          PK::RegularEsp);
  REQUIRE(PluginIndexMapping::DetectPluginKind("mod_flagged_esm.esp", 1) ==
          PK::Master);
  REQUIRE(PluginIndexMapping::DetectPluginKind("fish.esl", 0) == PK::LightEsl);
  REQUIRE(PluginIndexMapping::DetectPluginKind("fish.esl", 0x200) ==
          PK::LightEsl);
  REQUIRE(PluginIndexMapping::DetectPluginKind("tweaks.esp", 0x200) ==
          PK::LightEspfe);
  REQUIRE(PluginIndexMapping::DetectPluginKind("TWEAKS.ESP", 0x200) ==
          PK::LightEspfe);
}

TEST_CASE("PluginIndexMapping - Invariants & FailFast", "[PluginIndexMapping]")
{
  // Invariant 1: Duplicate plugin names must throw
  {
    PluginIndexMapping mapping;
    std::vector<std::string> duplicates = { "Skyrim.esm", "mod.esp",
                                            "mod.esp" };
    REQUIRE_THROWS_WITH(mapping.Build(duplicates),
                        Catch::Matchers::ContainsSubstring(
                          "Duplicate plugin filename 'mod.esp'"));
  }

  // Invariant 2: Slot separation & sequence
  {
    PluginIndexMapping mapping;
    std::vector<std::string> files = { "Skyrim.esm", "Update.esm",
                                       "lightA.esl", "modA.esp",
                                       "lightB.esl", "modB.esp" };
    REQUIRE_NOTHROW(mapping.Build(files));

    const auto& plugins = mapping.GetPlugins();
    REQUIRE(plugins.size() == 6);

    // Skyrim.esm (regular slot 0, espm index 0)
    REQUIRE(!plugins[0].isLight);
    REQUIRE(plugins[0].regularSlot == 0);
    REQUIRE(!plugins[0].lightSlot.has_value());
    REQUIRE(plugins[0].espmFileIndex == 0);

    // Update.esm (regular slot 1, espm index 1)
    REQUIRE(!plugins[1].isLight);
    REQUIRE(plugins[1].regularSlot == 1);
    REQUIRE(!plugins[1].lightSlot.has_value());
    REQUIRE(plugins[1].espmFileIndex == 1);

    // lightA.esl (light slot 0, espm index 2)
    REQUIRE(plugins[2].isLight);
    REQUIRE(!plugins[2].regularSlot.has_value());
    REQUIRE(plugins[2].lightSlot == 0);
    REQUIRE(plugins[2].espmFileIndex == 2);

    // modA.esp (regular slot 2, espm index 3)
    REQUIRE(!plugins[3].isLight);
    REQUIRE(plugins[3].regularSlot == 2);
    REQUIRE(!plugins[3].lightSlot.has_value());
    REQUIRE(plugins[3].espmFileIndex == 3);

    // lightB.esl (light slot 1, espm index 4)
    REQUIRE(plugins[4].isLight);
    REQUIRE(!plugins[4].regularSlot.has_value());
    REQUIRE(plugins[4].lightSlot == 1);
    REQUIRE(plugins[4].espmFileIndex == 4);

    // modB.esp (regular slot 3, espm index 5)
    REQUIRE(!plugins[5].isLight);
    REQUIRE(plugins[5].regularSlot == 3);
    REQUIRE(!plugins[5].lightSlot.has_value());
    REQUIRE(plugins[5].espmFileIndex == 5);
  }

  SECTION("Plugin filenames use Skyrim's case-insensitive semantics")
  {
    PluginIndexMapping mapping;
    mapping.Build({ "Skyrim.esm", "Light.esl" });
    REQUIRE(mapping.FormDescToRuntimeFormId(FormDesc(0x123, "LIGHT.ESL")) ==
            0xFE000123);
    REQUIRE(mapping.GetEspmFileIndexByFilename("data/light.ESL") == 1);
    REQUIRE_THROWS(mapping.Build({ "mod.esp", "MOD.ESP" }));
  }
}

TEST_CASE("PluginIndexMapping - BitPacking 0xFE & FormID Translation",
          "[PluginIndexMapping]")
{
  PluginIndexMapping mapping;
  std::vector<std::string> files = {
    "Skyrim.esm", // reg 0, espm 0
    "Update.esm", // reg 1, espm 1
    "mod1.esp",   // reg 2, espm 2
    "light1.esl", // light 0, espm 3
    "light2.esl", // light 1, espm 4
    "mod2.esp"    // reg 3, espm 5
  };
  mapping.Build(files);

  SECTION("RuntimeFormIdToEspmId")
  {
    // Light mod 0 (light1.esl at espm 3): 0xFE:000:800 -> 0x03000800
    REQUIRE(mapping.RuntimeFormIdToEspmId(0xFE000800) == 0x03000800);
    REQUIRE(mapping.RuntimeFormIdToEspmId(0xFE000001) == 0x03000001);

    // Light mod 1 (light2.esl at espm 4): 0xFE:001:001 -> 0x04000001
    REQUIRE(mapping.RuntimeFormIdToEspmId(0xFE001001) == 0x04000001);
    REQUIRE(mapping.RuntimeFormIdToEspmId(0xFE001FFF) == 0x04000FFF);

    // Regular mod 2 (mod1.esp at espm 2): 0x02:001234 -> 0x02001234
    REQUIRE(mapping.RuntimeFormIdToEspmId(0x02001234) == 0x02001234);

    // Regular mod 3 (mod2.esp at espm 5): 0x03:001234 -> 0x05001234
    REQUIRE(mapping.RuntimeFormIdToEspmId(0x03001234) == 0x05001234);

    // Dynamic FormID
    REQUIRE(mapping.RuntimeFormIdToEspmId(0xFF000005) == 0xFF000005);

    // Unmapped light slot throws
    REQUIRE_THROWS(mapping.RuntimeFormIdToEspmId(0xFE002001));

    // Unmapped regular slot throws
    REQUIRE_THROWS(mapping.RuntimeFormIdToEspmId(0x04000001));
  }

  SECTION("EspmIdToRuntimeFormId")
  {
    // espm 3 (light1.esl) -> 0xFE:000:xxx
    REQUIRE(mapping.EspmIdToRuntimeFormId(0x03000800) == 0xFE000800);
    REQUIRE(mapping.EspmIdToRuntimeFormId(0x03000001) == 0xFE000001);

    // espm 4 (light2.esl) -> 0xFE:001:xxx
    REQUIRE(mapping.EspmIdToRuntimeFormId(0x04000001) == 0xFE001001);

    // Malformed light plugins must not silently truncate local IDs.
    REQUIRE(!mapping.TryEspmIdToRuntimeFormId(0x04001001).has_value());
    REQUIRE_THROWS(mapping.EspmIdToFormDesc(0x04001001));

    // espm 5 (mod2.esp) -> regular slot 3
    REQUIRE(mapping.EspmIdToRuntimeFormId(0x05001234) == 0x03001234);

    // Dynamic FormID
    REQUIRE(mapping.EspmIdToRuntimeFormId(0xFF000005) == 0xFF000005);

    // Unknown espm index throws
    REQUIRE_THROWS(mapping.EspmIdToRuntimeFormId(0x06000001));
  }

  SECTION("FormDesc Conversions")
  {
    // Light mod 0: FormDesc(0x800, "light1.esl") <-> 0xFE000800
    REQUIRE(mapping.FormDescToRuntimeFormId(FormDesc(0x800, "light1.esl")) ==
            0xFE000800);
    REQUIRE(mapping.RuntimeFormIdToFormDesc(0xFE000800) ==
            FormDesc(0x800, "light1.esl"));

    // Light mod 1: FormDesc(0x1, "light2.esl") <-> 0xFE001001
    REQUIRE(mapping.FormDescToRuntimeFormId(FormDesc(0x1, "light2.esl")) ==
            0xFE001001);
    REQUIRE(mapping.RuntimeFormIdToFormDesc(0xFE001001) ==
            FormDesc(0x1, "light2.esl"));
    REQUIRE_THROWS(
      mapping.FormDescToRuntimeFormId(FormDesc(0x1000, "light2.esl")));
    REQUIRE_THROWS(mapping.FormDescToEspmId(FormDesc(0x1000, "light2.esl")));

    // Regular mod: FormDesc(0x1234, "mod2.esp") <-> 0x03001234
    REQUIRE(mapping.FormDescToRuntimeFormId(FormDesc(0x1234, "mod2.esp")) ==
            0x03001234);
    REQUIRE(mapping.RuntimeFormIdToFormDesc(0x03001234) ==
            FormDesc(0x1234, "mod2.esp"));

    // Tamriel
    REQUIRE(mapping.FormDescToRuntimeFormId(FormDesc(0x3C, "Skyrim.esm")) ==
            0x3C);
    REQUIRE(mapping.RuntimeFormIdToFormDesc(0x3C) == FormDesc::Tamriel());

    // Dynamic
    REQUIRE(mapping.FormDescToRuntimeFormId(FormDesc(0x5, "")) == 0xFF000005);
    REQUIRE(mapping.RuntimeFormIdToFormDesc(0xFF000005) == FormDesc(0x5, ""));

    // EspmId <-> FormDesc
    REQUIRE(mapping.EspmIdToFormDesc(0x03000800) ==
            FormDesc(0x800, "light1.esl"));
    REQUIRE(mapping.FormDescToEspmId(FormDesc(0x800, "light1.esl")) ==
            0x03000800);
    REQUIRE(mapping.EspmIdToFormDesc(0x05001234) ==
            FormDesc(0x1234, "mod2.esp"));
    REQUIRE(mapping.FormDescToEspmId(FormDesc(0x1234, "mod2.esp")) ==
            0x05001234);
  }
}

TEST_CASE("PluginIndexMapping - GetEspmFileIndex", "[PluginIndexMapping]")
{
  PluginIndexMapping mapping;
  std::vector<std::string> files = {
    "Skyrim.esm", // 0
    "Update.esm", // 1
    "fish.esl",   // 2 (light 0)
    "mod.esp"     // 3 (reg 2)
  };
  mapping.Build(files);

  // Regular forms
  REQUIRE(mapping.GetEspmFileIndex(0x00000001) == 0);
  REQUIRE(mapping.GetEspmFileIndex(0x01000001) == 1);
  REQUIRE(mapping.GetEspmFileIndex(0x02000001) == 3);

  // Light forms
  REQUIRE(mapping.GetEspmFileIndex(0xFE000800) == 2);
  REQUIRE(mapping.GetEspmFileIndex(0xFE000001) == 2);

  // Sentinel guarantees: Never returns 0xFE as sentinel!
  REQUIRE(mapping.GetEspmFileIndex(0xFF000001) ==
          PluginIndexMapping::kInvalidFileIndex);
  REQUIRE(mapping.GetEspmFileIndex(0x03000001) ==
          PluginIndexMapping::kInvalidFileIndex);
  REQUIRE(mapping.GetEspmFileIndex(0xFE001001) ==
          PluginIndexMapping::kInvalidFileIndex);
  REQUIRE(mapping.GetEspmFileIndex(0xFE000001) != 0xFE);

  // By filename
  REQUIRE(mapping.GetEspmFileIndexByFilename("fish.esl") == 2);
  REQUIRE(mapping.GetEspmFileIndexByFilename("Data/fish.esl") == 2);
  REQUIRE(mapping.GetEspmFileIndexByFilename("nonexistent.esp") ==
          PluginIndexMapping::kInvalidFileIndex);
}

TEST_CASE("PluginIndexMapping - Try Methods & RoundTrip",
          "[PluginIndexMapping]")
{
  PluginIndexMapping mapping;
  std::vector<std::string> files = { "Skyrim.esm", "modA.esp", "lightA.esl",
                                     "lightB.esl" };
  mapping.Build(files);

  // Try methods
  REQUIRE(mapping.TryRuntimeFormIdToEspmId(0xFE000800).has_value());
  REQUIRE(*mapping.TryRuntimeFormIdToEspmId(0xFE000800) == 0x02000800);
  REQUIRE(!mapping.TryRuntimeFormIdToEspmId(0xFE005000).has_value());
  REQUIRE(!mapping.TryRuntimeFormIdToEspmId(0x05000001).has_value());

  REQUIRE(mapping.TryEspmIdToRuntimeFormId(0x02000800).has_value());
  REQUIRE(*mapping.TryEspmIdToRuntimeFormId(0x02000800) == 0xFE000800);
  REQUIRE(!mapping.TryEspmIdToRuntimeFormId(0x10000000).has_value());

  // Round trip
  std::vector<uint32_t> testRuntimeIds = {
    0x00012345, // Skyrim.esm
    0x01ABCDEF, // modA.esp
    0xFE000001, // lightA.esl local 1
    0xFE000800, // lightA.esl local 0x800
    0xFE001042, // lightB.esl local 0x42
    0xFF000001  // dynamic
  };

  for (auto id : testRuntimeIds) {
    uint32_t espmId = mapping.RuntimeFormIdToEspmId(id);
    uint32_t backRuntime = mapping.EspmIdToRuntimeFormId(espmId);
    REQUIRE(backRuntime == id);

    FormDesc desc = mapping.RuntimeFormIdToFormDesc(id);
    uint32_t backFromDesc = mapping.FormDescToRuntimeFormId(desc);
    REQUIRE(backFromDesc == id);
  }
}

TEST_CASE("PluginIndexMapping - Supports more than 255 mixed sources",
          "[PluginIndexMapping]")
{
  std::vector<std::string> files;
  files.reserve(255);
  for (size_t i = 0; i < 254; ++i) {
    files.push_back("regular" + std::to_string(i) + ".esp");
  }
  files.push_back("light0.esl");

  PluginIndexMapping mapping;
  REQUIRE_NOTHROW(mapping.Build(files));
  REQUIRE(mapping.GetPlugins().size() == 255);
  REQUIRE(mapping.GetPlugins().front().regularSlot == 0);
  REQUIRE(mapping.GetPlugins()[253].regularSlot == 253);
  REQUIRE(mapping.GetPlugins().back().lightSlot == 0);
}

TEST_CASE("PluginIndexMapping - Supports the complete light slot space",
          "[PluginIndexMapping]")
{
  std::vector<std::string> files;
  files.reserve(4097);
  files.push_back("Skyrim.esm");
  for (size_t i = 0; i < 4096; ++i) {
    files.push_back("light" + std::to_string(i) + ".esl");
  }

  PluginIndexMapping mapping;
  REQUIRE_NOTHROW(mapping.Build(files));
  REQUIRE(mapping.GetPlugins().size() == 4097);
  REQUIRE(mapping.GetPlugins()[1].lightSlot == 0);
  REQUIRE(mapping.GetPlugins().back().lightSlot == 0xFFF);
  REQUIRE(mapping.GetEspmFileIndexByFilename("light4095.esl") == 4096);
  // The legacy physical-ID facade must fail explicitly instead of truncating
  // a source index that no longer fits in its byte-oriented representation.
  REQUIRE(!mapping.TryRuntimeFormIdToEspmId(0xFEFFF001).has_value());
}
