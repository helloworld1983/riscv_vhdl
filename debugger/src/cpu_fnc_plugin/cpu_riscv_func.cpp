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

#include <api_core.h>
#include "cpu_riscv_func.h"
#include "debug/dsumap.h"
#include "generic/riscv_disasm.h"

namespace debugger {

CpuRiver_Functional::CpuRiver_Functional(const char *name) :
    CpuGeneric(name),
    portRegs_(this, "regs", DSUREG(ureg.v.iregs), Reg_Total),
    portSavedRegs_(this, "savedregs", 0, Reg_Total),  // not mapped !!!
    portRegsFpu_(this, "fregs", DSUREG(ureg.v.fregs), RegFpu_Total),
    portCSR_(this, "csr", DSUREG(csr), 1<<12) {
    registerInterface(static_cast<ICpuRiscV *>(this));
    registerAttribute("VendorID", &vendorid_);
    registerAttribute("ImplementationID", &implementationid_);
    registerAttribute("HartID", &hartid_);
    registerAttribute("ListExtISA", &listExtISA_);
    registerAttribute("VectorTable", &vectorTable_);
    registerAttribute("ExceptionTable", &exceptionTable_);
}

CpuRiver_Functional::~CpuRiver_Functional() {
}

void CpuRiver_Functional::postinitService() {
    // Supported instruction sets:
    for (int i = 0; i < INSTR_HASH_TABLE_SIZE; i++) {
        listInstr_[i].make_list(0);
    }
    addIsaUserRV64I();
    addIsaPrivilegedRV64I();
    for (unsigned i = 0; i < listExtISA_.size(); i++) {
        if (listExtISA_[i].to_string()[0] == 'A') {
            addIsaExtensionA();
        } else if (listExtISA_[i].to_string()[0] == 'C') {
            addIsaExtensionC();
        } else if (listExtISA_[i].to_string()[0] == 'D') {
            addIsaExtensionD();
        } else if (listExtISA_[i].to_string()[0] == 'F') {
            addIsaExtensionF();
        } else if (listExtISA_[i].to_string()[0] == 'M') {
            addIsaExtensionM();
        }
    }

    // Power-on
    reset(0);

    CpuGeneric::postinitService();

    pcmd_br_ = new CmdBrRiscv(itap_);
    icmdexec_->registerCommand(static_cast<ICommand *>(pcmd_br_));

    pcmd_csr_ = new CmdCsr(itap_);
    icmdexec_->registerCommand(static_cast<ICommand *>(pcmd_csr_));

    pcmd_reg_ = new CmdRegRiscv(itap_);
    icmdexec_->registerCommand(static_cast<ICommand *>(pcmd_reg_));

    pcmd_regs_ = new CmdRegsRiscv(itap_);
    icmdexec_->registerCommand(static_cast<ICommand *>(pcmd_regs_));
}

void CpuRiver_Functional::predeleteService() {
    CpuGeneric::predeleteService();

    icmdexec_->unregisterCommand(static_cast<ICommand *>(pcmd_br_));
    icmdexec_->unregisterCommand(static_cast<ICommand *>(pcmd_csr_));
    icmdexec_->unregisterCommand(static_cast<ICommand *>(pcmd_reg_));
    icmdexec_->unregisterCommand(static_cast<ICommand *>(pcmd_regs_));
    delete pcmd_br_;
    delete pcmd_csr_;
    delete pcmd_reg_;
    delete pcmd_regs_;
}

unsigned CpuRiver_Functional::addSupportedInstruction(
                                    RiscvInstruction *instr) {
    AttributeType tmp(instr);
    listInstr_[instr->hash()].add_to_list(&tmp);
    return 0;
}

void CpuRiver_Functional::handleTrap() {
    csr_mstatus_type mstatus;
    csr_mcause_type mcause;

    /** Check stack protection exceptions: */
    uint64_t mstackovr = portCSR_.read(CSR_mstackovr).val;
    uint64_t mstackund = portCSR_.read(CSR_mstackund).val;
    uint64_t sp = portRegs_.read(Reg_sp).val;
    if (mstackovr != 0 && sp < mstackovr) {
        raiseSignal(EXCEPTION_StackOverflow);
        portCSR_.write(CSR_mstackovr, 0);
    } else if (mstackund != 0 && sp > mstackund) {
        raiseSignal(EXCEPTION_StackUnderflow);
        portCSR_.write(CSR_mstackund, 0);
    }

    if ((interrupt_pending_[0] | interrupt_pending_[1]) == 0) {
        return;
    }

    mstatus.value = portCSR_.read(CSR_mstatus).val;
    mcause.value =  portCSR_.read(CSR_mcause).val;
    uint64_t exception_mask = (1ull << INTERRUPT_USoftware) - 1;
    if ((interrupt_pending_[0] & exception_mask) == 0 && 
        mstatus.bits.MIE == 0 && cur_prv_level == PRV_M) {
        return;
    }
    if (mcause.value == EXCEPTION_Breakpoint) {
        DsuMapType::udbg_type::debug_region_type::breakpoint_control_reg t1;
        t1.val = br_control_.getValue().val;
        if (t1.bits.trap_on_break == 0) {
            sw_breakpoint_ = true;
            interrupt_pending_[0] &= ~(1ull << EXCEPTION_Breakpoint);
            npc_.setValue(pc_.getValue());
            halt("EBREAK Breakpoint");
            return;
        }
    }

    // All traps handle via machine mode while CSR mdelegate
    // doesn't setup other.
    // @todo delegating
    mstatus.bits.MPP = cur_prv_level;
    mstatus.bits.MPIE = (mstatus.value >> cur_prv_level) & 0x1;
    mstatus.bits.MIE = 0;
    cur_prv_level = PRV_M;
    portCSR_.write(CSR_mstatus, mstatus.value);

    int xepc = static_cast<int>((cur_prv_level << 8) + 0x41);
    portCSR_.write(xepc, npc_.getValue().val);
    if (interrupt_pending_[0] & exception_mask) {
        // Exception: ['cfg_nmi_name', address, ....]
        int entry_idx = 2*static_cast<int>(mcause.value) + 1;
        uint64_t trap = exceptionTable_[entry_idx].to_uint64();
        npc_.setValue(trap);
    } else {
        // Software interrupt handled after instruction was executed
        npc_.setValue(portCSR_.read(CSR_mtvec));
    }
    interrupt_pending_[0] = 0;
}

void CpuRiver_Functional::reset(IFace *isource) {
    CpuGeneric::reset(isource);
    portRegs_.reset();
    portCSR_.reset();
    portCSR_.write(CSR_mvendorid, vendorid_.to_uint64());
    portCSR_.write(CSR_mimplementationid, implementationid_.to_uint64());
    portCSR_.write(CSR_mhartid, hartid_.to_uint64());
    portCSR_.write(CSR_mtvec, vectorTable_.to_uint64());

    cur_prv_level = PRV_M;           // Current privilege level
}

GenericInstruction *CpuRiver_Functional::decodeInstruction(Reg64Type *cache) {
    RiscvInstruction *instr = NULL;
    int hash_idx = hash32(cacheline_[0].buf32[0]);
    for (unsigned i = 0; i < listInstr_[hash_idx].size(); i++) {
        instr = static_cast<RiscvInstruction *>(
                        listInstr_[hash_idx][i].to_iface());
        if (instr->parse(cacheline_[0].buf32)) {
            break;
        }
        instr = NULL;
    }
    // Check compressed instructions:
    if (instr == NULL) {
        hash_idx = hash16(cacheline_[0].buf16[0]);
        for (unsigned i = 0; i < listInstr_[hash_idx].size(); i++) {
            instr = static_cast<RiscvInstruction *>(
                            listInstr_[hash_idx][i].to_iface());
            if (instr->parse(cacheline_[0].buf32)) {
                break;
            }
            instr = NULL;
        }
    }
    return instr;
}

void CpuRiver_Functional::generateIllegalOpcode() {
    raiseSignal(EXCEPTION_InstrIllegal);
    RISCV_error("Illegal instruction at 0x%08" RV_PRI64 "x", getPC());
}

void CpuRiver_Functional::trackContextStart() {
    trace_data_.pc = pc_.getValue().val;
    trace_data_.instr = cacheline_[0].buf32[0];
    trace_data_.memop_ena = false;

    if (reg_trace_file == 0) {
        return;
    }
    /** Save previous reg values to find modification after exec() */
    uint64_t *dst = portSavedRegs_.getpR64();
    uint64_t *src = portRegs_.getpR64();
    memcpy(dst, src, Reg_Total*sizeof(uint64_t));
}

void CpuRiver_Functional::trackContextEnd() {
    CpuGeneric::trackContextEnd();

    if (reg_trace_file == 0) {
        return;
    }
    char tstr[1024];

    riscv_disassembler(trace_data_.instr,
                       trace_data_.disasm,
                       sizeof(trace_data_.disasm));

    RISCV_sprintf(tstr, sizeof(tstr),
        "%9" RV_PRI64 "d: %08" RV_PRI64 "x: %s \n",
            step_cnt_ - 1,
            trace_data_.pc,
            trace_data_.disasm);
    (*reg_trace_file) << tstr;


    if (trace_data_.memop_ena && !trace_data_.write) {
        RISCV_sprintf(tstr, sizeof(tstr),
            "%20s [%08" RV_PRI64 "x] => %016" RV_PRI64 "x\n",
                "",
                trace_data_.memop_addr,
                trace_data_.data.val);
        (*reg_trace_file) << tstr;
    }


    uint64_t *prev = portSavedRegs_.getpR64();
    uint64_t *cur = portRegs_.getpR64();
    for (int i = 0; i < Reg_Total; i++) {
        if (prev[i] != cur[i]) {
            RISCV_sprintf(tstr, sizeof(tstr),
                "%20s %10s <= %016" RV_PRI64 "x\n",
                    "",
                    IREGS_NAMES[i],
                    cur[i]);
            (*reg_trace_file) << tstr;
        }
    }

    if (trace_data_.memop_ena && trace_data_.write) {
        RISCV_sprintf(tstr, sizeof(tstr),
            "%20s [%08" RV_PRI64 "x] <= %016" RV_PRI64 "x\n",
                "",
                trace_data_.memop_addr,
                trace_data_.data.val);
        (*reg_trace_file) << tstr;
    }

    reg_trace_file->flush();
}

void CpuRiver_Functional::raiseSignal(int idx) {
    if (idx < INTERRUPT_USoftware) {
        // Exception:
        csr_mcause_type cause;
        cause.value     = 0;
        cause.bits.irq  = 0;
        cause.bits.code = idx;
        if ((interrupt_pending_[0] & (1ull << EXCEPTION_InstrFault)) == 0) {
            // Wrong instruction address can generate others exceptions, ignore them
            portCSR_.write(CSR_mcause, cause.value);
            interrupt_pending_[idx >> 6] |= 1LL << (idx & 0x3F);
        }
    } else if (idx < SIGNAL_HardReset) {
        csr_mcause_type cause;
        cause.value     = 0;
        cause.bits.irq  = 1;
        cause.bits.code = idx - INTERRUPT_USoftware;
        portCSR_.write(CSR_mcause, cause.value);
        interrupt_pending_[idx >> 6] |= 1LL << (idx & 0x3F);
    } else if (idx == SIGNAL_HardReset) {
    } else {
        RISCV_error("Raise unsupported signal %d", idx);
    }
}

void CpuRiver_Functional::lowerSignal(int idx) {
    if (idx == SIGNAL_HardReset) {
    } else if (idx < SIGNAL_HardReset) {
        interrupt_pending_[idx >> 6] &= ~(1ull << (idx & 0x3F));
    } else {
        RISCV_error("Lower unsupported signal %d", idx);
    }
}

void CpuRiver_Functional::exceptionLoadInstruction(Axi4TransactionType *tr) {
    portCSR_.write(CSR_mbadaddr, tr->addr);
    raiseSignal(EXCEPTION_InstrFault);
}

void CpuRiver_Functional::exceptionLoadData(Axi4TransactionType *tr) {
    portCSR_.write(CSR_mbadaddr, tr->addr);
    raiseSignal(EXCEPTION_LoadFault);
}

void CpuRiver_Functional::exceptionStoreData(Axi4TransactionType *tr) {
    portCSR_.write(CSR_mbadaddr, tr->addr);
    raiseSignal(EXCEPTION_StoreFault);
}

uint64_t CpuRiver_Functional::readCSR(int idx) {
    if (idx == CSR_mtime) {
        return step_cnt_;
    }
    return portCSR_.read(idx).val;
}

void CpuRiver_Functional::writeCSR(int idx, uint64_t val) {
    switch (idx) {
    // Read-Only registers
    case CSR_misa:
    case CSR_mvendorid:
    case CSR_marchid:
    case CSR_mimplementationid:
    case CSR_mhartid:
        break;
    case CSR_mtime:
        break;
    default:
        portCSR_.write(idx, val);
    }
}

}  // namespace debugger

