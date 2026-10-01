#include "FormDesc.h"
#include "PluginIndexMapping.h"
#include <cstdio>

std::string FormDesc::ToString(char delimiter) const
{
  auto fullFmt = "%0x%c%s";
  auto idFmt = "%0x";
  size_t size = !file.empty()
    ? std::snprintf(nullptr, 0, fullFmt, shortFormId, delimiter, file.c_str())
    : std::snprintf(nullptr, 0, idFmt, shortFormId);

  std::string buffer;
  buffer.resize(size + 1);

  if (!file.empty()) {
    std::sprintf(buffer.data(), fullFmt, shortFormId, delimiter, file.c_str());
  } else {
    std::sprintf(buffer.data(), idFmt, shortFormId);
  }
  buffer.resize(size); // remove extra null terminator
  return buffer;
}

FormDesc FormDesc::FromString(const std::string& str, char delimiter)
{
  FormDesc res;
  std::string id, file;

  if (str.find(delimiter) == std::string::npos) {
    std::sscanf(str.data(), "%x", &res.shortFormId);
    return res;
  }

  for (auto it = str.begin(); it != str.end(); ++it) {
    if (*it == delimiter) {
      id = { str.begin(), it };
      res.file = { it + 1, str.end() };
      break;
    }
  }

  std::sscanf(id.data(), "%x", &res.shortFormId);
  return res;
}

uint32_t FormDesc::ToFormId(const PluginIndexMapping& mapping) const
{
  return mapping.FormDescToRuntimeFormId(*this);
}

FormDesc FormDesc::FromFormId(uint32_t formId, const PluginIndexMapping& mapping)
{
  return mapping.RuntimeFormIdToFormDesc(formId);
}

static const FormDesc kTamriel = FormDesc::FromString("3c:Skyrim.esm");

FormDesc FormDesc::Tamriel()
{
  return kTamriel;
}
