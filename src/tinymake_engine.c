#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "tinymake_engine.h"
#include "tinymake_log.h"
#include "tinymake_parser.h"
#include "tinymake_string.h"

int generate_object_files(build_context_t* ctx)
{
    int rc = 0;

    char command[2048];
    char compiler_invokation[1024] = "";
    strcat(compiler_invokation, ctx->compiler);
    if(ctx->compile_flags[0] != '\0')
    {
        strcat(compiler_invokation, " ");
        strcat(compiler_invokation, ctx->compile_flags);
    }

    for(size_t i = 0; i < ctx->includes_count; ++i)
    {
        strcat(compiler_invokation, " ");
        strcat(compiler_invokation, "-I");
        strcat(compiler_invokation, ctx->includes[i]);
    }

    for(size_t i = 0; i < ctx->compile_files_count; ++i)
    {
        if(should_recompile(&ctx->compile_files[i]))
        {
            snprintf(command, sizeof(command), "%s -MMD -MF %s -c %s -o %s", compiler_invokation,
                                                                             ctx->compile_files[i].d_path,
                                                                             ctx->compile_files[i].c_path,
                                                                             ctx->compile_files[i].o_path);
            log_msg(INFO, "Compiling: %s", ctx->compile_files[i].c_path);
            log_msg(DEBUG, "Compile command: %s", command);
            rc = system(command);

            ctx->has_new_obj_file = true;
        }
        else
        {
            log_msg(INFO, "Skipping compilation of: %s", ctx->compile_files[i].c_path);
        }
    }

    return rc;
}

int link_object_files(build_context_t* ctx)
{
    char command[2048];

    struct stat s;
    if(!ctx->has_new_obj_file && stat(ctx->binary_name, &s) == 0)
    {
        log_msg(INFO, "Nothing to link. Skipping...");
        return 0;
    }

    snprintf(command, sizeof(command), "%s -o %s", ctx->compiler, ctx->binary_name);

    for(size_t i = 0; i < ctx->compile_files_count; ++i)
    {
        strcat(command, " ");
        strcat(command, ctx->compile_files[i].o_path);
    }

    log_msg(INFO, "Linking...");
    log_msg(DEBUG, "Link command: %s", command);
    int rc = system(command);

    return rc;
}

bool should_recompile(compile_file_t* file)
{
    struct stat c_file_stat;
    if(stat(file->c_path, &c_file_stat) != 0)
    {
        log_msg(ERROR, "Could not find or access the following file: %s", file->c_path);
    }

    struct stat o_file_stat = {0};
    struct stat d_file_stat = {0};

    // Si le fichier .o ou .d est manquant, on recompile
    if(stat(file->o_path, &o_file_stat) != 0 || stat(file->d_path, &d_file_stat) != 0)
    {
        return true;
    }

    // Si le fichier .o est plus ancien que le fichier .c, on recompile
    if(o_file_stat.st_mtim.tv_sec < c_file_stat.st_mtim.tv_sec)
    {
        return true;
    }

    // Si une des dépendances dans le fichier .d est plus récent que le fichier .o, on recompile
    FILE* dep = fopen(file->d_path, "r");
    if(dep == NULL)
    {
        log_msg(WARN, "Could not find or access dependency file: %s. Recompiling...", file->d_path);
        return true;
    }

    bool is_first_line = true;
    char line[MAX_LINE_SIZE];
    while(fgets(line, sizeof(line), dep))
    {
        char* tokens[128];
        size_t token_count = tokenize_line(trim_whitespaces(line), tokens);
        for(size_t i = is_first_line ? 1 : 0; i < token_count; ++i)
        {
            if(tokens[i][0] == '\\')
            {
                continue;
            }

            struct stat dep_stat;
            if(stat(tokens[i], &dep_stat) == 0 && dep_stat.st_mtim.tv_sec > o_file_stat.st_mtim.tv_sec)
            {
                return true;
            }
        }

        is_first_line = false;
    }

    return false;
}
