#include "tmk_parser.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "tmk_log.h"
#include "tmk_path.h"
#include "tmk_str.h"
#include "tmk_str_array.h"

typedef struct
{
  const tmk_str* path;
  size_t line_no;
  // Line on which each key was defined, 0 if not defined yet
  size_t cc_line;
  size_t cflags_line;
  size_t ldflags_line;
  size_t includes_line;
} parse_state;

// Logs "<path>:<line>: <message>" and returns -1
static int parse_error(const parse_state* st, const char* fmt, ...)
{
  tmk_str msg = tmk_str_create();
  tmk_str_format(&msg, "%s:%zu: ", tmk_str_cstr(st->path), st->line_no);

  va_list args;
  va_start(args, fmt);
  tmk_str_vappend_format(&msg, fmt, args);
  va_end(args);

  log_msg(ERROR, "%s", tmk_str_cstr(&msg));
  tmk_str_free(&msg);

  return -1;
}

// Checks a "key = values..." line with min <= values <= max, and that the key
// was not already defined
static int expect_assignment(const parse_state* st, const tmk_str_array* tokens,
                             size_t min, size_t max, size_t* defined_on)
{
  const char* key = tmk_str_cstr(&tokens->items[0]);

  if (tokens->len < 2 || !tmk_str_equal_cstr(&tokens->items[1], "="))
  {
    return parse_error(st, "expected '=' after '%s'", key);
  }

  if (*defined_on != 0)
  {
    return parse_error(st, "'%s' already defined on line %zu", key,
                       *defined_on);
  }

  size_t n_values = tokens->len - 2;
  if (min == max && n_values != min)
  {
    return parse_error(st, "'%s' expects exactly %zu value(s), got %zu", key,
                       min, n_values);
  }
  if (n_values < min)
  {
    return parse_error(st, "'%s' expects at least %zu value(s), got %zu", key,
                       min, n_values);
  }
  if (n_values > max)
  {
    return parse_error(st, "'%s' expects at most %zu value(s), got %zu", key,
                       max, n_values);
  }

  *defined_on = st->line_no;
  return 0;
}

// "key = value"
static int parse_value(const parse_state* st, tmk_str_array* tokens,
                       tmk_str* dst, size_t* defined_on)
{
  if (expect_assignment(st, tokens, 1, 1, defined_on) != 0)
  {
    return -1;
  }

  *dst = tmk_str_array_take(tokens, 2);
  log_msg(DBG, "%s: %s", tmk_str_cstr(&tokens->items[0]), tmk_str_cstr(dst));

  return 0;
}

// "key = values..."
static int parse_list(const parse_state* st, tmk_str_array* tokens,
                      tmk_str_array* dst, size_t* defined_on)
{
  if (expect_assignment(st, tokens, 0, SIZE_MAX, defined_on) != 0)
  {
    return -1;
  }

  for (size_t i = 2; i < tokens->len; ++i)
  {
    log_msg(DBG, "%s: %s", tmk_str_cstr(&tokens->items[0]),
            tmk_str_cstr(&tokens->items[i]));
    tmk_str_array_push(dst, tmk_str_array_take(tokens, i));
  }

  return 0;
}

// "binary name: sources..."
static int parse_binary(build_context* ctx, const parse_state* st,
                        tmk_str_array* tokens)
{
  tmk_str* name = tokens->len >= 2 ? &tokens->items[1] : NULL;
  if (name == NULL || name->len < 2 || name->str[name->len - 1] != ':')
  {
    return parse_error(st, "expected 'binary <name>: <sources...>'");
  }

  // The ':' is not part of the name
  tmk_str_set_len(name, name->len - 1);

  size_t n_sources = tokens->len - 2;
  if (n_sources == 0)
  {
    return parse_error(st, "binary '%s' has no source files",
                       tmk_str_cstr(name));
  }
  if (n_sources > MAX_C_FILES)
  {
    return parse_error(st, "binary '%s' has too many source files (max %d)",
                       tmk_str_cstr(name), MAX_C_FILES);
  }
  if (ctx->targets_count == MAX_TARGETS)
  {
    return parse_error(st, "too many targets (max %d)", MAX_TARGETS);
  }

  tmk_str target_name = tmk_str_from_cstr(BIN_DIR);
  tmk_str_append_format(&target_name, "/%s", tmk_str_cstr(name));

  for (size_t i = 0; i < ctx->targets_count; ++i)
  {
    if (tmk_str_equal(&ctx->targets[i].name, &target_name))
    {
      tmk_str_free(&target_name);
      return parse_error(st, "binary '%s' already defined", tmk_str_cstr(name));
    }
  }

  target* current_target = &ctx->targets[ctx->targets_count++];
  current_target->type = BINARY;
  current_target->name = target_name;
  current_target->source_files_count = 0;
  log_msg(DBG, "Found target: %s with %zu files to compile",
          tmk_str_cstr(&current_target->name), n_sources);

  for (size_t i = 2; i < tokens->len; ++i)
  {
    log_msg(DBG, "File to compile: %s", tmk_str_cstr(&tokens->items[i]));
    current_target->source_files[current_target->source_files_count++] =
        create_compile_file(&tokens->items[i]);
  }

  return 0;
}

static void free_target(target* target)
{
  tmk_str_free(&target->name);
  for (size_t i = 0; i < target->source_files_count; ++i)
  {
    tmk_str_free(&target->source_files[i].stripped_name);
    tmk_str_free(&target->source_files[i].c_path);
    tmk_str_free(&target->source_files[i].o_path);
    tmk_str_free(&target->source_files[i].d_path);
  }
}

void free_build_context(build_context* ctx)
{
  tmk_str_free(&ctx->compiler);
  tmk_str_array_free(&ctx->compile_flags);
  tmk_str_array_free(&ctx->link_flags);
  tmk_str_array_free(&ctx->includes_dir);

  for (size_t i = 0; i < ctx->targets_count; ++i)
  {
    free_target(&ctx->targets[i]);
  }
}

int parse_build_file(build_context* ctx, const tmk_str* path)
{
  FILE* f = fopen(tmk_str_cstr(path), "r");
  if (f == NULL)
  {
    log_msg(ERROR, "Could not open the build file ('%s')", tmk_str_cstr(path));
    return -1;
  }

  parse_state st = {.path = path};
  int rc = 0;

  tmk_str line = tmk_str_create();
  while (rc == 0 && tmk_str_read_line(&line, f) != -1)
  {
    st.line_no++;
    tmk_str_trim(&line);

    // Commentaires et lignes vides
    if (line.len == 0 || line.str[0] == '#')
    {
      continue;
    }

    tmk_str_array tokens = tmk_str_tokenize(&line, ' ');
    const tmk_str* key = &tokens.items[0];

    if (tmk_str_equal_cstr(key, "cc"))
    {
      rc = parse_value(&st, &tokens, &ctx->compiler, &st.cc_line);
    }
    else if (tmk_str_equal_cstr(key, "cflags"))
    {
      rc = parse_list(&st, &tokens, &ctx->compile_flags, &st.cflags_line);
    }
    else if (tmk_str_equal_cstr(key, "ldflags"))
    {
      rc = parse_list(&st, &tokens, &ctx->link_flags, &st.ldflags_line);
    }
    else if (tmk_str_equal_cstr(key, "includes"))
    {
      rc = parse_list(&st, &tokens, &ctx->includes_dir, &st.includes_line);
    }
    else if (tmk_str_equal_cstr(key, "binary"))
    {
      rc = parse_binary(ctx, &st, &tokens);
    }
    else
    {
      rc = parse_error(&st, "unknown key '%s'", tmk_str_cstr(key));
    }

    tmk_str_array_free(&tokens);
  }
  tmk_str_free(&line);
  fclose(f);

  if (rc == 0 && st.cc_line == 0)
  {
    log_msg(ERROR, "%s: no compiler defined, add a 'cc = <compiler>' line",
            tmk_str_cstr(path));
    rc = -1;
  }

  return rc;
}
