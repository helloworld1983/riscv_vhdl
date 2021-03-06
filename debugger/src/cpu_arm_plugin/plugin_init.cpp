/*
 *  Copyright 2018 Sergey Khabarov, sergeykhbr@gmail.com
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

#include "cpu_arm7_func.h"
#include "srcproc/srcproc.h"

namespace debugger {

extern "C" void plugin_init(void) {
    REGISTER_CLASS_IDX(CpuCortex_Functional, 1);
    REGISTER_CLASS_IDX(ArmSourceService, 2);
}

}  // namespace debugger
