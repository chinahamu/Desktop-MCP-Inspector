#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

void drain_stdin()
{
  std::string line;
  while (std::getline(std::cin, line)) {
  }
}

int echo_once()
{
  std::cerr << "mock stderr ready" << std::endl;

  std::string line;
  if (!std::getline(std::cin, line)) {
    return 2;
  }

  std::cout << R"({"jsonrpc":"2.0","id":1,"result":{"mode":"echo-once"}})" << std::endl;
  drain_stdin();
  return 0;
}

int protocol_violation()
{
  std::cout << R"({"jsonrpc":"1.0","id":1,"result":true})" << std::endl;
  drain_stdin();
  return 0;
}

int stderr_tail_and_exit()
{
  std::cerr << "unterminated stderr tail" << std::flush;
  std::this_thread::sleep_for(std::chrono::milliseconds(25));
  return 7;
}

} // namespace

int main(int argc, char* argv[])
{
  const std::string mode = argc >= 2 ? argv[1] : "";
  if (mode == "--echo-once") {
    return echo_once();
  }

  if (mode == "--protocol-violation") {
    return protocol_violation();
  }

  if (mode == "--stderr-tail-and-exit") {
    return stderr_tail_and_exit();
  }

  std::cerr << "unknown stdio mock mode: " << mode << std::endl;
  return 64;
}
