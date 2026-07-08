#pragma once

#include "qsh/qsh_types.hpp"
#include "qsh/qsh_reader.hpp"
#include <functional>

namespace qsh {

class AuxInfoReader {
public:
    bool next(QshFile& file, AuxInfoRecord& record);
    size_t scan_all(QshFile& file, const std::function<void(const AuxInfoRecord&)>& callback);

private:
    AuxInfoRecord prev_;
};

}  // namespace qsh
