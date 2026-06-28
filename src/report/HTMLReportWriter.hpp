#pragma once

#include "ReportModel.hpp"

#include <QString>

namespace report {

class HTMLReportWriter final
{
public:
  [[nodiscard]] QString write(const ReportModel& model) const;
};

} // namespace report
