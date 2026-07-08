#define HAL_STORAGE_IMPL
#include "HalStorage.h"

#include <FS.h>  // need to be included before SdFat.h for compatibility with FS.h's File class
#include <Logging.h>
#if BOARD_NM_EPD_420
#include <SPI.h>
#include <SdFat.h>

#include "NmEpd420Pins.h"
#else
#include <SDCardManager.h>
#endif

#include <algorithm>
#include <cassert>

#if !BOARD_NM_EPD_420
#define SDCard SDCardManager::getInstance()
#endif

HalStorage HalStorage::instance;

#if BOARD_NM_EPD_420
namespace {
SPIClass nmSdSpi(HSPI);
SdFs nmSd;

String readAll(FsFile& file) {
  String content;
  while (file.available()) {
    content += static_cast<char>(file.read());
  }
  return content;
}
}  // namespace
#endif

HalStorage::HalStorage() {
  storageMutex = xSemaphoreCreateMutex();
  assert(storageMutex != nullptr);
}

// begin() and ready() are only called from setup, no need to acquire mutex for them

bool HalStorage::begin() {
#if BOARD_NM_EPD_420
  pinMode(NM_SD_CS, OUTPUT);
  digitalWrite(NM_SD_CS, HIGH);
  nmSdSpi.begin(NM_SD_SCK, NM_SD_MISO, NM_SD_MOSI, NM_SD_CS);
  initialized = nmSd.begin(SdSpiConfig(NM_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(20), &nmSdSpi));
  return initialized;
#else
  return SDCard.begin();
#endif
}

bool HalStorage::ready() const {
#if BOARD_NM_EPD_420
  return initialized;
#else
  return SDCard.ready();
#endif
}

// For the rest of the methods, we acquire the mutex to ensure thread safety

class HalStorage::StorageLock {
 public:
  StorageLock() { xSemaphoreTake(HalStorage::getInstance().storageMutex, portMAX_DELAY); }
  ~StorageLock() { xSemaphoreGive(HalStorage::getInstance().storageMutex); }
};

#define HAL_STORAGE_WRAPPED_CALL(method, ...) \
  HalStorage::StorageLock lock;               \
  return SDCard.method(__VA_ARGS__);

std::vector<String> HalStorage::listFiles(const char* path, int maxFiles) {
#if BOARD_NM_EPD_420
  StorageLock lock;
  std::vector<String> files;
  FsFile dir = nmSd.open(path);
  if (!dir || !dir.isDirectory()) {
    return files;
  }

  FsFile file;
  while (files.size() < static_cast<size_t>(maxFiles) && file.openNext(&dir, O_RDONLY)) {
    char name[128] = {};
    file.getName(name, sizeof(name));
    files.emplace_back(name);
    file.close();
  }
  dir.close();
  return files;
#else
  HAL_STORAGE_WRAPPED_CALL(listFiles, path, maxFiles);
#endif
}

String HalStorage::readFile(const char* path) {
#if BOARD_NM_EPD_420
  StorageLock lock;
  FsFile file = nmSd.open(path, O_RDONLY);
  if (!file) {
    return "";
  }
  String content = readAll(file);
  file.close();
  return content;
#else
  HAL_STORAGE_WRAPPED_CALL(readFile, path);
#endif
}

bool HalStorage::readFileToStream(const char* path, Print& out, size_t chunkSize) {
#if BOARD_NM_EPD_420
  StorageLock lock;
  FsFile file = nmSd.open(path, O_RDONLY);
  if (!file) {
    return false;
  }
  auto* buffer = static_cast<uint8_t*>(malloc(chunkSize));
  if (!buffer) {
    file.close();
    return false;
  }
  while (file.available()) {
    const int n = file.read(buffer, chunkSize);
    if (n <= 0 || out.write(buffer, static_cast<size_t>(n)) != static_cast<size_t>(n)) {
      free(buffer);
      file.close();
      return false;
    }
  }
  free(buffer);
  file.close();
  return true;
#else
  HAL_STORAGE_WRAPPED_CALL(readFileToStream, path, out, chunkSize);
#endif
}

size_t HalStorage::readFileToBuffer(const char* path, char* buffer, size_t bufferSize, size_t maxBytes) {
#if BOARD_NM_EPD_420
  StorageLock lock;
  if (!buffer || bufferSize == 0) {
    return 0;
  }
  FsFile file = nmSd.open(path, O_RDONLY);
  if (!file) {
    buffer[0] = '\0';
    return 0;
  }
  const size_t limit = maxBytes > 0 ? std::min(bufferSize - 1, maxBytes) : bufferSize - 1;
  const int bytesRead = file.read(buffer, limit);
  file.close();
  const size_t n = bytesRead > 0 ? static_cast<size_t>(bytesRead) : 0;
  buffer[n] = '\0';
  return n;
#else
  HAL_STORAGE_WRAPPED_CALL(readFileToBuffer, path, buffer, bufferSize, maxBytes);
#endif
}

bool HalStorage::writeFile(const char* path, const String& content) {
#if BOARD_NM_EPD_420
  StorageLock lock;
  FsFile file = nmSd.open(path, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file) {
    return false;
  }
  const bool ok = file.write(reinterpret_cast<const uint8_t*>(content.c_str()), content.length()) == content.length();
  file.close();
  return ok;
#else
  HAL_STORAGE_WRAPPED_CALL(writeFile, path, content);
#endif
}

bool HalStorage::ensureDirectoryExists(const char* path) {
#if BOARD_NM_EPD_420
  StorageLock lock;
  return nmSd.exists(path) || nmSd.mkdir(path);
#else
  HAL_STORAGE_WRAPPED_CALL(ensureDirectoryExists, path);
#endif
}

class HalFile::Impl {
 public:
  Impl(FsFile&& fsFile) : file(std::move(fsFile)) {}
  FsFile file;
};

HalFile::HalFile() = default;

HalFile::HalFile(std::unique_ptr<Impl> impl) : impl(std::move(impl)) {}

HalFile::~HalFile() = default;

HalFile::HalFile(HalFile&&) = default;

HalFile& HalFile::operator=(HalFile&&) = default;

HalFile HalStorage::open(const char* path, const oflag_t oflag) {
  StorageLock lock;  // ensure thread safety for the duration of this function
#if BOARD_NM_EPD_420
  return HalFile(std::make_unique<HalFile::Impl>(nmSd.open(path, oflag)));
#else
  return HalFile(std::make_unique<HalFile::Impl>(SDCard.open(path, oflag)));
#endif
}

bool HalStorage::mkdir(const char* path, const bool pFlag) {
#if BOARD_NM_EPD_420
  StorageLock lock;
  return pFlag ? nmSd.mkdir(path, true) : nmSd.mkdir(path);
#else
  HAL_STORAGE_WRAPPED_CALL(mkdir, path, pFlag);
#endif
}

bool HalStorage::exists(const char* path) {
#if BOARD_NM_EPD_420
  StorageLock lock;
  return nmSd.exists(path);
#else
  HAL_STORAGE_WRAPPED_CALL(exists, path);
#endif
}

bool HalStorage::remove(const char* path) {
#if BOARD_NM_EPD_420
  StorageLock lock;
  return nmSd.remove(path);
#else
  HAL_STORAGE_WRAPPED_CALL(remove, path);
#endif
}
bool HalStorage::rename(const char* oldPath, const char* newPath) {
#if BOARD_NM_EPD_420
  StorageLock lock;
  return nmSd.rename(oldPath, newPath);
#else
  HAL_STORAGE_WRAPPED_CALL(rename, oldPath, newPath);
#endif
}

bool HalStorage::rmdir(const char* path) {
#if BOARD_NM_EPD_420
  StorageLock lock;
  return nmSd.rmdir(path);
#else
  HAL_STORAGE_WRAPPED_CALL(rmdir, path);
#endif
}

bool HalStorage::openFileForRead(const char* moduleName, const char* path, HalFile& file) {
  StorageLock lock;  // ensure thread safety for the duration of this function
#if BOARD_NM_EPD_420
  (void)moduleName;
  FsFile fsFile = nmSd.open(path, O_RDONLY);
  bool ok = static_cast<bool>(fsFile);
#else
  FsFile fsFile;
  bool ok = SDCard.openFileForRead(moduleName, path, fsFile);
#endif
  file = HalFile(std::make_unique<HalFile::Impl>(std::move(fsFile)));
  return ok;
}

bool HalStorage::openFileForRead(const char* moduleName, const std::string& path, HalFile& file) {
  return openFileForRead(moduleName, path.c_str(), file);
}

bool HalStorage::openFileForRead(const char* moduleName, const String& path, HalFile& file) {
  return openFileForRead(moduleName, path.c_str(), file);
}

bool HalStorage::openFileForWrite(const char* moduleName, const char* path, HalFile& file) {
  StorageLock lock;  // ensure thread safety for the duration of this function
#if BOARD_NM_EPD_420
  (void)moduleName;
  FsFile fsFile = nmSd.open(path, O_WRONLY | O_CREAT | O_TRUNC);
  bool ok = static_cast<bool>(fsFile);
#else
  FsFile fsFile;
  bool ok = SDCard.openFileForWrite(moduleName, path, fsFile);
#endif
  file = HalFile(std::make_unique<HalFile::Impl>(std::move(fsFile)));
  return ok;
}

bool HalStorage::openFileForWrite(const char* moduleName, const std::string& path, HalFile& file) {
  return openFileForWrite(moduleName, path.c_str(), file);
}

bool HalStorage::openFileForWrite(const char* moduleName, const String& path, HalFile& file) {
  return openFileForWrite(moduleName, path.c_str(), file);
}

bool HalStorage::removeDir(const char* path) {
#if BOARD_NM_EPD_420
  StorageLock lock;
  return nmSd.rmdir(path);
#else
  HAL_STORAGE_WRAPPED_CALL(removeDir, path);
#endif
}

// HalFile implementation
// Allow doing file operations while ensuring thread safety via HalStorage's mutex.
// Please keep the list below in sync with the HalFile.h header

#define HAL_FILE_WRAPPED_CALL(method, ...) \
  HalStorage::StorageLock lock;            \
  assert(impl != nullptr);                 \
  return impl->file.method(__VA_ARGS__);

#define HAL_FILE_FORWARD_CALL(method, ...) \
  assert(impl != nullptr);                 \
  return impl->file.method(__VA_ARGS__);

void HalFile::flush() { HAL_FILE_WRAPPED_CALL(flush, ); }
size_t HalFile::getName(char* name, size_t len) { HAL_FILE_WRAPPED_CALL(getName, name, len); }
size_t HalFile::size() { HAL_FILE_FORWARD_CALL(size, ); }          // already thread-safe, no need to wrap
size_t HalFile::fileSize() { HAL_FILE_FORWARD_CALL(fileSize, ); }  // already thread-safe, no need to wrap
bool HalFile::seek(size_t pos) { HAL_FILE_WRAPPED_CALL(seekSet, pos); }
bool HalFile::seekCur(int64_t offset) { HAL_FILE_WRAPPED_CALL(seekCur, offset); }
bool HalFile::seekSet(size_t offset) { HAL_FILE_WRAPPED_CALL(seekSet, offset); }
int HalFile::available() const { HAL_FILE_WRAPPED_CALL(available, ); }
size_t HalFile::position() const { HAL_FILE_WRAPPED_CALL(position, ); }
int HalFile::read(void* buf, size_t count) { HAL_FILE_WRAPPED_CALL(read, buf, count); }
int HalFile::read() { HAL_FILE_WRAPPED_CALL(read, ); }
size_t HalFile::write(const void* buf, size_t count) { HAL_FILE_WRAPPED_CALL(write, buf, count); }
size_t HalFile::write(uint8_t b) { HAL_FILE_WRAPPED_CALL(write, b); }
bool HalFile::rename(const char* newPath) { HAL_FILE_WRAPPED_CALL(rename, newPath); }
bool HalFile::isDirectory() const { HAL_FILE_FORWARD_CALL(isDirectory, ); }  // already thread-safe, no need to wrap
void HalFile::rewindDirectory() { HAL_FILE_WRAPPED_CALL(rewindDirectory, ); }
bool HalFile::close() { HAL_FILE_WRAPPED_CALL(close, ); }
HalFile HalFile::openNextFile() {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
  return HalFile(std::make_unique<Impl>(impl->file.openNextFile()));
}
bool HalFile::isOpen() const { return impl != nullptr && impl->file.isOpen(); }  // already thread-safe, no need to wrap
HalFile::operator bool() const { return isOpen(); }
