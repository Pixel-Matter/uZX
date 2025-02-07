# pragma once

#include <string>

namespace Util {

const std::string b64decode(const void* data, const size_t &len);

std::string b64decode(const std::string& str64);

}  // namespace Util
