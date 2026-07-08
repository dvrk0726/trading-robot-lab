#pragma once

#include "qsh/qsh_types.hpp"
#include "qsh/qsh_reader.hpp"
#include <functional>
#include <map>

namespace qsh {

class QuotesReader {
public:
    bool next(QshFile& file, QuotesRecord& record);
    size_t scan_all(QshFile& file, const std::function<void(const QuotesRecord&)>& callback);

private:
    std::map<Price, Volume> book_;
    Price key_ = 0;
};

}  // namespace qsh
