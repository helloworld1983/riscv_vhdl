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

#ifndef __DEBUGGER_CPU_ARM7_FUNCTIONAL_H__
#define __DEBUGGER_CPU_ARM7_FUNCTIONAL_H__

#include "arm-isa.h"
#include "instructions.h"
#include "generic/cpu_generic.h"
#include "coreservices/icpuarm.h"
#include "cmds/cmd_br_arm7.h"
#include "cmds/cmd_reg_arm7.h"
#include "cmds/cmd_regs_arm7.h"

namespace debugger {

class CpuCortex_Functional : public CpuGeneric,
                             public ICpuArm {
 public:
     explicit CpuCortex_Functional(const char *name);
     virtual ~CpuCortex_Functional();

    /** IService interface */
    virtual void postinitService();
    virtual void predeleteService();

    /** IHap */
    virtual void hapTriggered(IFace *isrc, EHapType type, const char *descr);

    /** IResetListener interface */
    virtual void reset(IFace *isource);

    /** ICpuGeneric interface */
    virtual void raiseSignal(int idx);
    virtual void lowerSignal(int idx);
    virtual void raiseSoftwareIrq();
    virtual uint64_t getIrqAddress(int idx) { return 0; }

    /** ICpuArm */
    virtual void setInstrMode(EInstructionModes mode) {
        const uint32_t MODE[InstrModes_Total] = {0u, 1u};
        p_psr_->u.T = MODE[mode];
    }
    virtual EInstructionModes getInstrMode() {
        const EInstructionModes MODE[2] = {ARM_mode, THUMB_mode};
        return MODE[p_psr_->u.T];
    }
    virtual uint32_t getZ() { return p_psr_->u.Z; }
    virtual void setZ(uint32_t z) { p_psr_->u.Z = z; }
    virtual uint32_t getC() { return p_psr_->u.C; }
    virtual void setC(uint32_t c) { p_psr_->u.C = c; }
    virtual uint32_t getN() { return p_psr_->u.N; }
    virtual void setN(uint32_t n) { p_psr_->u.N = n; }
    virtual uint32_t getV() { return p_psr_->u.V; }
    virtual void setV(uint32_t v) { p_psr_->u.V = v; }

    // Common River methods shared with instructions:
    uint64_t *getpRegs() { return portRegs_.getpR64(); }

 protected:
    /** CpuGeneric common methods */
    virtual EEndianessType endianess() { return LittleEndian; }
    virtual GenericInstruction *decodeInstruction(Reg64Type *cache);
    virtual void generateIllegalOpcode();
    virtual void handleTrap();
    /** Tack Registers changes during execution */
    virtual void trackContextStart();
    /** // Stop tracking and write trace file */
    virtual void trackContextEnd() override;

    void addArm7tmdiIsa();
    unsigned addSupportedInstruction(ArmInstruction *instr);
    uint32_t hash32(uint32_t val) { return (val >> 24) & 0xf; }

 private:
    AttributeType defaultMode_;
    AttributeType vendorID_;
    AttributeType vectorTable_;

    static const int INSTR_HASH_TABLE_SIZE = 1 << 4;
    AttributeType listInstr_[INSTR_HASH_TABLE_SIZE];
    ArmInstruction *isaTableArmV7_[ARMV7_Total];

    GenericReg64Bank portRegs_;
    GenericReg64Bank portSavedRegs_;
    ProgramStatusRegsiterType *p_psr_;

    char errmsg_[256];

    CmdBrArm *pcmd_br_;
    CmdRegArm *pcmd_reg_;
    CmdRegsArm *pcmd_regs_;
};

DECLARE_CLASS(CpuCortex_Functional)

}  // namespace debugger

#endif  // __DEBUGGER_CPU_RISCV_FUNCTIONAL_H__
