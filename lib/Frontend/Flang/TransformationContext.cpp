//=== TransformationContext.cpp - TSAR Transformation Engine (Flang) C++ *-===//
//
//                       Traits Static Analyzer (SAPFOR)
//
// Copyright 2020 DVM System Group
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
//
// This file implements Flang-based source level transformation engine which.
//
//===----------------------------------------------------------------------===//

#include "tsar/Frontend/Flang/TransformationContext.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/DebugInfoMetadata.h>

using namespace tsar;
using namespace llvm;
using namespace Fortran;

namespace {
using NameHierarchyMapT = std::map<SmallVector<std::string, 3>, std::string>;
using MangledToSourceMapT = llvm::StringMap<semantics::Symbol *>;

void collect(const Module &M, const DICompileUnit &CU,
    NameHierarchyMapT &NameHierarchy) {
  for (auto &F : M) {
    if (auto *DISub = F.getSubprogram(); DISub && DISub->getUnit() == &CU) {
      NameHierarchyMapT::key_type Key;
      DIScope *Scope{DISub};
      do {
        if (Scope->getName().empty())
          break;
        Key.push_back(std::string{Scope->getName()});
        Scope = Scope->getScope();
      } while (Scope != &CU && Scope);
      if (Scope != &CU)
        continue;
      std::reverse(Key.begin(), Key.end());
      NameHierarchy.try_emplace(Key, F.getName());
    }
  }
}

void match(semantics::Scope &Parent, NameHierarchyMapT::key_type &Names,
    const NameHierarchyMapT &NameHierarchy, MangledToSourceMapT &Map) {
  if (auto *S{Parent.symbol()}) {
    Names.push_back(S->name().ToString());
    if (Parent.kind() == semantics::Scope::Kind::Subprogram)
      if (auto I = NameHierarchy.find(Names); I != NameHierarchy.end())
        Map.try_emplace(I->second, S);
    for (auto &Child : Parent.children())
      match(Child, Names, NameHierarchy, Map);
    Names.pop_back();
  }
}
}

void FlangTransformationContext::initializeDemangler(
    const Module &M, const DICompileUnit &CU) {
  assert(hasInstance() && "Transformation context is not configured!");
  NameHierarchyMapT NameHierarchy;
  collect(M, CU, NameHierarchy);
  for (auto &Child : mContext.globalScope().children()) {
    NameHierarchyMapT::key_type Names;
    match(Child, Names, NameHierarchy, mGlobals);
  }
}