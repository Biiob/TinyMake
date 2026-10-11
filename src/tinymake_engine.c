#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "tinymake_engine.h"
#include "tinymake_constants.h"
#include "tinymake_log.h"
#include "tinymake_parser.h"
#include "tinymake_string.h"
#include "tinymake_types.h"

#define DEFAULT_N_THREADS 4

static int g_log_fd = 0;
static FILE* g_log_file = NULL;
static char g_log_name[MAX_FILE_NAME_SIZE] = ".tinymake_log_XXXXXX";

typedef struct
{
    target_t* target;
    compile_file_t* files;
    size_t begin;
    size_t end;
    char command[1024];
} thread_context_t;

int create_needed_directories()
{
    if((mkdir(BIN_DIR, 0755) == -1 && errno != EEXIST) || (mkdir(OBJ_DIR, 0755) == -1 && errno != EEXIST))
    {
        log_msg(ERROR, "Could not create binary or object directories. Error code: %d", errno);
        return -1;
    }
    return 0;
}

static void* compile_file(void* arg)
{
    thread_context_t* ctx = arg;
    char command[2048];

    for(size_t i = ctx->begin; i < ctx->end; ++i)
    {
        snprintf(command, sizeof(command), "%s -MMD -MF %s -c %s -o %s", ctx->command,
                                                                         ctx->files[i].d_path,
                                                                         ctx->files[i].c_path,
                                                                         ctx->files[i].o_path);
        if(should_recompile(&ctx->files[i], command))
        {
            log_msg(INFO, "Compiling: %s", command);
            int rc = system(command);
            if(!ctx->target->should_relink)
            {
                ctx->target->should_relink = true;
            }
        }
        else
        {
            log_msg(INFO, "Skipping compilation of: %s", ctx->files[i].c_path);
        }

        size_t command_length = strlen(command);
        size_t bytes_written = fwrite(command, sizeof(char), command_length, g_log_file);
        if(bytes_written != command_length)
        {
            log_msg(WARN, "Log line might be incomplete");
        }
        putc('\n', g_log_file);
    }

    return NULL;
}

int generate_object_files(build_context_t* ctx, target_t* target)
{
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

    long n_processors = sysconf(_SC_NPROCESSORS_ONLN);
    size_t n_threads = n_processors == -1 ? DEFAULT_N_THREADS : n_processors;
    n_threads = n_threads > target->source_files_count ? target->source_files_count : n_threads;
    log_msg(DBG, "Compiling with %lld threads", n_threads);
    pthread_t threads[n_threads];
    thread_context_t thread_ctxs[n_threads];

    size_t n_items_per_thread = target->source_files_count / n_threads;

    for(size_t i = 0; i < n_threads; ++i)
    {
        thread_ctxs[i].target = target;
        thread_ctxs[i].files = target->source_files;
        thread_ctxs[i].begin = i * n_items_per_thread;
        thread_ctxs[i].end = (i+1) * n_items_per_thread;
        snprintf(thread_ctxs[i].command, sizeof(thread_ctxs[i].command), "%s", compiler_invokation);

        int err = pthread_create(&threads[i], NULL, compile_file, &thread_ctxs[i]);
        if(err)
        {
            log_msg(ERROR, "pthread_create: %s", strerror(err));
            return -1;
        }
    }

    for(size_t i = 0; i < n_threads; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

int link_object_files(build_context_t* ctx, target_t* target)
{
    char command[2048];

    snprintf(command, sizeof(command), "%s -o %s", ctx->compiler, target->name);

    for(size_t i = 0; i < target->source_files_count; ++i)
    {
        strcat(command, " ");
        strcat(command, target->source_files[i].o_path);
    }

    size_t command_length = strlen(command);
    size_t bytes_written = fwrite(command, sizeof(char), command_length, g_log_file);
    if(bytes_written != command_length)
    {
        log_msg(WARN, "Log line might be incomplete");
    }
    putc('\n', g_log_file);

    struct stat s;
    int rc = 0;

    if(target->should_relink || stat(target->name, &s) != 0 || should_relink(command))
    {
        log_msg(INFO, "Linking...");
        log_msg(DBG, "Link command: %s", command);
        rc = system(command);
    }
    else
    {
        log_msg(INFO, "Nothing to link. Skipping...");
    }

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

    return has_command_changed;
}

bool should_relink(const char* command)
{
    FILE* log = fopen(".tinymake_log", "r");
    char line[MAX_LINE_SIZE];
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

    return has_command_changed;
}

int open_log_file()
{
    g_log_fd = mkstemp(g_log_name);
    if(g_log_fd == -1)
    {
        perror("mkstemp");
        return -1;
    }

    g_log_file = fdopen(g_log_fd, "w");
    if(g_log_file == NULL)
    {
        perror("fdopen");
        close(g_log_fd);
        unlink(g_log_name);
        return -1;
    }

    return 0;
}

int close_log_file()
{
    if(fflush(g_log_file) != 0 || fsync(g_log_fd) != 0)
    {
        perror("fflush");
        fclose(g_log_file);
        close(g_log_fd);
        unlink(g_log_name);
        return -1;
    }

    if(fclose(g_log_file) != 0)
    {
        perror("fclose");
        close(g_log_fd);
        unlink(g_log_name);
        return -1;
    }

    if(rename(g_log_name, ".tinymake_log") != 0)
    {
        perror("rename");
        close(g_log_fd);
        unlink(g_log_name);
        return -1;
    }

    return 0;
}
