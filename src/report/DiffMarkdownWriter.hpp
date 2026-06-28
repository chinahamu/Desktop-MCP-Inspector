#pragma once

#include "../compare/ServerDiff.hpp"

#include <QString>

namespace report {

class DiffMarkdownWriter final
{
public:
  [[nodiscard]] QString write(const compare::ServerDiffResult& diff) const;
};

} // namespace report
