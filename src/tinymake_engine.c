#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <spawn.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "tinymake_engine.h"
#include "tinymake_constants.h"
#include "tinymake_log.h"
#include "tinymake_string.h"
#include "tinymake_types.h"

extern char** environ;

static int g_log_fd = 0;
static FILE* g_log_file = NULL;
static char g_log_name[MAX_FILE_NAME_SIZE] = ".tinymake_log_XXXXXX";

typedef enum
{
    PENDING,
    SKIPPED,
    RUNNING,
    DONE,
    FAILED
} job_status_t;

typedef struct
{
    compile_file_t* file;
    pid_t pid;
    int pipes[2];
    char command[2048];
    char buffer[8192];
    job_status_t status;
} job_t;

typedef enum
{
    CHILD_OK,
    CHILD_EXIT_FAILURE,
    CHILD_SIGNALED,
    CHILD_WAIT_ERROR
} child_result_t;

typedef struct
{
    char** argv;
    size_t argc;
    size_t dep_idx;
    size_t file_idx;
    size_t obj_idx;
} compile_args_t;

static void build_argv(build_context_t* ctx, compile_args_t* args)
{
    size_t arg = 0;
    args->argv[arg++] = ctx->compiler;
    for(size_t i = 0; i < ctx->flags_count; ++i, ++arg)
    {
        args->argv[arg] = ctx->compile_flags_tok[i];
    }
    for(size_t i = 0; i < ctx->includes_count; ++i, ++arg)
    {
        args->argv[arg] = ctx->includes_tok[i];
    }
    args->argv[arg++] = "-fdiagnostics-color=always";
    args->argv[arg++] = "-MMD";
    args->argv[arg++] = "-MF";
    args->dep_idx = arg++;
    args->argv[arg++] = "-c";
    args->file_idx = arg++;
    args->argv[arg++] = "-o";
    args->obj_idx = arg++;
    args->argv[arg] = NULL;
}

static void build_display_command(compile_args_t* args, char* command)
{
    size_t len = 0;
    for(size_t i = 0; i < args->argc; ++i)
    {
        if(args->argv[i] != NULL && args->argv[i][0] != '\0')
        {
            strcat(command, args->argv[i]);
            strcat(command, " ");
            len += strlen(args->argv[i]) + 1;
        }
    }
    command[len - 1] = '\0';
}

static void print_command_to_log(const char* command)
{
    size_t command_length = strlen(command);
    size_t bytes_written = fwrite(command, sizeof(char), command_length, g_log_file);
    if(bytes_written != command_length)
    {
        log_msg(WARN, "Command line in the log file is truncated. Will trigger a recompilation on the next run");
    }
    putc('\n', g_log_file);
}

static child_result_t wait_child(pid_t pid, int* detail)
{
    int status;
    pid_t r;

    do
    {
        r = waitpid(pid, &status, 0);
    } while(r == -1 && errno == EINTR);

    if(r == -1)
    {
        *detail = errno;
        return CHILD_WAIT_ERROR;
    }

    if(WIFEXITED(status))
    {
        *detail = WEXITSTATUS(status);
        return *detail == 0 ? CHILD_OK : CHILD_EXIT_FAILURE;
    }

    if(WIFSIGNALED(status))
    {
        *detail = WTERMSIG(status);
        return CHILD_SIGNALED;
    }

    *detail = 0;
    return CHILD_WAIT_ERROR;
}

int generate_object_files(build_context_t* ctx, target_t* target)
{
    const size_t n_processes = ctx->n_processes != 0 ? ctx->n_processes : 1;
    size_t available_processes = n_processes;
    const size_t n_files = target->source_files_count;
    job_t jobs[n_files];
    struct pollfd pfds[n_files];
    for(size_t i = 0; i < n_files; ++i)
    {
        jobs[i].file = &target->source_files[i];
        jobs[i].status = PENDING;
        jobs[i].command[0] = '\0';
        pfds[i].fd = -1;
    }

    size_t argc = ctx->flags_count + ctx->includes_count + 10;
    char* argv[argc];
    compile_args_t args = {0};
    args.argc = argc;
    args.argv = argv;
    build_argv(ctx, &args);

    bool hasFailedJob = false;
    for(;;)
    {
        bool hasPendingJob = false;
        bool hasRunningJob = false;
        for(size_t i = 0; i < n_files; ++i)
        {
            if(jobs[i].status == FAILED)
            {
                hasFailedJob = true;
            }
            if(jobs[i].status == PENDING)
            {
                hasPendingJob = true;
            }
            else if(jobs[i].status == RUNNING)
            {
                hasRunningJob = true;
            }
        }

        if(!hasRunningJob && (!hasPendingJob || hasFailedJob))
        {
            break;
        }

        while(available_processes > 0 && hasPendingJob && !hasFailedJob)
        {
            size_t j = 0;
            while(j < n_files && jobs[j].status != PENDING)
            {
                j++;
            }

            if(j == n_files)
            {
                break;
            }

            args.argv[args.dep_idx] = jobs[j].file->d_path;
            args.argv[args.file_idx] = jobs[j].file->c_path;
            args.argv[args.obj_idx] = jobs[j].file->o_path;

            build_display_command(&args, jobs[j].command);

            if(!should_recompile(jobs[j].file, jobs[j].command))
            {
                log_msg(INFO, "Skipping compilation of: %s", jobs[j].file->c_path);
                print_command_to_log(jobs[j].command);
                jobs[j].status = SKIPPED;
            }
            else
            {
                if(pipe(jobs[j].pipes) != 0)
                {
                    log_msg(ERROR, "pipe: %s", strerror(errno));
                    jobs[j].status = FAILED;
                    goto exit_error;
                }

                if(fcntl(jobs[j].pipes[0], F_SETFD, FD_CLOEXEC) != 0 || fcntl(jobs[j].pipes[1], F_SETFD, FD_CLOEXEC) != 0)
                {
                    log_msg(ERROR, "fcntl: %s", strerror(errno));
                    jobs[j].status = FAILED;
                    close(jobs[j].pipes[0]);
                    close(jobs[j].pipes[1]);
                    goto exit_error;
                }

                posix_spawn_file_actions_t fa;
                int err = posix_spawn_file_actions_init(&fa);
                if(err)
                {
                    log_msg(ERROR, "posix_spawn_file_actions_init: %s", strerror(err));
                    jobs[j].status = FAILED;
                    close(jobs[j].pipes[0]);
                    close(jobs[j].pipes[1]);
                    goto exit_error;
                }

                err = posix_spawn_file_actions_adddup2(&fa, jobs[j].pipes[1], STDOUT_FILENO);
                if(err)
                {
                    log_msg(ERROR, "posix_spawn_file_actions_adddup2: %s", strerror(err));
                    jobs[j].status = FAILED;
                    close(jobs[j].pipes[0]);
                    close(jobs[j].pipes[1]);
                    posix_spawn_file_actions_destroy(&fa);
                    goto exit_error;
                }

                err = posix_spawn_file_actions_adddup2(&fa, jobs[j].pipes[1], STDERR_FILENO);
                if(err)
                {
                    log_msg(ERROR, "posix_spawn_file_actions_adddup2: %s", strerror(err));
                    jobs[j].status = FAILED;
                    close(jobs[j].pipes[0]);
                    close(jobs[j].pipes[1]);
                    posix_spawn_file_actions_destroy(&fa);
                    goto exit_error;
                }

                err = posix_spawnp(&jobs[j].pid, ctx->compiler, &fa, NULL, argv, environ);
                posix_spawn_file_actions_destroy(&fa);
                if(err)
                {
                    log_msg(ERROR, "posix_spawnp: %s", strerror(err));
                    jobs[j].status = FAILED;
                    close(jobs[j].pipes[0]);
                    close(jobs[j].pipes[1]);
                    goto exit_error;
                }

                jobs[j].status = RUNNING;

                close(jobs[j].pipes[1]);
                pfds[j].fd = jobs[j].pipes[0];
                pfds[j].events = POLLIN;

                available_processes--;

                target->should_relink = true;
            }
        }

        if(available_processes == n_processes)
        {
            continue;
        }

        if(poll(pfds, n_files, -1) == -1)
        {
            if(errno == EINTR)
            {
                continue;
            }
            else
            {
                goto exit_error;
            }
        }

        for(size_t i = 0; i < n_files; ++i)
        {
            if(jobs[i].status != RUNNING)
            {
                continue;
            }

            if(pfds[i].revents & (POLLIN | POLLHUP | POLLERR))
            {
                char buf[4096];
                ssize_t n = read(pfds[i].fd, buf, sizeof(buf));
                if(n > 0)
                {
                    strncat(jobs[i].buffer, buf, n);
                }
                else if(n == 0 || errno != EINTR)
                {
                    close(pfds[i].fd);
                    pfds[i].fd = -1;
                    available_processes++;

                    int detail;
                    switch(wait_child(jobs[i].pid, &detail))
                    {
                        case CHILD_OK:
                            jobs[i].status = DONE;
                            log_msg(INFO, "%s\n%s", jobs[i].command, jobs[i].buffer);
                            print_command_to_log(jobs[i].command);
                            break;

                        case CHILD_EXIT_FAILURE:
                            jobs[i].status = FAILED;
                            log_msg(ERROR, "\n%s", jobs[i].buffer);
                            log_msg(ERROR, "%s: compiler exited with code %d", jobs[i].file->c_path, detail);
                            break;

                        case CHILD_SIGNALED:
                            jobs[i].status = FAILED;
                            log_msg(ERROR, "\n%s", jobs[i].buffer);
                            log_msg(ERROR, "%s: compiler killed by signal %d", jobs[i].file->c_path, detail);
                            break;

                        case CHILD_WAIT_ERROR:
                            jobs[i].status = FAILED;
                            log_msg(ERROR, "waitpid for %s: %s", jobs[i].file->c_path,
                                    detail ? strerror(detail) : "unexpected child status");
                            goto exit_error;
                    }
                }
            }
        }
    }

    return hasFailedJob ? -1 : 0;

    exit_error:

    for(size_t i = 0; i < n_files; ++i)
    {
        if(jobs[i].status == RUNNING)
        {
            if(pfds[i].fd != -1)
            {
                close(pfds[i].fd);
            }
            pfds[i].fd = -1;
            int d;
            wait_child(jobs[i].pid, &d);
        }
    }
    return -1;
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

int create_needed_directories()
{
    if((mkdir(BIN_DIR, 0755) == -1 && errno != EEXIST) || (mkdir(OBJ_DIR, 0755) == -1 && errno != EEXIST))
    {
        log_msg(ERROR, "Could not create binary or object directories. Error code: %d", errno);
        return -1;
    }
    return 0;
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
