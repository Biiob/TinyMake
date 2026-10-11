#include <string.h>

#include "tinymake_cli_parser.h"
#include "tinymake_log.h"
#include "tinymake_types.h"

int parse_arguments(int argc, char* argv[], command_line_arguments_t* ctx)
{
    for(int i = 1; i < argc; ++i)
    {
        if(strcmp(argv[i], "-j") == 0)
        {
            ctx->n_threads = atoi(argv[i+1]);
            ++i;
        }
        else
        {
            log_msg(ERROR, "Unknown argument: %s", argv[i]);
            return -1;
        }
    }

    return 0;
}
