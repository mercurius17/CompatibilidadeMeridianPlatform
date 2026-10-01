#include "PluginIndexMapping.h"
#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>

TEST_CASE("ToString/FromString", "[FormDesc]")
{
  REQUIRE(FormDesc(0xAAA, "").ToString() == "aaa");
  REQUIRE(FormDesc(0xAAA, "Skyrim.esm").ToString() == "aaa:Skyrim.esm");

  auto x = FormDesc::FromString("aaa");
  REQUIRE(x.file == "");
  REQUIRE(x.shortFormId == 0xAAA);

  auto v = FormDesc::FromString("aaa:Skyrim.esm");
  REQUIRE(v.file == "Skyrim.esm");
  REQUIRE(v.shortFormId == 0xAAA);
}

TEST_CASE("ToFormId/FromFormId", "[FormDesc]")
{
  PluginIndexMapping mapping;
  mapping.Build({ "Skyrim.esm", "Update.esm" });

  REQUIRE(FormDesc::FromFormId(0x01000001, mapping) ==
          FormDesc(0x1, "Update.esm"));
  REQUIRE(FormDesc::FromFormId(0x00000001, mapping) ==
          FormDesc(0x1, "Skyrim.esm"));
  REQUIRE(FormDesc::FromFormId(0xff000bbb, mapping) == FormDesc(0xbbb, ""));

  REQUIRE(FormDesc(0x1, "Update.esm").ToFormId(mapping) == 0x01000001);
  REQUIRE(FormDesc(0x1, "Skyrim.esm").ToFormId(mapping) == 0x00000001);
  REQUIRE(FormDesc(0x1, "").ToFormId(mapping) == 0xff000001);

  PluginIndexMapping mappingWithEsl;
  mappingWithEsl.Build(
    { "Skyrim.esm", "Update.esm", "fishing.esl", "rare_curios.esl" });
  REQUIRE(FormDesc::FromFormId(0xFE000800, mappingWithEsl) ==
          FormDesc(0x800, "fishing.esl"));
  REQUIRE(FormDesc::FromFormId(0xFE001001, mappingWithEsl) ==
          FormDesc(0x1, "rare_curios.esl"));
  REQUIRE(FormDesc(0x800, "fishing.esl").ToFormId(mappingWithEsl) ==
          0xFE000800);
  REQUIRE(FormDesc(0x1, "rare_curios.esl").ToFormId(mappingWithEsl) ==
          0xFE001001);
}
