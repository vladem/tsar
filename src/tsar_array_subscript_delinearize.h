#ifndef TSAR_ARRAY_SUBSCRIPT_DELINEARIZE_H
#define TSAR_ARRAY_SUBSCRIPT_DELINEARIZE_H

#include <llvm/Pass.h>
#include <llvm/IR/Function.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <utility>
#include <set>
#include <map>
#include "tsar_bimap.h"
#include "tsar_pass.h"
#include "tsar_utility.h"
#include <llvm/Analysis/ScalarEvolutionExpressions.h>

#include "tsar_pass.h"
#include "tsar_transformation.h"
#include <llvm/ADT/Statistic.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Pass.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/PassAnalysisSupport.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/ADT/SmallSet.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/ADT/Sequence.h>
#include <llvm/IR/Type.h>


#include <vector>
#include <utility>

namespace llvm {
class Instruction;
class Function;
class Value;
class SCEV;
class ScalarEvolution;
class SCEVMulExpr;
}

namespace tsar {

  class Array {
  public:
    using ExprList = llvm::SmallVector<const llvm::SCEV *, 4>;

    /// Map from linearized index of array to its delinearized representation.
    /// Instruction does not access memory.
    struct Element {
      /// Pointer to an element of array.
      llvm::Value *Ptr;

      /// \brief Number of subscript expressions which allow to access this
      /// element.
      ///
      /// This is representation of offset (`Ptr-ArrayPtr`) after delinearization.
      /// If possible each subscript expression should be converted to AddRecExpr.
      /// In this case, it is possible to determine coefficients and loop.
      ExprList Subscript;

      bool isValid;

      Element(llvm::Value *Ptr, const ExprList &Subscript, bool isValid = true) :
        Ptr(Ptr),
        Subscript(Subscript),
        isValid(isValid) {}

      Element(llvm::Value *Ptr, ExprList &&Subscript, bool isValid = true) :
        Ptr(Ptr),
        Subscript(std::move(Subscript)),
        isValid(isValid) {}
    };

    using Elements = std::vector<Element>;
    using iterator = Elements::iterator;
    using const_iterator = Elements::const_iterator;

    Array(llvm::Value *BasePtr) :
      mBasePtr(BasePtr),
      mIsArrayValid(true){}

    // to access all elements
    iterator begin() {
      return mElements.begin();
    }

    iterator end() {
      return mElements.end();
    }

    const_iterator cbegin() const {
      return mElements.cbegin();
    }

    const_iterator cend() const {
      return mElements.cend();
    }

    std::size_t size() const {
      return mElements.size();
    }

    bool empty() const {
      return mElements.empty();
    }

    Element * getElement(std::size_t Idx) {
      if (Idx >= 0 && Idx < mElements.size()) {
        return &mElements[Idx];
      }
      return nullptr;
    }

    void pushBack(const Element &Access) {
      mElements.push_back(Access);
    }
    void pushBack(Element &&Access) {
      mElements.push_back(std::move(Access));
    }
 
    template<class... ArgTy> void emplaceBack(ArgTy &&...Args) {
      mElements.emplace_back(std::forward<ArgTy>(Args)...);
    }

    void clearDimensions() {
      mDims.clear();
    }

    void resizeDimensions(std::size_t Size) {
      mDims.resize(Size);
    }

    void setDimension(std::size_t DimIdx, const llvm::SCEV *Expr) {
      if (DimIdx >= 0 && DimIdx < mDims.size()) {
        mDims[DimIdx] = Expr;
      }
    }

    const llvm::SCEV * getDimension(std::size_t DimIdx) {
      if (DimIdx >= 0 && DimIdx < mDims.size()) {
        return mDims[DimIdx];
      }
      return nullptr;
    }

    const ExprList & getDimensions() const {
      return mDims;
    }

    std::size_t getDimensionsCount() const {
      return mDims.size();
    }

    bool isDimensionsEmpty() const {
      return mDims.empty();
    }

    llvm::Value * getBase() const {
      return mBasePtr;
    }

    bool isValid() const {
      return mIsArrayValid;
    }

    void setValid(bool Valid) {
      mIsArrayValid = Valid;
    }

  private:
    llvm::Value *mBasePtr;
    ExprList mDims;
    std::vector<Element> mElements;

    bool mIsArrayValid;

  };

  std::pair<const llvm::SCEV *, const llvm::SCEV *> findCoefficientsInSCEV(
    const llvm::SCEV *Expr, llvm::ScalarEvolution &SE);
}
namespace llvm {

template<> struct DenseMapInfo<tsar::Array> {
  static unsigned getHashValue(const tsar::Array &Arr) {
    return DenseMapInfo<Value *>::getHashValue(Arr.getBase());
  }

  static bool isEqual(const tsar::Array &LHS, const tsar::Array &RHS) {
    return DenseMapInfo<Value *>::isEqual(LHS.getBase(), RHS.getBase());
  }

  static unsigned getHashValue(llvm::Value *Arr) {
    return DenseMapInfo<Value *>::getHashValue(Arr);
  }

  static bool isEqual(llvm::Value *LHS , const tsar::Array &RHS) {
    return DenseMapInfo<Value *>::isEqual(LHS, RHS.getBase());
  }

  static tsar::Array getEmptyKey() {
    return tsar::Array(DenseMapInfo<Value *>::getEmptyKey());
  }

  static tsar::Array getTombstoneKey() {
    return tsar::Array(DenseMapInfo<Value *>::getTombstoneKey());
  }
};

class DelinearizeInfo {
public:
  using ArrayMapInfo = DenseMapInfo<tsar::Array>;
  using ArraySet = llvm::DenseSet<tsar::Array, ArrayMapInfo>;

  DelinearizeInfo(const ArraySet &AnalyzedArrays) :
    mArrays(AnalyzedArrays) {}

  DelinearizeInfo() = default;

  DelinearizeInfo(ArraySet &&AnalyzedArrays) :
    mArrays(std::move(AnalyzedArrays)) {}

  void fillElementsMap();

  std::pair<tsar::Array *, tsar::Array::Element *> findElement(llvm::Value *Ptr);
  const tsar::Array * findArray(llvm::Value *BasePtr) const;

  void clear() {
    mArrays.clear();
    mElements.clear();
  }

  const ArraySet & getAnalyzedArrays() const {
    return mArrays;
  }

private:
  ArraySet mArrays;
  llvm::DenseMap<llvm::Value *, std::pair<tsar::Array *, std::size_t>> mElements;
};
}

namespace llvm {
class ArraySubscriptDelinearizePass :
  public FunctionPass, private bcl::Uncopyable {
public:

  typedef std::map <Instruction *, SmallVector<
    std::pair<const SCEV *, const SCEV *>, 3>> ArraySubscriptDelinearizeInfo;

  typedef std::set<Instruction *> ArraySubscriptSet;

  static char ID;

  ArraySubscriptDelinearizePass() : llvm::FunctionPass(ID) {
    initializeArraySubscriptDelinearizePassPass(*llvm::PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override;

  void print(raw_ostream &OS, const Module *M) const override;

  const DelinearizeInfo & getDelinearizeInfo() const {
    return mDelinearizeInfo;
  }

private:
  DelinearizeInfo mDelinearizeInfo;
};
}


#endif //TSAR_ARRAY_SUBSCRIPT_DELINEARIZE_H