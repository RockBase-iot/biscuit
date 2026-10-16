#pragma once

#include <GfxRenderer.h>

#include <string>

#include "components/themes/BaseTheme.h"

namespace QrUtils {

// Returns the smallest QR version (1..40) that fits len bytes in byte mode at
// ECC_LOW, or 0 when the payload exceeds the version-40 capacity. The ricmoo
// qrcode library does not validate overflow, so callers MUST use this instead
// of guessing a version — a too-small version silently yields an unscannable
// code.
int versionForPayloadSize(size_t len);

// Renders a QR code with the given text payload within the specified bounding box.
void drawQrCode(const GfxRenderer& renderer, const Rect& bounds, const std::string& textPayload);

}  // namespace QrUtils
