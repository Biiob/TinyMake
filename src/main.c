#include "tinymake_types.h"
#include "tinymake_log.h"
#include "tinymake_parser.h"
#include "tinymake_engine.h"

int main()
{
    log_msg(INFO, "Starting build...");

    build_context_t ctx = {0};
    if(parse_build_file(&ctx) != 0)
    {
        return -1;
    }

    if(generate_object_files(&ctx) != 0)
    {
        return -1;
    }

    if(link_object_files(&ctx) != 0)
    {
        return -1;
    }

    log_msg(INFO, "Build has ended");

    return 0;
}
