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

#include "api_core.h"
#include "cpu_riscv_rtl.h"

namespace debugger {

CpuRiscV_RTL::CpuRiscV_RTL(const char *name)  
    : IService(name), IHap(HAP_ConfigDone) {
    registerInterface(static_cast<IThread *>(this));
    registerInterface(static_cast<IClock *>(this));
    registerInterface(static_cast<IHap *>(this));
    registerAttribute("HartID", &hartid_);
    registerAttribute("AsyncReset", &asyncReset_);
    registerAttribute("FpuEnable", &fpuEnable_);
    registerAttribute("TracerEnable", &tracerEnable_);
    registerAttribute("Bus", &bus_);
    registerAttribute("CmdExecutor", &cmdexec_);
    registerAttribute("Tap", &tap_);
    registerAttribute("FreqHz", &freqHz_);
    registerAttribute("InVcdFile", &InVcdFile_);
    registerAttribute("OutVcdFile", &OutVcdFile_);

    bus_.make_string("");
    freqHz_.make_uint64(1);
    fpuEnable_.make_boolean(true);
    InVcdFile_.make_string("");
    OutVcdFile_.make_string("");
    RISCV_event_create(&config_done_, "riscv_sysc_config_done");
    RISCV_register_hap(static_cast<IHap *>(this));

    //createSystemC();
}

CpuRiscV_RTL::~CpuRiscV_RTL() {
    deleteSystemC();
    RISCV_event_close(&config_done_);
}

void CpuRiscV_RTL::postinitService() {
    ibus_ = static_cast<IMemoryOperation *>(
       RISCV_get_service_iface(bus_.to_string(), IFACE_MEMORY_OPERATION));

    if (!ibus_) {
        RISCV_error("Bus interface '%s' not found", 
                    bus_.to_string());
        return;
    }

    icmdexec_ = static_cast<ICmdExecutor *>(
       RISCV_get_service_iface(cmdexec_.to_string(), IFACE_CMD_EXECUTOR));
    if (!icmdexec_) {
        RISCV_error("ICmdExecutor interface '%s' not found", 
                    cmdexec_.to_string());
        return;
    }

    itap_ = static_cast<ITap *>(
       RISCV_get_service_iface(tap_.to_string(), IFACE_TAP));
    if (!itap_) {
        RISCV_error("ITap interface '%s' not found", tap_.to_string());
        return;
    }

    createSystemC();

    if (InVcdFile_.size()) {
        i_vcd_ = sc_create_vcd_trace_file(InVcdFile_.to_string());
        i_vcd_->set_time_unit(1, SC_PS);
    } else {
        i_vcd_ = 0;
    }

    if (OutVcdFile_.size()) {
        o_vcd_ = sc_create_vcd_trace_file(OutVcdFile_.to_string());
        o_vcd_->set_time_unit(1, SC_PS);
    } else {
        o_vcd_ = 0;
    }

    wrapper_->setBus(ibus_);
    wrapper_->setClockHz(freqHz_.to_int());
    wrapper_->generateVCD(i_vcd_, o_vcd_);
    top_->generateVCD(i_vcd_, o_vcd_);

    pcmd_br_ = new CmdBrRiscv(itap_);
    icmdexec_->registerCommand(static_cast<ICommand *>(pcmd_br_));

    pcmd_csr_ = new CmdCsr(itap_);
    icmdexec_->registerCommand(static_cast<ICommand *>(pcmd_csr_));

    pcmd_reg_ = new CmdRegRiscv(itap_);
    icmdexec_->registerCommand(static_cast<ICommand *>(pcmd_reg_));

    pcmd_regs_ = new CmdRegsRiscv(itap_);
    icmdexec_->registerCommand(static_cast<ICommand *>(pcmd_regs_));

    if (!run()) {
        RISCV_error("Can't create thread.", NULL);
        return;
    }
}

void CpuRiscV_RTL::predeleteService() {
    icmdexec_->unregisterCommand(static_cast<ICommand *>(pcmd_br_));
    icmdexec_->unregisterCommand(static_cast<ICommand *>(pcmd_csr_));
    icmdexec_->unregisterCommand(static_cast<ICommand *>(pcmd_reg_));
    icmdexec_->unregisterCommand(static_cast<ICommand *>(pcmd_regs_));
    delete pcmd_br_;
    delete pcmd_csr_;
    delete pcmd_reg_;
    delete pcmd_regs_;
}

void CpuRiscV_RTL::createSystemC() {
    sc_set_default_time_unit(1, SC_NS);

    /** Create all objects, then initilize SystemC context: */
    wrapper_ = new RtlWrapper(static_cast<IService *>(this), "wrapper");
    registerInterface(static_cast<ICpuGeneric *>(wrapper_));
    registerInterface(static_cast<ICpuRiscV *>(wrapper_));
    registerInterface(static_cast<IResetListener *>(wrapper_));
    w_clk = wrapper_->o_clk;
    wrapper_->o_nrst(w_nrst);
    wrapper_->i_time(wb_time);
    wrapper_->o_req_mem_ready(w_req_mem_ready);
    wrapper_->i_req_mem_valid(w_req_mem_valid);
    wrapper_->i_req_mem_path(w_req_mem_path);
    wrapper_->i_req_mem_write(w_req_mem_write);
    wrapper_->i_req_mem_addr(wb_req_mem_addr);
    wrapper_->i_req_mem_strob(wb_req_mem_strob);
    wrapper_->i_req_mem_data(wb_req_mem_data);
    wrapper_->i_req_mem_len(wb_req_mem_len);
    wrapper_->i_req_mem_burst(wb_req_mem_burst);
    wrapper_->o_resp_mem_valid(w_resp_mem_valid);
    wrapper_->o_resp_mem_path(w_resp_mem_path);
    wrapper_->o_resp_mem_data(wb_resp_mem_data);
    wrapper_->o_resp_mem_load_fault(w_resp_mem_load_fault);
    wrapper_->o_resp_mem_store_fault(w_resp_mem_store_fault);
    wrapper_->o_resp_mem_store_fault_addr(wb_resp_mem_store_fault_addr);
    wrapper_->o_interrupt(w_interrupt);
    wrapper_->o_dport_valid(w_dport_valid);
    wrapper_->o_dport_write(w_dport_write);
    wrapper_->o_dport_region(wb_dport_region);
    wrapper_->o_dport_addr(wb_dport_addr);
    wrapper_->o_dport_wdata(wb_dport_wdata);
    wrapper_->i_dport_ready(w_dport_ready);
    wrapper_->i_dport_rdata(wb_dport_rdata);
    wrapper_->i_halted(w_halted);

    top_ = new RiverTop("top", hartid_.to_uint32(),
                               asyncReset_.to_bool(),
                               fpuEnable_.to_bool(),
                               tracerEnable_.to_bool());
    top_->i_clk(wrapper_->o_clk);
    top_->i_nrst(w_nrst);
    top_->i_req_mem_ready(w_req_mem_ready);
    top_->o_req_mem_path(w_req_mem_path);
    top_->o_req_mem_valid(w_req_mem_valid);
    top_->o_req_mem_write(w_req_mem_write);
    top_->o_req_mem_addr(wb_req_mem_addr);
    top_->o_req_mem_strob(wb_req_mem_strob);
    top_->o_req_mem_data(wb_req_mem_data);
    top_->o_req_mem_len(wb_req_mem_len);
    top_->o_req_mem_burst(wb_req_mem_burst);
    top_->o_req_mem_last(w_req_mem_last);
    top_->i_resp_mem_valid(w_resp_mem_valid);
    top_->i_resp_mem_path(w_resp_mem_path);
    top_->i_resp_mem_data(wb_resp_mem_data);
    top_->i_resp_mem_load_fault(w_resp_mem_load_fault);
    top_->i_resp_mem_store_fault(w_resp_mem_store_fault);
    top_->i_resp_mem_store_fault_addr(wb_resp_mem_store_fault_addr);
    top_->i_ext_irq(w_interrupt);
    top_->o_time(wb_time);
    top_->o_exec_cnt(wb_exec_cnt);
    top_->i_dport_valid(w_dport_valid);
    top_->i_dport_write(w_dport_write);
    top_->i_dport_region(wb_dport_region);
    top_->i_dport_addr(wb_dport_addr);
    top_->i_dport_wdata(wb_dport_wdata);
    top_->o_dport_ready(w_dport_ready);
    top_->o_dport_rdata(wb_dport_rdata);
    top_->o_halted(w_halted);

#ifdef DBG_ICACHE_LRU_TB
    ICacheLru_tb *tb = new ICacheLru_tb("tb");
#endif
#ifdef DBG_DCACHE_LRU_TB
    DCacheLru_tb *tb = new DCacheLru_tb("tb");
#endif
#ifdef DBG_IDIV_TB
    IntDiv_tb *tb = new IntDiv_tb("tb");
#endif

    //sc_start(0, SC_NS);
    sc_initialize();
}

void CpuRiscV_RTL::deleteSystemC() {
    delete wrapper_;
    delete top_;
}

void CpuRiscV_RTL::hapTriggered(IFace *isrc, EHapType type,
                                const char *descr) {
    RISCV_event_set(&config_done_);
}

void CpuRiscV_RTL::stop() {
    sc_stop();
    IThread::stop();
}

void CpuRiscV_RTL::busyLoop() {
    RISCV_event_wait(&config_done_);

    sc_start();

    if (i_vcd_) {
        sc_close_vcd_trace_file(i_vcd_);
    }
    if (o_vcd_) {
        sc_close_vcd_trace_file(o_vcd_);
    }
}

}  // namespace debugger

