#include "tmk_parser.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "tmk_constants.h"
#include "tmk_log.h"
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

// Logs "<path>:<line>: <message>" and returns false
static bool parse_error(const parse_state* st, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

static bool parse_error(const parse_state* st, const char* fmt, ...)
{
  tmk_str msg = tmk_str_create();
  tmk_str_format(&msg, "%s:%zu: ", tmk_str_cstr(st->path), st->line_no);

  va_list args;
  va_start(args, fmt);
  tmk_str_vappend_format(&msg, fmt, args);
  va_end(args);

  tmk_log_error("%s", tmk_str_cstr(&msg));
  tmk_str_free(&msg);

  return false;
}

// Checks a "key = values..." line with min <= values <= max, and that the key
// was not already defined
static bool expect_assignment(const parse_state* st,
                              const tmk_str_array* tokens, size_t min,
                              size_t max, size_t* defined_on)
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
  return true;
}

// "key = value"
static bool parse_value(const parse_state* st, tmk_str_array* tokens,
                        tmk_str* dst, size_t* defined_on)
{
  if (!expect_assignment(st, tokens, 1, 1, defined_on))
  {
    return false;
  }

  *dst = tmk_str_array_take(tokens, 2);
  tmk_log_debug("%s: %s", tmk_str_cstr(&tokens->items[0]), tmk_str_cstr(dst));

  return true;
}

// "key = values..."
static bool parse_list(const parse_state* st, tmk_str_array* tokens,
                       tmk_str_array* dst, size_t* defined_on)
{
  if (!expect_assignment(st, tokens, 0, SIZE_MAX, defined_on))
  {
    return false;
  }

  for (size_t i = 2; i < tokens->len; ++i)
  {
    tmk_log_debug("%s: %s", tmk_str_cstr(&tokens->items[0]),
                  tmk_str_cstr(&tokens->items[i]));
    tmk_array_push(dst, tmk_str_array_take(tokens, i));
  }

  return true;
}

// "binary name: sources..."
static bool parse_binary(tmk_config* config, const parse_state* st,
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
  if (n_sources > TMK_MAX_SOURCES)
  {
    return parse_error(st, "binary '%s' has too many source files (max %d)",
                       tmk_str_cstr(name), TMK_MAX_SOURCES);
  }
  if (config->n_targets == TMK_MAX_TARGETS)
  {
    return parse_error(st, "too many targets (max %d)", TMK_MAX_TARGETS);
  }

  tmk_str path = tmk_str_from_cstr(TMK_BIN_DIR);
  tmk_str_append_format(&path, "/%s", tmk_str_cstr(name));

  for (size_t i = 0; i < config->n_targets; ++i)
  {
    if (tmk_str_equal(&config->targets[i].path, &path))
    {
      tmk_str_free(&path);
      return parse_error(st, "binary '%s' already defined", tmk_str_cstr(name));
    }
  }

  tmk_target* target = &config->targets[config->n_targets++];
  target->type = TMK_TARGET_BINARY;
  target->path = path;
  target->n_sources = 0;
  tmk_log_debug("Found target %s with %zu sources", tmk_str_cstr(&path),
                n_sources);

  for (size_t i = 2; i < tokens->len; ++i)
  {
    target->sources[target->n_sources++] =
        tmk_source_create(tmk_str_array_take(tokens, i));
  }

  return true;
}

bool tmk_parser_parse(tmk_config* config, const tmk_str* path)
{
  FILE* f = fopen(tmk_str_cstr(path), "r");
  if (f == NULL)
  {
    tmk_log_error("Could not open the build file ('%s')", tmk_str_cstr(path));
    return false;
  }

  parse_state st = {.path = path};
  bool ok = true;

  tmk_str line = tmk_str_create();
  while (ok && tmk_str_read_line(&line, f) != -1)
  {
    st.line_no++;
    tmk_str_trim(&line);

    // Blank lines and comments
    if (line.len == 0 || line.str[0] == '#')
    {
      continue;
    }

    tmk_str_array tokens = tmk_str_tokenize(&line, ' ');
    const tmk_str* key = &tokens.items[0];

    if (tmk_str_equal_cstr(key, "cc"))
    {
      ok = parse_value(&st, &tokens, &config->compiler, &st.cc_line);
    }
    else if (tmk_str_equal_cstr(key, "cflags"))
    {
      ok = parse_list(&st, &tokens, &config->compile_flags, &st.cflags_line);
    }
    else if (tmk_str_equal_cstr(key, "ldflags"))
    {
      ok = parse_list(&st, &tokens, &config->link_flags, &st.ldflags_line);
    }
    else if (tmk_str_equal_cstr(key, "includes"))
    {
      ok = parse_list(&st, &tokens, &config->include_dirs, &st.includes_line);
    }
    else if (tmk_str_equal_cstr(key, "binary"))
    {
      ok = parse_binary(config, &st, &tokens);
    }
    else
    {
      ok = parse_error(&st, "unknown key '%s'", tmk_str_cstr(key));
    }

    tmk_str_array_free(&tokens);
  }
  tmk_str_free(&line);
  fclose(f);

  if (ok && st.cc_line == 0)
  {
    tmk_log_error("%s: no compiler defined, add a 'cc = <compiler>' line",
                  tmk_str_cstr(path));
    ok = false;
  }

  return ok;
}
