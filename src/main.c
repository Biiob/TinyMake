#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define MAX_LINE_SIZE 1024
#define MAX_C_FILES 64
#define MAX_FILE_NAME 256

struct build_context_t
{
    char compiler[32];
    char compile_flags[MAX_LINE_SIZE];
    char includes[64];
    char binary_name[MAX_FILE_NAME];
    char c_files[MAX_C_FILES][MAX_FILE_NAME];
    size_t c_files_count;
};

size_t parse_line(char* line, char* tokens[])
{
    char* save_buffer;
    size_t token_count = 0;
    for(char* token = strtok_r(line, " ", &save_buffer); token != NULL; token = strtok_r(NULL, " ", &save_buffer), ++token_count)
    {
        tokens[token_count] = token;
    }

    return token_count;
}

int parse_build_file(struct build_context_t* ctx)
{
    FILE* f = fopen("build.txt", "r");
    if(f == NULL)
    {
        printf("[ERROR] Could not open the build file ('build.txt')\n");
        return -1;
    }

    char line[MAX_LINE_SIZE];
    while(fgets(line, sizeof(line), f))
    {
        // Commentaires et lignes vides
        if(line[0] == '#' || line[0] == ' ' || line[0] == '\n')
        {
            continue;
        }

        char* tokens[MAX_C_FILES];
        char line_to_parse[MAX_LINE_SIZE];
        snprintf(line_to_parse, sizeof(line_to_parse), "%s", line);
        size_t token_count = parse_line(line_to_parse, tokens);

        if(strcmp(tokens[0], "cc") == 0)
        {
            char* equal_sign = strchr(line, '=');
            if(equal_sign == NULL)
            {
                printf("[ERROR] Badly formatted 'cc' line: missing '='\n");
                return -1;
            }
            char* compiler = equal_sign + 1;
            if(compiler[0] == ' ')
            {
                compiler++;
            }

            size_t s = snprintf(ctx->compiler, sizeof(ctx->compiler), "%s", compiler);
            if(ctx->compiler[s - 1] == '\n')
            {
                ctx->compiler[s - 1] = '\0';
            }
            printf("Compiler: %s\n", ctx->compiler);
        }
        else if(strcmp(tokens[0], "cflags") == 0)
        {
            char* equal_sign = strchr(line, '=');
            if(equal_sign == NULL)
            {
                printf("[ERROR] Badly formatted 'cflags' line: missing '='\n");
                return -1;
            }
            char* flags = equal_sign + 1;
            if(flags[0] == ' ')
            {
                flags++;
            }

            size_t s = snprintf(ctx->compile_flags, sizeof(ctx->compile_flags), "%s", flags);
            if(ctx->compile_flags[s - 1] == '\n')
            {
                ctx->compile_flags[s - 1] = '\0';
            }
            printf("Compile flags: %s\n", ctx->compile_flags);
        }
        else if(strcmp(tokens[0], "includes") == 0)
        {
            char* equal_sign = strchr(line, '=');
            if(equal_sign == NULL)
            {
                printf("[ERROR] Badly formatted 'includes' line: missing '='\n");
                return -1;
            }
            char* includes = equal_sign + 1;
            if(includes[0] == ' ')
            {
                includes++;
            }

            size_t s = snprintf(ctx->includes, sizeof(ctx->includes), "%s", includes);
            if(ctx->includes[s - 1] == '\n')
            {
                ctx->includes[s - 1] = '\0';
            }
            printf("Include directory: %s\n", ctx->includes);
        }
        else if(strcmp(tokens[0], "binary") == 0)
        {
            size_t s = snprintf(ctx->binary_name, sizeof(ctx->binary_name), "%s", tokens[1]);
            // On supprime le ':' à la fin du nom de l'exécutable
            ctx->binary_name[s - 1] = '\0';
            for(size_t i = 2; i < token_count; ++i)
            {
                snprintf(ctx->c_files[i-2], sizeof(ctx->c_files[i-2]), "%s", tokens[i]);
            }
            ctx->c_files_count = token_count - 2;
        }
    }

    fclose(f);
    return 0;
}

int generate_object_files(struct build_context_t* ctx)
{
    char command[2048];
    char compiler_invokation[1024] = "";
    strcat(compiler_invokation, ctx->compiler);
    strcat(compiler_invokation, " ");
    if(ctx->compile_flags[0] != '\0')
    {
        strcat(compiler_invokation, ctx->compile_flags);
        strcat(compiler_invokation, " ");
    }
    if(ctx->includes[0] != '\0')
    {
        strcat(compiler_invokation, "-I");
        strcat(compiler_invokation, ctx->includes);
        strcat(compiler_invokation, " ");
    }

    for(size_t i = 0; i < ctx->c_files_count; ++i)
    {
        char file_name[MAX_FILE_NAME];
        strncpy(file_name, ctx->c_files[i], sizeof(file_name) - 1);
        char* last_dot = strrchr(file_name, '.');
        if(last_dot != NULL)
        {
            *last_dot = '\0';
        }

        snprintf(command, sizeof(command), "%s -c %s.c -o %s.o", compiler_invokation, file_name, file_name);

        printf("Command: %s\n", command);
        int rc = system(command);
    }

    return 0;
}

int link_object_files(struct build_context_t* ctx)
{
    char command[2048];
    size_t command_size = snprintf(command, sizeof(command), "gcc -o %s ", ctx->binary_name);
    for(size_t i = 0; i < ctx->c_files_count; ++i)
    {
        char file_name[MAX_FILE_NAME];
        strncpy(file_name, ctx->c_files[i], sizeof(file_name) - 1);
        char* last_dot = strrchr(file_name, '.');
        if(last_dot != NULL)
        {
            *last_dot = '\0';
        }

        char object_file_name[MAX_FILE_NAME];
        size_t file_name_size = snprintf(object_file_name, sizeof(object_file_name), "%s.o", file_name);

        memcpy(command + command_size, object_file_name, file_name_size);
        command_size += file_name_size + 1;
        command[command_size - 1] = ' ';
        command[command_size] = '\0';
    }

    printf("Link command: %s\n", command);
    int rc = system(command);

    return rc;
}

int main()
{
    struct build_context_t ctx = {0};
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

    return 0;
}
