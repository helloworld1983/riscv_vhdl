/*
 *  Copyright 2019 Sergey Khabarov, sergeykhbr@gmail.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include "types.h"
#include <inttypes.h>

extern "C" void LIB_thread_create(void *data);

extern "C" uint64_t LIB_thread_id();

extern "C" void LIB_thread_join(thread_def th, int ms);

extern "C" void LIB_sleep_ms(int ms);

extern "C" int LIB_printf(const char *fmt, ...);

extern "C" void *LIB_get_proc_addr(const char *f);

