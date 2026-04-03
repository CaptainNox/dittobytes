/**
 * GNU General Public License, version 2.0.
 *
 * Copyright (c) 2025 Tijme Gommers (@tijme).
 *
 * This source code file is part of Dittobytes. Dittobytes is
 * licensed under GNU General Public License, version 2.0, and
 * you are free to use, modify, and distribute this file under
 * its terms. However, any modified versions of this file must
 * include this same license and copyright notice.
 */

#pragma once

#include <vector>
#include <functional>

/*
 * LLVM includes
 */

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/FormatVariadic.h"

/**
 * Modify `mov reg1, reg2` substitution options
 */
#include "options_amd64/TransformRegMovOptionAMD64_PUSH_POP.cpp"

using namespace llvm;

class TransformRegMovModule {
private:
    /**
     * Whether this class modified the machine function.
     */
    bool modified = false;

    /**
     * List of lambdas that invoke the `run` methods of AMD64 option classes.
     */
    std::vector<std::function<bool(MachineFunction&, bool)>> options_amd64;

    /**
     * List of lambdas that invoke the `run` methods of ARM64 option classes.
     */
    std::vector<std::function<bool(MachineFunction&, bool)>> options_arm64;

    bool isModuleEnabled() {
        // Currently always enabled for testing, implement env vars
        return true;
    }

public:

    /**
    * Constructor that initializes the list of substitution option classes.
    */
    TransformRegMovModule() {
        options_amd64 = {
            [&](MachineFunction &MF, bool modifyAll) { return TransformRegMovOptionAMD64_PUSH_POP().runOnMachineFunction(MF, false); }
        };
    }

    bool runOnMachineFunction(MachineFunction &MF) {
        if (!isModuleEnabled()) return false;

        auto architecture = MF.getTarget().getTargetTriple().getArch();

        int index = 0;

        switch (architecture) {
            case Triple::x86_64:
                modified = options_amd64[index](MF, true) || modified;
                break;
            case Triple::aarch64:
                break;
            default:
                report_fatal_error(formatv("TransformRegMovImmediatesModule failed due to unknown architecture: {0}.", architecture));
                break;
        }

        return modified;
    }
};
