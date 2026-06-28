#include "StdioLaunchSpec.hpp"

#include "ServerProfile.hpp"

namespace transport {

StdioLaunchSpec make_stdio_launch_spec(const config::ServerProfile& profile)
{
  return StdioLaunchSpec{
      .program = profile.command,
      .arguments = profile.args,
      .working_directory = profile.cwd,
      .environment = profile.env,
      .timeout_ms = profile.timeout_ms,
  };
}

} // namespace transport
