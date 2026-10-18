#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "tinymake_parser.h"
#include "tinymake_log.h"
#include "tinymake_string.h"
#include "tinymake_path.h"

void create_compile_file(const char* path, compile_file_t* out)
{
    get_file_name_without_ext(path, out->stripped_name);
    snprintf(out->c_path, sizeof(out->c_path), "%s", path);
    snprintf(out->o_path, sizeof(out->o_path), OBJ_DIR "/%s.o", out->stripped_name);
    snprintf(out->d_path, sizeof(out->d_path), OBJ_DIR "/%s.d", out->stripped_name);
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

    ctx->targets_count = 0;

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

        if(strcasecmp(start_token, "cc") == 0)
        {
            char* compiler = parse_key_value_line(line_cursor);
            if(compiler == NULL)
            {
                log_msg(ERROR, "Badly formatted 'cc' line");
                return -1;
            }

            snprintf(ctx->compiler, sizeof(ctx->compiler), "%s", compiler);
            log_msg(DBG, "Compiler: %s", ctx->compiler);
        }
        else if(strcasecmp(start_token, "cflags") == 0)
        {
            char* flags = parse_key_value_line(line_cursor);
            if(flags == NULL)
            {
                log_msg(ERROR, "Badly formatted 'cflags' line");
                return -1;
            }

            log_msg(DBG, "Compile flags: %s", flags);

            char* tokens[MAX_FLAGS];
            size_t token_count = tokenize_line(flags, tokens);
            ctx->compile_flags_count = token_count;

            for(size_t i = 0; i < token_count; ++i)
            {
                snprintf(ctx->compile_flags[i], sizeof(ctx->compile_flags[i]), "%s", tokens[i]);
            }
        }
        else if(strcasecmp(start_token, "includes") == 0)
        {
            char* includes = parse_key_value_line(line_cursor);
            if(includes == NULL)
            {
                log_msg(ERROR, "Badly formatted 'includes' line");
                return -1;
            }

            char* tokens[MAX_INCLUDE_DIRECTORIES];
            size_t token_count = tokenize_line(includes, tokens);
            ctx->includes_dir_count = token_count;
            log_msg(DBG, "%d include directories", ctx->includes_dir_count);

            for(size_t i = 0; i < token_count; ++i)
            {
                snprintf(ctx->includes_dir[i], sizeof(ctx->includes_dir[i]), "-I%s", tokens[i]);
                log_msg(DBG, "Include directory: %s", tokens[i]);
            }
        }
        else if(strcasecmp(start_token, "binary") == 0)
        {
            target_t* current_target = &ctx->targets[ctx->targets_count];
            current_target->type = BINARY;

            char* target_name = strtok_r(NULL, " ", &line_cursor);
            size_t s = snprintf(current_target->name, sizeof(current_target->name), BIN_DIR "/%s", target_name);
            // On supprime le ':' à la fin du nom de l'exécutable
            current_target->name[s - 1] = '\0';
            log_msg(DBG, "Found target: %s", current_target->name);

            char* tokens[MAX_C_FILES];
            size_t token_count = tokenize_line(line_cursor, tokens);
            current_target->source_files_count = token_count;
            log_msg(DBG, "with %d files to compile", current_target->source_files_count);

            for(size_t i = 0; i < token_count; ++i)
            {
                create_compile_file(tokens[i], &current_target->source_files[i]);
                log_msg(DBG, "File to compile: %s", current_target->source_files[i].c_path);
            }

            ctx->targets_count++;
        }
    }

    fclose(f);
    return 0;
}
