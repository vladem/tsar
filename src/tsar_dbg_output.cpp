//===---- tsar_dbg_output.cpp - Output functions for debugging --*- C++ -*-===//
//
//                       Traits Static Analyzer (SAPFOR)
//
//===----------------------------------------------------------------------===//
//
// This file implements a set of output functions specified in tsar_dbg_output.h
//
//===----------------------------------------------------------------------===//

#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Config/llvm-config.h>
#if (LLVM_VERSION_MAJOR < 4 && LLVM_VERSION_MINOR < 5)
#include <llvm/DebugInfo.h>
#else
#include <llvm/IR/DebugInfo.h>
#endif
#include "tsar_dbg_output.h"

using namespace llvm;

namespace tsar {
void printAllocaSource(llvm::raw_ostream &o, llvm::AllocaInst *AI) {
  assert(AI && "Alloca must not be null!");
  DbgDeclareInst *DDI = FindAllocaDbgDeclare(AI);
  if (DDI) {
#if (LLVM_VERSION_MAJOR < 4 && LLVM_VERSION_MINOR < 7)
    DIVariable DIVar(DDI->getVariable());
    o << DIVar.getLineNumber() << ": ";
#else
    const DILocalVariable &DIVar = *DDI->getVariable();
    o << DIVar.getLine() << ": ";
#endif
#if (LLVM_VERSION_MAJOR < 4 && LLVM_VERSION_MINOR < 5)
    DIType DITy(DIVar.getType());
    if (DITy.isDerivedType()) {
      DIDerivedType &DIDTy = static_cast<DIDerivedType &>(DITy);
      o << DIDTy.getTypeDerivedFrom().getName() << "* ";
    } else {
      o << DITy.getName() << " ";
    }
#else
    DITypeRef DITyRef(DIVar.getType());
#if (LLVM_VERSION_MAJOR < 4 && LLVM_VERSION_MINOR < 6)
    Value *DITyVal = (Value *) DITyRef;
#else
    Metadata *DITyVal = (Metadata *)DITyRef;
#endif
#if (LLVM_VERSION_MAJOR < 4 && LLVM_VERSION_MINOR < 7)
    if (DITyVal) {
      if (const MDNode *MD = dyn_cast<MDNode>(DITyVal)) {
        DIType DITy(MD);
        if (DITy.isDerivedType()) {
          DIDerivedType &DIDTy = static_cast<DIDerivedType &>(DITy);
          o << DIDTy.getTypeDerivedFrom().getName() << "* ";
        } else {
          o << DITy.getName() << " ";
        }
      } else {
        o << DITyRef.getName() << " ";
      }
    } else {
      o << DITyRef.getName() << " ";
    }
#else
    bool isDerived = false;
    if (DIDerivedType *DITy = dyn_cast_or_null<DIDerivedType>(DITyVal)) {
      DITyVal = DITy->getBaseType();
      isDerived = true;
    }
    if (DIType *DITy = dyn_cast_or_null<DIType>(DITyVal))
      o << DITy->getName() << " ";
    else
      o << "<unknown type>" << (isDerived ? "* " : " ");
#endif
#endif
    o << DIVar.getName() << ": ";
  }
  AI->print(o);
  o << "\n";
}

namespace {
void printLoops(llvm::raw_ostream &o, const Twine &Offset,
                LoopInfo::reverse_iterator ReverseI,
                LoopInfo::reverse_iterator ReverseEI) {
  for (; ReverseI != ReverseEI; --ReverseEI) {
    (Offset + "- ").print(o);
    DebugLoc loc = (*ReverseI)->getStartLoc();
#if (LLVM_VERSION_MAJOR < 4 && LLVM_VERSION_MINOR < 7)
    loc.print(getGlobalContext(), o);
#else
    loc.print(o);
#endif
    o << "\n";
    printLoops(o, Offset + "\t", (*ReverseI)->rbegin(), (*ReverseI)->rend());
  }
}
}

void printLoops(llvm::raw_ostream &o, const LoopInfo &LI) {
  printLoops(o, "", LI.rbegin(), LI.rend());
}
}