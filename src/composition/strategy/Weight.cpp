#include <composition/metric/Performance.hpp>
#include <composition/strategy/Weight.hpp>

namespace composition::strategy {
using composition::metric::Performance;
using composition::metric::Weights;

Weight::Weight(Weights W, std::unordered_map<llvm::Function*, llvm::BlockFrequencyInfo*> BFI)
    : W(std::move(W)), BFI(std::move(BFI)) {}

Manifest* Weight::decideCycle(std::vector<Manifest*> manifests) { return decide(manifests); }

Manifest* Weight::decidePresentPreserved(std::vector<Manifest*> manifests) { return decide(manifests); }

Manifest* Weight::decide(std::vector<Manifest*> manifests) {
  std::unordered_map<Manifest*, Score> scores{};

  for (auto& m : manifests) {
    scores[m] = calculateScore(*m);
  }

  std::sort(manifests.begin(), manifests.end(), [scores](Manifest* m1, Manifest* m2) {
    auto m1It = scores.at(m1);
    auto m2It = scores.at(m2);

    return m1It < m2It;
  });

  return *manifests.begin();
}

std::set<llvm::BasicBlock*> getBlocks(llvm::Value* v) {
  std::set<llvm::BasicBlock*> blocks{};

  if (auto* F = llvm::dyn_cast<llvm::Function>(v)) {
    for (auto& BB : *F) {
      blocks.insert(&BB);
    }
  } else if (auto* BB = llvm::dyn_cast<llvm::BasicBlock>(v)) {
    blocks.insert(BB);
  } else if (auto* I = llvm::dyn_cast<llvm::Instruction>(v)) {
    if (I->getParent() != nullptr) {
      blocks.insert(I->getParent());
    }
  }
  return blocks;
}

std::set<llvm::BasicBlock*> getBlocks(std::set<llvm::Instruction*> vs) {
  std::set<llvm::BasicBlock*> allBlocks{};
  for (auto* v : vs) {
    auto newBlocks = getBlocks(v);
    allBlocks.insert(newBlocks.begin(), newBlocks.end());
  }
  return allBlocks;
}

std::set<llvm::BasicBlock*> getBlocks(std::set<llvm::Value*> vs) {
  std::set<llvm::BasicBlock*> allBlocks{};
  for (auto* v : vs) {
    auto newBlocks = getBlocks(v);
    allBlocks.insert(newBlocks.begin(), newBlocks.end());
  }
  return allBlocks;
}

Weight::Score Weight::calculateScore(Manifest& m) {
  auto cov = m.Coverage();
  auto undo = m.UndoValues();

  auto covBlocks = getBlocks(cov);
  auto undoBlocks = getBlocks(undo);

  size_t covBlocksPerformance = 0;
  size_t undoBlocksPerformance = 0;

  for (auto* BB : covBlocks) {
    covBlocksPerformance += Performance::getBlockFreq(BB, BFI.at(BB->getParent()), false);
  }
  for (auto* BB : undoBlocks) {
    undoBlocksPerformance += Performance::getBlockFreq(BB, BFI.at(BB->getParent()), false);
  }

  return W.explicitInstructionCoverage * cov.size() + W.basicBlockProfileCount * covBlocksPerformance -
         W.basicBlockProfileCount * undoBlocksPerformance - W.protectionCosts[m.name];
}
} // namespace composition::strategy