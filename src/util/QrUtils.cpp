#include "QrUtils.h"

#include <Utf8.h>
#include <qrcode.h>

#include <algorithm>

#include "Logging.h"
#include "Memory.h"

int QrUtils::versionForPayloadSize(const size_t len) {
  // Byte-mode capacities at ECC_LOW for QR versions 1..40.
  static constexpr uint16_t kByteCapacityLow[40] = {
      17,   32,   53,   78,   106,  134,  154,  192,  230,  271,   // 1-10
      321,  367,  425,  458,  520,  586,  644,  718,  792,  858,   // 11-20
      929,  1003, 1091, 1171, 1273, 1367, 1465, 1528, 1628, 1732,  // 21-30
      1840, 1952, 2068, 2188, 2303, 2431, 2563, 2699, 2809, 2953,  // 31-40
  };
  for (uint8_t version = 1; version <= 40; version++) {
    if (len <= kByteCapacityLow[version - 1]) return version;
  }
  return 0;
}

void QrUtils::drawQrCode(const GfxRenderer& renderer, const Rect& bounds, const std::string& textPayload) {
  // Pick the smallest QR version that actually fits the payload; the ricmoo
  // qrcode library does not check overflow and silently emits a broken matrix
  // when the data exceeds the version capacity.
  size_t len = textPayload.length();

  // Truncate to max QR capacity at a UTF-8 safe boundary to avoid splitting multi-byte sequences
  static constexpr size_t MAX_QR_CAPACITY = 2953;  // Version 40, ECC_LOW, byte mode
  std::string truncated;
  const char* payload = textPayload.c_str();
  if (len > MAX_QR_CAPACITY) {
    len = utf8SafeTruncateBuffer(textPayload.c_str(), static_cast<int>(MAX_QR_CAPACITY));
    truncated = textPayload.substr(0, len);
    payload = truncated.c_str();
  }

  const int version = versionForPayloadSize(len);
  if (version == 0) {
    LOG_ERR("QR", "Payload too large for QR Code (%zu bytes)", len);
    return;
  }

  // Make sure we have a large enough buffer on the heap to avoid blowing the stack
  uint32_t bufferSize = qrcode_getBufferSize(version);
  auto qrcodeBytes = makeUniqueNoThrow<uint8_t[]>(bufferSize);
  if (!qrcodeBytes) {
    LOG_ERR("QR", "OOM: QR buffer (%u bytes)", static_cast<unsigned>(bufferSize));
    return;
  }

  QRCode qrcode;
  // Initialize the QR code. We use ECC_LOW for max capacity.
  int8_t res = qrcode_initText(&qrcode, qrcodeBytes.get(), version, ECC_LOW, payload);

  if (res == 0) {
    // Determine the optimal pixel size.
    const int maxDim = std::min(bounds.width, bounds.height);

    int px = maxDim / qrcode.size;
    if (px < 1) px = 1;

    // Calculate centering X and Y
    const int qrDisplaySize = qrcode.size * px;
    const int xOff = bounds.x + (bounds.width - qrDisplaySize) / 2;
    const int yOff = bounds.y + (bounds.height - qrDisplaySize) / 2;

    // Draw the QR Code
    for (uint8_t cy = 0; cy < qrcode.size; cy++) {
      for (uint8_t cx = 0; cx < qrcode.size; cx++) {
        if (qrcode_getModule(&qrcode, cx, cy)) {
          renderer.fillRect(xOff + px * cx, yOff + px * cy, px, px, true);
        }
      }
    }
  } else {
    // If it fails (e.g. text too large), log an error
    LOG_ERR("QR", "Text too large for QR Code version %d", version);
  }
}
