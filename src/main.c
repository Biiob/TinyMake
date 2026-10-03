#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>

#include "tinymake_log.h"
#include "tinymake_string.h"
#include "tinymake_path.h"

#define MAX_LINE_SIZE 4096
#define MAX_C_FILES 64
#define MAX_FILE_NAME_SIZE 256
#define MAX_INCLUDE_DIRECTORIES 64

typedef struct
{
    char compiler[32];
    char compile_flags[MAX_LINE_SIZE];
    char includes[MAX_INCLUDE_DIRECTORIES][MAX_FILE_NAME_SIZE];
    size_t includes_count;
    char binary_name[MAX_FILE_NAME_SIZE];
    char c_files[MAX_C_FILES][MAX_FILE_NAME_SIZE];
    size_t c_files_count;
} build_context_t;

size_t tokenize_line(char* line, char* tokens[])
{
    char* save_buffer;
    size_t token_count = 0;
    for(char* token = strtok_r(line, " ", &save_buffer); token != NULL; token = strtok_r(NULL, " ", &save_buffer), ++token_count)
    {
        tokens[token_count] = token;
    }

    return token_count;
}

char* parse_key_value_line(const char* line)
{
    char* equal_sign = strchr(line, '=');
    if(equal_sign == NULL)
    {
        return NULL;
    }
    char* value = trim_whitespaces(equal_sign + 1);

    return value;
}

int parse_build_file(build_context_t* ctx)
{
    FILE* f = fopen("build.txt", "r");
    if(f == NULL)
    {
        log_msg(ERROR, "Could not open the build file ('build.txt')");
        return -1;
    }

    char line[MAX_LINE_SIZE];
    while(fgets(line, sizeof(line), f))
    {
        // Commentaires et lignes vides
        if(line[0] == '#' || isspace((unsigned char) line[0]))
        {
            continue;
        }

        char* trimmed_line = trim_whitespaces(line);

        char* line_cursor;
        char* start_token = strtok_r(trimmed_line, " ", &line_cursor);

        if(strcmp(start_token, "cc") == 0)
        {
            char* compiler = parse_key_value_line(line_cursor);
            if(compiler == NULL)
            {
                log_msg(ERROR, "Badly formatted 'cc' line");
                return -1;
            }

            snprintf(ctx->compiler, sizeof(ctx->compiler), "%s", compiler);
            log_msg(DEBUG, "Compiler: %s", ctx->compiler);
        }
        else if(strcmp(start_token, "cflags") == 0)
        {
            char* flags = parse_key_value_line(line_cursor);
            if(flags == NULL)
            {
                log_msg(ERROR, "Badly formatted 'cflags' line");
                return -1;
            }

            snprintf(ctx->compile_flags, sizeof(ctx->compile_flags), "%s", flags);
            log_msg(DEBUG, "Compile flags: %s", ctx->compile_flags);
        }
        else if(strcmp(start_token, "includes") == 0)
        {
            char* includes = parse_key_value_line(line_cursor);
            if(includes == NULL)
            {
                log_msg(ERROR, "Badly formatted 'includes' line");
                return -1;
            }

            char* tokens[MAX_INCLUDE_DIRECTORIES];
            size_t token_count = tokenize_line(includes, tokens);
            ctx->includes_count = token_count;
            log_msg(DEBUG, "%d include directories", ctx->includes_count);

            for(size_t i = 0; i < token_count; ++i)
            {
                snprintf(ctx->includes[i], sizeof(ctx->includes[i]), "%s", tokens[i]);
                log_msg(DEBUG, "Include directory: %s", ctx->includes[i]);
            }
        }
        else if(strcmp(start_token, "binary") == 0)
        {
            char* target_name = strtok_r(NULL, " ", &line_cursor);
            size_t s = snprintf(ctx->binary_name, sizeof(ctx->binary_name), "%s", target_name);
            // On supprime le ':' à la fin du nom de l'exécutable
            ctx->binary_name[s - 1] = '\0';
            log_msg(DEBUG, "Target name: %s", ctx->binary_name);

            char* tokens[MAX_C_FILES];
            size_t token_count = tokenize_line(line_cursor, tokens);
            ctx->c_files_count = token_count;
            log_msg(DEBUG, "%d C files to compile", ctx->c_files_count);

            for(size_t i = 0; i < token_count; ++i)
            {
                snprintf(ctx->c_files[i], sizeof(ctx->c_files[i]), "%s", tokens[i]);
                log_msg(DEBUG, "File to compile: %s", ctx->c_files[i]);
            }
        }
    }

    fclose(f);
    return 0;
}

int generate_object_files(build_context_t* ctx)
{
    int rc = 0;

    char command[2048];
    char compiler_invokation[1024] = "";
    strcat(compiler_invokation, ctx->compiler);
    strcat(compiler_invokation, " ");
    if(ctx->compile_flags[0] != '\0')
    {
        strcat(compiler_invokation, ctx->compile_flags);
        strcat(compiler_invokation, " ");
    }

    for(size_t i = 0; i < ctx->includes_count; ++i)
    {
        strcat(compiler_invokation, "-I");
        strcat(compiler_invokation, ctx->includes[i]);
        strcat(compiler_invokation, " ");
    }

    for(size_t i = 0; i < ctx->c_files_count; ++i)
    {
        char file_name[MAX_FILE_NAME_SIZE];
        get_file_name_without_ext(ctx->c_files[i], file_name);

        snprintf(command, sizeof(command), "%s -c %s -o obj/%s.o", compiler_invokation, ctx->c_files[i], file_name);

        log_msg(INFO, "Compiling: %s", ctx->c_files[i]);
        log_msg(DEBUG, "Compile command: %s", command);
        rc = system(command);
    }

    return rc;
}

int link_object_files(build_context_t* ctx)
{
    char command[2048];
    size_t command_size = snprintf(command, sizeof(command), "%s -o bin/%s ", ctx->compiler, ctx->binary_name);

    for(size_t i = 0; i < ctx->c_files_count; ++i)
    {
        char file_name[MAX_FILE_NAME_SIZE];
        get_file_name_without_ext(ctx->c_files[i], file_name);

        char obj_file[MAX_FILE_NAME_SIZE];
        size_t obj_file_size = snprintf(obj_file, sizeof(obj_file), "obj/%s.o", file_name);

        memcpy(command + command_size, obj_file, obj_file_size);
        command_size += obj_file_size + 1;
        command[command_size - 1] = ' ';
        command[command_size] = '\0';
    }

    log_msg(INFO, "Linking...");
    log_msg(DEBUG, "Link command: %s", command);
    int rc = system(command);

    return rc;
}

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
