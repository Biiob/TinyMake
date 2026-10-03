#include <string.h>

#include "tinymake_path.h"

void get_file_path(const char* full_path, char* out)
{
   const char* last_slash = strrchr(full_path, '/');
   if(last_slash == NULL)
   {
       out[0] = '\0';
   }
   else
   {
       size_t n = last_slash - full_path;
       if(n == 0)
       {
           n = 1;
       }
       memcpy(out, full_path, n);
       out[n] = '\0';
   }
}

void get_file_name(const char* full_path, char* out)
{
    const char* last_slash = strrchr(full_path, '/');
    const char* start_char = last_slash == NULL ? full_path : last_slash + 1;
    size_t n = strlen(start_char);
    memcpy(out, start_char, n);
    out[n] = '\0';
}

void get_file_name_without_ext(const char* full_path, char* out)
{
    const char* last_slash = strrchr(full_path, '/');
    const char* start_char = last_slash == NULL ? full_path : last_slash + 1;
    const char* dot = strrchr(start_char, '.');
    if(dot == NULL || dot == start_char)
    {
        dot = start_char + strlen(start_char);
    }
    size_t n = dot - start_char;
    memcpy(out, start_char, n);
    out[n] = '\0';
}
