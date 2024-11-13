#include "shell_commands.h"
#include <stdio.h>


void command_echo(Console &c) {
    printf("%s\r\n", c.packet.take_rest_string());
}
