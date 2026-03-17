#include "ZipArchive.h"

#include <memory>

namespace CesiumIo {

void ZipArchiveDeleter::operator()(zip_t* archive) const noexcept {
  if (archive) {
    zip_close(archive);
  }
}

// Explicit template instantiations for the two element types used in the
// codebase.  The template definition lives in the header so that other
// element types can be added without touching this file.
template bool
readZipEntry<uint8_t>(zip_t*, const std::string&, std::vector<uint8_t>&);
template bool
readZipEntry<std::byte>(zip_t*, const std::string&, std::vector<std::byte>&);

} // namespace CesiumIo
