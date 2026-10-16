#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

namespace opena8dj {

inline std::filesystem::path repo_root() {
#ifdef OPENA8DJ_SOURCE_ROOT
  const auto root = std::filesystem::weakly_canonical(OPENA8DJ_SOURCE_ROOT);
  if (std::filesystem::is_regular_file(root / "CMakeLists.txt")) {
    return root;
  }
  throw std::runtime_error("configured OpenA8DJ source root has no CMakeLists.txt: " + root.string());
#else
  throw std::runtime_error("repo_root requires CMake definition OPENA8DJ_SOURCE_ROOT");
#endif
}

}  // namespace opena8dj
