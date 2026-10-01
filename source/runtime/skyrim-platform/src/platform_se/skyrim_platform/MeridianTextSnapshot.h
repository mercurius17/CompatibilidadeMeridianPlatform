#pragma once
#include <ui/TextToDraw.h>
namespace MeridianTextSnapshot {
// Publish on Skyrim Platform's serialized JS worker; consume on the renderer.
void Publish();
void Read(TextToDrawCallback callback);
}
