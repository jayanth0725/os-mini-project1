#ifndef EXECUTE
#define EXECUTE

#include "parser.h"

// Overall execute command that handles builtin commands, external commands, redirection and piping.
int execute_command_group(Token *tokens, const char *shell_home);

#endif