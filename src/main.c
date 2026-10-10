#include "tinymake_types.h"
#include "tinymake_log.h"
#include "tinymake_parser.h"
#include "tinymake_engine.h"

int main()
{
    log_msg(INFO, "Starting build...");

    if(create_needed_directories() != 0)
    {
        log_msg(ERROR, "Build has ended in error. See above for the details.");
        return -1;
    }

    if(open_log_file() != 0)
    {
        log_msg(ERROR, "Build has ended in error. See above for the details.");
        return -1;
    }

    build_context_t ctx = {0};
    log_msg(DEBUG, "Parsing the content of 'build.txt'");
    log_msg(DEBUG, "Context uses %lld KB of memory", sizeof(ctx)/1024);
    if(parse_build_file(&ctx) != 0)
    {
        log_msg(ERROR, "Build has ended in error. See above for the details.");
        return -1;
    }

    for(size_t i = 0; i < ctx.targets_count; ++i)
    {
        log_msg(DEBUG, "Building target: %s", ctx.targets[i].name);
        if(generate_object_files(&ctx, &ctx.targets[i]) != 0)
        {
            log_msg(ERROR, "Build has ended in error. See above for the details.");
            return -1;
        }

        if(link_object_files(&ctx, &ctx.targets[i]) != 0)
        {
            log_msg(ERROR, "Build has ended in error. See above for the details.");
            return -1;
        }
    }

    if(close_log_file() != 0)
    {
        log_msg(ERROR, "Build has ended in error. See above for the details.");
        return -1;
    }

    log_msg(INFO, "Build has ended");

    return 0;
}
