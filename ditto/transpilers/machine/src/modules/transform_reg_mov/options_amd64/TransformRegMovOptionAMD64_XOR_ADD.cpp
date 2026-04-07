
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#include "../../../../../shared/helpers/RandomHelper.cpp"

using namespace llvm;

class TransformRegMovOptionAMD64_XOR_ADD {
private:
    bool modified = false;

    unsigned getXorOpcode (const unsigned &SizeInBits) {
        switch (SizeInBits) {
            case 8: return X86::XOR8rr;
            case 16: return X86::XOR16rr;
            case 32: return X86::XOR32rr;
            case 64: return X86::XOR64rr;
            default: return 0;
        }
    }

    unsigned getAddOpcode(const unsigned &SizeInBits) {
        switch(SizeInBits) {
            case 8: return X86::ADD8rr;
            case 16: return X86::ADD16rr;
            case 32: return X86::ADD32rr;
            case 64: return X86::ADD64rr;
            default: return 0;
        }
    }

public:
    bool runOnMachineFunction(MachineFunction &MF, bool modifyAll) {
        const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
        const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();

        dbgs() << "        ↳ Running AMD64 module: TransformRegMov(option=XOR_ADD,modifyAll=" << modifyAll << ").\n";

        for (auto &MachineBasicBlock : MF) {
            for ( auto MachineInstruction = MachineBasicBlock.begin(); MachineInstruction != MachineBasicBlock.end(); ) {
                MachineInstr &Instruction = *MachineInstruction++;
                if (!Instruction.isMoveReg()) continue;
                if (!modifyAll && !RandomHelper::getChanceOneIn(2)) continue;

                Register srcReg = Instruction.getOperand(1).getReg();
                Register dstReg = Instruction.getOperand(0).getReg();

                // Determine the sizes of the registers
                const TargetRegisterClass &srcRC = *TRI->getMinimalPhysRegClass(srcReg);
                const TargetRegisterClass &dstRC = *TRI->getMinimalPhysRegClass(dstReg);
                const unsigned srcRegSize = TRI->getRegSizeInBits(srcRC).getFixedValue();
                const unsigned dstRegSize = TRI->getRegSizeInBits(dstRC).getFixedValue();

                unsigned xorOp = this->getXorOpcode(srcRegSize);
                unsigned addOp = this->getAddOpcode(dstRegSize);

                const DebugLoc &debugLoc = Instruction.getDebugLoc();
                BuildMI(MachineBasicBlock, Instruction, debugLoc, TII->get(xorOp), dstReg)
                    .addReg(dstReg, RegState::Undef)
                    .addReg(dstReg, RegState::Undef);

                BuildMI(MachineBasicBlock, Instruction, debugLoc, TII->get(addOp), dstReg)
                    .addReg(dstReg)
                    .addReg(srcReg);

                Instruction.eraseFromParent();

                dbgs() << "          ✓ Modified register mov operation with random option `XOR_ADD`.\n";
                this->modified = true;
            }
        }

        return this->modified;
    }

};
