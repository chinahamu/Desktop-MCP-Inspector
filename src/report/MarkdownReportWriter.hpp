#pragma once

#include "ReportModel.hpp"

#include <QString>

namespace report {

class MarkdownReportWriter final
{
public:
  [[nodiscard]] QString write(const ReportModel& model) const;
};

} // namespace report
