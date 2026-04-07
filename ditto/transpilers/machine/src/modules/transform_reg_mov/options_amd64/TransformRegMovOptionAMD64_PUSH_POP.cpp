
/*
 * LLVM includes
 */

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/X86/X86.h"
#include "llvm/Target/X86/X86InstrInfo.h"
#include "llvm/Target/X86/X86TargetMachine.h"
#include "llvm/Support/Alignment.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

#include "../../../../../shared/helpers/RandomHelper.cpp"

using namespace llvm;

class TransformRegMovOptionAMD64_PUSH_POP {
private:
    bool modified = false;

    unsigned getPushOpcode(const unsigned &SizeInBits) {
        switch (SizeInBits) {
            case 16: return X86::PUSH16r;
            case 32: return X86::PUSH32r;
            case 64: return X86::PUSH64r;
            default: return 0;
        }
    }

    unsigned getPopOpcode(const unsigned &SizeInBits) {
        switch (SizeInBits) {
            case 16: return X86::POP16r;
            case 32: return X86::POP32r;
            case 64: return X86::POP64r;
            default: return 0;
        }
    }

public:

    bool runOnMachineFunction(MachineFunction &MF, bool modifyAll) {
        const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
        const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();

        dbgs() << "        ↳ Running AMD64 module: TransformRegMov(option=PUSH_POP,modifyAll=" << modifyAll << ").\n";


        for (auto &MachineBasicBlock : MF) {
            for ( auto MachineInstruction = MachineBasicBlock.begin(); MachineInstruction != MachineBasicBlock.end(); ) {
                MachineInstr &Instruction = *MachineInstruction++;
                if (!Instruction.isMoveReg()) continue;
                if (!modifyAll && !RandomHelper::getChanceOneIn(2)) continue;

                dbgs() << "          ↳ Found AMD64 register move instruction!: ";
                Instruction.print(dbgs());

                Register srcReg = Instruction.getOperand(1).getReg();
                Register dstReg = Instruction.getOperand(0).getReg();

                // Determine the sizes of the registers
                const TargetRegisterClass &srcRC = *TRI->getMinimalPhysRegClass(srcReg);
                const TargetRegisterClass &dstRC = *TRI->getMinimalPhysRegClass(dstReg);
                const unsigned srcRegSize = TRI->getRegSizeInBits(srcRC).getFixedValue();
                const unsigned dstRegSize = TRI->getRegSizeInBits(dstRC).getFixedValue();

                unsigned opPush = this->getPushOpcode(srcRegSize);
                unsigned opPop = this->getPopOpcode(dstRegSize);
                if (opPush == 0 || opPop == 0) continue;

                const DebugLoc &debugLoc = Instruction.getDebugLoc();
                BuildMI(MachineBasicBlock, Instruction, debugLoc, TII->get(opPush)).addReg(srcReg);
                BuildMI(MachineBasicBlock, Instruction, debugLoc, TII->get(opPop)).addReg(dstReg, RegState::Define);

                Instruction.eraseFromParent();

                dbgs() << "          ✓ Modified register mov operation with random option `PUSH_POP`.\n";
                this->modified = true;
            }
        }

        return this->modified;
    }
};
