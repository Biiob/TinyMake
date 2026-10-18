#include "tinymake_types.h"
#include "tinymake_log.h"
#include "tinymake_parser.h"
#include "tinymake_engine.h"
#include "tinymake_cli_parser.h"

int main(int argc, char* argv[])
{
    log_msg(INFO, "Starting build...");

    command_line_arguments_t args = {0};
    if(parse_arguments(argc, argv, &args) != 0)
    {
        goto exit_error;
    }

    if(create_needed_directories() != 0)
    {
        goto exit_error;
    }

    if(open_log_file() != 0)
    {
        goto exit_error;
    }

    build_context_t ctx = {0};
    ctx.n_processes = args.n_processes;
    log_msg(DBG, "Parsing the content of 'build.txt'");
    log_msg(DBG, "Context uses %lld KB of memory", sizeof(ctx)/1024);
    if(parse_build_file(&ctx) != 0)
    {
        goto cleanup;
    }

    for(size_t i = 0; i < ctx.targets_count; ++i)
    {
        log_msg(INFO, "Building target: %s", ctx.targets[i].name);
        if(generate_object_files(&ctx, &ctx.targets[i]) != 0)
        {
            goto cleanup;
        }

        if(link_object_files(&ctx, &ctx.targets[i]) != 0)
        {
            goto cleanup;
        }
    }

    if(close_log_file() != 0)
    {
        goto exit_error;
    }

    log_msg(INFO, "Build has ended");
    return 0;

cleanup:
    close_log_file();

exit_error:
    log_msg(ERROR, "Build has ended in error. See above for the details.");
    return -1;
}
