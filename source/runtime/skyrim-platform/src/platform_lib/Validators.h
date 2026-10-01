#pragma once

#include <string_view>

bool ValidateFilename(std::string_view filename, bool allowDots);

bool ValidateRelativePath(std::string_view path);

// Real Bethesda plugin filenames commonly contain characters ValidateFilename
// rejects (spaces, apostrophes, ampersands, brackets, ...). This instead
// denies only what's actually dangerous for a single filename appended to a
// fixed base path: path separators/traversal, Windows-reserved characters,
// and control characters.
bool ValidatePluginFilename(std::string_view filename);
