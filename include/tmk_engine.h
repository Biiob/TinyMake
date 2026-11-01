#ifndef TMK_ENGINE_H
#define TMK_ENGINE_H

#include "tmk_types.h"

int generate_object_files(build_context* ctx, target* target);
int link_object_files(build_context* ctx, target* target);

#endif
