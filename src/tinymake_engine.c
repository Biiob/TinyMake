#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include "tinymake_engine.h"
#include "tinymake_constants.h"
#include "tinymake_log.h"
#include "tinymake_parser.h"
#include "tinymake_string.h"
#include "tinymake_types.h"

int create_needed_directories()
{
    if((mkdir(BIN_DIR, 0755) == -1 && errno != EEXIST) || (mkdir(OBJ_DIR, 0755) == -1 && errno != EEXIST))
    {
        log_msg(ERROR, "Could not create binary or object directories. Error code: %d", errno);
        return -1;
    }
    return 0;
}

int generate_object_files(build_context_t* ctx, target_t* target)
{
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

    char log_name[MAX_FILE_NAME_SIZE] = ".tinymake_log_XXXXXX";
    int fd = mkstemp(log_name);
    if(fd == -1)
    {
        perror("mkstemp");
        return -1;
    }

    FILE* log = fdopen(fd, "w");
    if(log == NULL)
    {
        perror("fdopen");
        close(fd);
        unlink(log_name);
        return -1;
    }

    int rc = 0;
    for(size_t i = 0; i < target->source_files_count; ++i)
    {
        snprintf(command, sizeof(command), "%s -MMD -MF %s -c %s -o %s", compiler_invokation,
                                                                         target->source_files[i].d_path,
                                                                         target->source_files[i].c_path,
                                                                         target->source_files[i].o_path);
        if(should_recompile(&target->source_files[i], command))
        {
            log_msg(INFO, "Compiling: %s", target->source_files[i].c_path);
            log_msg(DEBUG, "Compile command: %s", command);
            rc = system(command);
        }
        else
        {
            log_msg(INFO, "Skipping compilation of: %s", target->source_files[i].c_path);
        }

        size_t command_length = strlen(command);
        size_t bytes_written = fwrite(command, sizeof(char), command_length, log);
        if(bytes_written != command_length)
        {
            log_msg(WARN, "Log line might be incomplete");
        }
        putc('\n', log);

        target->should_relink = true;
    }

    fflush(log);
    fsync(fileno(log));
    fclose(log);
    rename(log_name, ".tinymake_log");

    return rc;
}

int link_object_files(build_context_t* ctx, target_t* target)
{
    char command[2048];

    struct stat s;
    if(!target->should_relink && stat(target->name, &s) == 0)
    {
        log_msg(INFO, "Nothing to link. Skipping...");
        return 0;
    }

    snprintf(command, sizeof(command), "%s -o %s", ctx->compiler, target->name);

    for(size_t i = 0; i < target->source_files_count; ++i)
    {
        strcat(command, " ");
        strcat(command, target->source_files[i].o_path);
    }

    log_msg(INFO, "Linking...");
    log_msg(DEBUG, "Link command: %s", command);
    int rc = system(command);

    return rc;
}

bool should_recompile(compile_file_t* file, const char* command)
{
    struct stat c_file_stat;
    if(stat(file->c_path, &c_file_stat) != 0)
    {
        log_msg(ERROR, "Could not find or access the following file: %s", file->c_path);
        return false;
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
    fclose(dep);

    FILE* log = fopen(".tinymake_log", "r");
    bool has_command_changed = true;
    while(fgets(line, sizeof(line), log))
    {
        if(strcmp(trim_whitespaces(line), command) == 0)
        {
            has_command_changed = false;
            break;
        }
    }
    fclose(log);

    if(has_command_changed)
    {
        return true;
    }

    return false;
}
