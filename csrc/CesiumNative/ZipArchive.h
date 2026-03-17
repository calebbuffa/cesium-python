#pragma once

#include <zip.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace CesiumIo {

/**
 * @brief Custom deleter for zip_t archives from libzip
 */
struct ZipArchiveDeleter {
  void operator()(zip_t* archive) const noexcept;
};

/**
 * @brief Read a single entry from a zip archive into a contiguous vector.
 *
 * Works with any element type whose size matches a byte (uint8_t, std::byte,
 * char, etc.).
 *
 * @tparam T Element type (must be 1 byte; static_assert enforced).
 * @param archive Open zip archive handle.
 * @param entry   Path to entry within archive.
 * @param data    Output buffer; resized to the entry's uncompressed size.
 * @return true if read successfully, false otherwise.
 */
template <typename T>
bool readZipEntry(
    zip_t* archive,
    const std::string& entry,
    std::vector<T>& data) {
  static_assert(
      sizeof(T) == 1,
      "readZipEntry only supports byte-sized element types");

  if (!archive) {
    return false;
  }

  zip_stat_t st;
  zip_stat_init(&st);
  if (zip_stat(archive, entry.c_str(), 0, &st) != 0) {
    return false;
  }

  std::unique_ptr<zip_file_t, decltype(&zip_fclose)> file(
      zip_fopen(archive, entry.c_str(), 0),
      &zip_fclose);
  if (!file) {
    return false;
  }

  data.resize(static_cast<size_t>(st.size));
  zip_int64_t bytesRead = zip_fread(file.get(), data.data(), st.size);

  return bytesRead >= 0 && static_cast<zip_uint64_t>(bytesRead) == st.size;
}

} // namespace CesiumIo
