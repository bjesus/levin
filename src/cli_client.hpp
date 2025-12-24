#pragma once

#include <string>

namespace levin {
namespace cli {

/**
 * Run CLI client command
 * @param argc Argument count
 * @param argv Argument values (starting from the command, not program name)
 * @return Exit code
 */
int run_client(int argc, char** argv);

} // namespace cli
} // namespace levin
