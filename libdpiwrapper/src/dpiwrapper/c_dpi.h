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

#include <inttypes.h>

static const int AXI4_BURST_LEN_MAX     = 4;

typedef struct axi4_slave_out_t {
    char ready;
    char valid;
    uint64_t rdata[AXI4_BURST_LEN_MAX];
} axi4_slave_out_t;

typedef struct sv_out_t {
	double tm;
	uint32_t clkcnt;
    int irq_request;
    axi4_slave_out_t slvo;
} sv_out_t;


static const int REQ_TYPE_SERVER_ERR    = -2;
static const int REQ_TYPE_STOP_SIM      = -1;
static const int REQ_TYPE_INFO          = 1;
static const int REQ_TYPE_MOVE_CLOCK    = 2;
static const int REQ_TYPE_MOVE_TIME     = 3;
static const int REQ_TYPE_AXI4          = 4;

typedef struct axi4_slave_in_t {
    uint64_t addr;
    uint64_t wdata[AXI4_BURST_LEN_MAX];
    char we;        // 0 = read; 1 = write
    char wstrb;     // 0xff for burst operations
    char len;       // 0=not a burst; not 0 = burst 8 bytes
} axi4_slave_in_t;

typedef struct sv_in_t {
    int req_valid;
    int req_type;
    int param1;
    axi4_slave_in_t slvi;
} sv_in_t;

typedef void (*sv_func_info_proc)(const char *info);

typedef struct dpi_sv_interface {
    sv_func_info_proc sv_func_info;
} dpi_sv_interface;

typedef int (*c_task_server_start_proc)();
typedef int (*c_task_slv_clk_posedge_proc)(const sv_out_t *sv2c,
                                           sv_in_t *c2sv);

/** Interface between clients and DPI context task */
enum EJsonRequestList {
    Req_SourceName,
    Req_CmdType,
    Req_Data,
    Req_ListSize
};

enum EJsonResponseList {
    Resp_CmdType,
    Resp_Data,
    Resp_ListSize
};

void dpi_put_fifo_request(void *iface);
