#include "tinymake_types.h"
#include "tinymake_log.h"
#include "tinymake_parser.h"
#include "tinymake_engine.h"

int main()
{
    log_msg(INFO, "Starting build...");

    if(create_needed_directories() != 0)
    {
        log_msg(INFO, "Build has ended in error. See above for the details");
        return -1;
    }

    build_context_t ctx = {0};
    if(parse_build_file(&ctx) != 0)
    {
        log_msg(INFO, "Build has ended in error. See above for the details");
        return -1;
    }

    if(generate_object_files(&ctx) != 0)
    {
        log_msg(INFO, "Build has ended in error. See above for the details");
        return -1;
    }

    if(link_object_files(&ctx) != 0)
    {
        log_msg(INFO, "Build has ended in error. See above for the details");
        return -1;
    }

    log_msg(INFO, "Build has ended");

    return 0;
}
