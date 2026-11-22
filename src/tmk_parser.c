#include "tmk_parser.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "tmk_array.h"
#include "tmk_log.h"
#include "tmk_str_array.h"

typedef struct
{
  const tmk_str* path;
  size_t line_no;
  // Line on which each key was defined, 0 if not defined yet
  size_t cc_line;
  size_t ar_line;
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

// "binary/library name: sources... [| libraries...]"
static bool parse_target(tmk_config* config, const parse_state* st,
                         tmk_str_array* tokens, tmk_target_type type)
{
  const char* kind = type == TMK_TARGET_BINARY ? "binary" : "library";
  tmk_str* name = tokens->len >= 2 ? &tokens->items[1] : NULL;
  if (name == NULL || name->len < 2 || name->str[name->len - 1] != ':')
  {
    return parse_error(
        st, "expected '%s <name>: <sources...> [| <libraries...>]'", kind);
  }

  // The ':' is not part of the name
  tmk_str_set_len(name, name->len - 1);

  // Sources go up to the '|', and the libraries come after it
  size_t bar = tokens->len;
  for (size_t i = 2; i < tokens->len; ++i)
  {
    if (!tmk_str_equal_cstr(&tokens->items[i], "|"))
    {
      continue;
    }
    if (bar != tokens->len)
    {
      return parse_error(st, "%s '%s' has more than one '|'", kind,
                         tmk_str_cstr(name));
    }
    bar = i;
  }

  size_t n_sources = bar - 2;
  if (n_sources == 0)
  {
    return parse_error(st, "%s '%s' has no source files", kind,
                       tmk_str_cstr(name));
  }
  if (bar == tokens->len - 1)
  {
    return parse_error(st, "%s '%s' has no library after '|'", kind,
                       tmk_str_cstr(name));
  }

  // Binaries and libraries share names: dependencies refer to them
  for (size_t i = 0; i < config->targets.len; ++i)
  {
    const tmk_target* other = &config->targets.items[i];
    if (tmk_str_equal(&other->name, name))
    {
      return parse_error(st, "'%s' already defined on line %zu",
                         tmk_str_cstr(name), other->line_no);
    }
  }

  tmk_target target = {0};
  target.type = type;
  target.name = tmk_str_clone(name);
  target.line_no = st->line_no;

  for (size_t i = 2; i < bar; ++i)
  {
    tmk_array_push(&target.sources, tmk_str_array_take(tokens, i));
  }
  for (size_t i = bar + 1; i < tokens->len; ++i)
  {
    tmk_array_push(&target.deps, tmk_str_array_take(tokens, i));
  }
  tmk_log_debug("Found %s %s with %zu sources and %zu libraries", kind,
                tmk_str_cstr(name), target.sources.len, target.deps.len);

  tmk_array_push(&config->targets, target);

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
    else if (tmk_str_equal_cstr(key, "ar"))
    {
      ok = parse_value(&st, &tokens, &config->archiver, &st.ar_line);
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
      ok = parse_target(config, &st, &tokens, TMK_TARGET_BINARY);
    }
    else if (tmk_str_equal_cstr(key, "library"))
    {
      ok = parse_target(config, &st, &tokens, TMK_TARGET_LIBRARY);
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
