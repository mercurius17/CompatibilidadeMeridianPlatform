#include "Validators.h"

bool ValidateFilename(std::string_view filename, bool allowDots)
{
  for (char c : filename) {
    if (!(('0' <= c && c <= '9') || ('A' <= c && c <= 'Z') ||
          ('a' <= c && c <= 'z') || (c == '.' && allowDots) || c == '-' ||
          c == '_')) {
      return false;
    }
  }
  return true;
}

bool ValidateRelativePath(std::string_view path)
{
  for (size_t i = 0; i < path.size(); ++i) {
    // Forbid everything including ':' and null character
    const char& c = path[i];
    if (!(('0' <= c && c <= '9') || ('A' <= c && c <= 'Z') ||
          ('a' <= c && c <= 'z') || c == '.' || c == '-' || c == '_' ||
          c == '/' || c == '\\')) {
      return false;
    }
    if (i > 0 && path[i - 1] == '.' && path[i] == '.') {
      return false;
    }
  }
  return true;
}

bool ValidatePluginFilename(std::string_view filename)
{
  if (filename.empty()) {
    return false;
  }
  for (size_t i = 0; i < filename.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(filename[i]);
    // Control characters, and the characters Windows itself forbids in a
    // filename (path separators included, since this must stay a single
    // path segment, not a path).
    if (c < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' ||
        c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
      return false;
    }
    if (i > 0 && filename[i - 1] == '.' && filename[i] == '.') {
      return false; // no ".." even without a separator around it
    }
  }
  return true;
}
