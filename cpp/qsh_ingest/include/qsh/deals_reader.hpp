#pragma once

#include "qsh/qsh_types.hpp"
#include "qsh/qsh_reader.hpp"
#include <functional>

namespace qsh {

class DealsReader {
public:
    bool next(QshFile& file, DealRecord& record);
    size_t scan_all(QshFile& file, const std::function<void(const DealRecord&)>& callback);

private:
    DealRecord prev_;
};

}  // namespace qsh
