#include "wavemap/data_structure/volumetric/hashed_chunked_wavelet_octree_block.h"

#include <tracy/Tracy.hpp>

namespace wavemap {
void HashedChunkedWaveletOctreeBlock::threshold() {
  ZoneScoped;
  if (getNeedsThresholding()) {
    root_scale_coefficient_ = recursiveThreshold(chunked_ndtree_.getRootChunk(),
                                                 root_scale_coefficient_)
                                  .scale;
    setNeedsThresholding(false);
  }
}

void HashedChunkedWaveletOctreeBlock::prune() {
  ZoneScoped;
  if (getNeedsPruning()) {
    threshold();
    recursivePrune(chunked_ndtree_.getRootChunk());
    setNeedsPruning(false);
  }
}

void HashedChunkedWaveletOctreeBlock::setCellValue(const OctreeIndex& index,
                                                   FloatingPoint new_value) {
  setNeedsPruning();
  setNeedsThresholding();
  setLastUpdatedStamp();

  // Descend the tree chunk by chunk while decompressing, and caching chunk ptrs
  const MortonIndex morton_code = convert::nodeIndexToMorton(index);
  std::array<NodeChunkType*, kMaxChunkStackDepth> chunk_ptrs{};
  chunk_ptrs[0] = &chunked_ndtree_.getRootChunk();
  FloatingPoint current_value = root_scale_coefficient_;
  for (int chunk_top_height = tree_height_; index.height < chunk_top_height;
       chunk_top_height -= kChunkHeight) {
    // Get the current chunk
    const int chunk_depth = (tree_height_ - chunk_top_height) / kChunkHeight;
    NodeChunkType* const current_chunk = chunk_ptrs[chunk_depth];
    // Decompress level by level
    for (int parent_height = chunk_top_height;
         chunk_top_height - kChunkHeight < parent_height; --parent_height) {
      // Perform one decompression stage
      const LinearIndex relative_node_index =
          OctreeIndex::computeTreeTraversalDistance(
              morton_code, chunk_top_height, parent_height);
      const NdtreeIndexRelativeChild relative_child_index =
          OctreeIndex::computeRelativeChildIndex(morton_code, parent_height);
      current_value = Transform::backwardSingleChild(
          {current_value, current_chunk->nodeData(relative_node_index)},
          relative_child_index);
      // If we've reached the requested resolution, stop descending
      if (parent_height == index.height + 1) {
        break;
      }
    }
    if (chunk_top_height - kChunkHeight <= index.height + 1) {
      break;
    }

    // Descend to the next chunk
    const LinearIndex linear_child_index =
        OctreeIndex::computeLevelTraversalDistance(
            morton_code, chunk_top_height, chunk_top_height - kChunkHeight);
    if (current_chunk->hasChild(linear_child_index)) {
      chunk_ptrs[chunk_depth + 1] = current_chunk->getChild(linear_child_index);
    } else {
      chunk_ptrs[chunk_depth + 1] =
          current_chunk->allocateChild(linear_child_index);
    }
  }

  Coefficients::Parent coefficients{new_value - current_value, {}};
  for (int parent_height = index.height + 1; parent_height <= tree_height_;
       ++parent_height) {
    // Get the current chunk
    const int chunk_depth = (tree_height_ - parent_height) / kChunkHeight;
    NodeChunkType* current_chunk = chunk_ptrs[chunk_depth];
    // Get the index of the data w.r.t. the chunk
    const int chunk_top_height = tree_height_ - chunk_depth * kChunkHeight;
    const LinearIndex relative_node_index =
        OctreeIndex::computeTreeTraversalDistance(morton_code, chunk_top_height,
                                                  parent_height);
    // Compute and apply the transformed update
    const NdtreeIndexRelativeChild relative_child_index =
        OctreeIndex::computeRelativeChildIndex(morton_code, parent_height);
    coefficients =
        Transform::forwardSingleChild(coefficients.scale, relative_child_index);
    current_chunk->nodeData(relative_node_index) += coefficients.details;
    // TODO(victorr): Flag should skip last level
    current_chunk->nodeHasAtLeastOneChild(relative_node_index) = true;
  }
  root_scale_coefficient_ += coefficients.scale;
}

void HashedChunkedWaveletOctreeBlock::addToCellValue(const OctreeIndex& index,
                                                     FloatingPoint update) {
  setNeedsPruning();
  setNeedsThresholding();
  setLastUpdatedStamp();

  const MortonIndex morton_code = convert::nodeIndexToMorton(index);
  std::array<NodeChunkType*, kMaxChunkStackDepth> chunk_ptrs{};
  chunk_ptrs[0] = &chunked_ndtree_.getRootChunk();
  const int last_chunk_depth = (tree_height_ - index.height - 1) / kChunkHeight;
  for (int chunk_depth = 1; chunk_depth <= last_chunk_depth; ++chunk_depth) {
    const int parent_chunk_top_height =
        tree_height_ - (chunk_depth - 1) * kChunkHeight;
    const int chunk_top_height = tree_height_ - chunk_depth * kChunkHeight;
    const LinearIndex linear_child_index =
        OctreeIndex::computeLevelTraversalDistance(
            morton_code, parent_chunk_top_height, chunk_top_height);
    NodeChunkType* current_chunk = chunk_ptrs[chunk_depth - 1];
    if (current_chunk->hasChild(linear_child_index)) {
      chunk_ptrs[chunk_depth] = current_chunk->getChild(linear_child_index);
    } else {
      chunk_ptrs[chunk_depth] =
          current_chunk->allocateChild(linear_child_index);
    }
  }

  Coefficients::Parent coefficients{update, {}};
  for (int parent_height = index.height + 1; parent_height <= tree_height_;
       ++parent_height) {
    // Get the current chunk
    const int chunk_depth = (tree_height_ - parent_height) / kChunkHeight;
    NodeChunkType* current_chunk = chunk_ptrs[chunk_depth];
    // Get the index of the data w.r.t. the chunk
    const int chunk_top_height = tree_height_ - chunk_depth * kChunkHeight;
    const LinearIndex relative_node_index =
        OctreeIndex::computeTreeTraversalDistance(morton_code, chunk_top_height,
                                                  parent_height);
    // Compute and apply the transformed update
    const NdtreeIndexRelativeChild relative_child_index =
        OctreeIndex::computeRelativeChildIndex(morton_code, parent_height);
    coefficients =
        Transform::forwardSingleChild(coefficients.scale, relative_child_index);
    current_chunk->nodeData(relative_node_index) += coefficients.details;
    // TODO(victorr): Flag should skip last level
    current_chunk->nodeHasAtLeastOneChild(relative_node_index) = true;
  }
  root_scale_coefficient_ += coefficients.scale;
}

void HashedChunkedWaveletOctreeBlock::forEachLeaf(
    const BlockIndex& block_index,
    VolumetricDataStructureBase::IndexedLeafVisitorFunction visitor_fn) const {
  ZoneScoped;
  if (empty()) {
    return;
  }

  struct StackElement {
    const OctreeIndex node_index;
    const NodeChunkType& chunk;
    const Coefficients::Scale scale_coefficient{};
  };
  std::stack<StackElement> stack;
  stack.emplace(StackElement{{tree_height_, block_index},
                             chunked_ndtree_.getRootChunk(),
                             root_scale_coefficient_});
  while (!stack.empty()) {
    const OctreeIndex index = stack.top().node_index;
    const NodeChunkType& chunk = stack.top().chunk;
    const FloatingPoint scale_coefficient = stack.top().scale_coefficient;
    stack.pop();

    const MortonIndex morton_code = convert::nodeIndexToMorton(index);
    const int chunk_top_height =
        kChunkHeight * int_math::div_round_up(index.height, kChunkHeight);
    const LinearIndex relative_node_index =
        OctreeIndex::computeTreeTraversalDistance(morton_code, chunk_top_height,
                                                  index.height);
    const Coefficients::CoefficientsArray child_scale_coefficients =
        Transform::backward(
            {scale_coefficient, chunk.nodeData(relative_node_index)});
    const bool parent_has_nonzero_children =
        chunk.nodeHasAtLeastOneChild(relative_node_index);

    for (NdtreeIndexRelativeChild relative_child_idx = 0;
         relative_child_idx < OctreeIndex::kNumChildren; ++relative_child_idx) {
      const OctreeIndex child_index =
          index.computeChildIndex(relative_child_idx);
      const FloatingPoint child_scale_coefficient =
          child_scale_coefficients[relative_child_idx];

      const bool child_in_same_chunk = child_index.height % kChunkHeight != 0;
      if (child_in_same_chunk) {
        if (parent_has_nonzero_children) {
          stack.emplace(
              StackElement{child_index, chunk, child_scale_coefficient});
        } else {
          visitor_fn(child_index, child_scale_coefficient);
        }
      } else {
        const MortonIndex child_morton =
            convert::nodeIndexToMorton(child_index);
        const LinearIndex relative_child_chunk_index =
            OctreeIndex::computeLevelTraversalDistance(
                child_morton, chunk_top_height, child_index.height);
        if (chunk.hasChild(relative_child_chunk_index)) {
          const NodeChunkType& child_chunk =
              *chunk.getChild(relative_child_chunk_index);
          stack.emplace(
              StackElement{child_index, child_chunk, child_scale_coefficient});
        } else {
          visitor_fn(child_index, child_scale_coefficient);
        }
      }
    }
  }
}

HashedChunkedWaveletOctreeBlock::RecursiveThresholdReturnValue
HashedChunkedWaveletOctreeBlock::recursiveThreshold(  // NOLINT
    HashedChunkedWaveletOctreeBlock::NodeChunkType& chunk,
    FloatingPoint scale_coefficient) {
  constexpr auto tree_size = [](auto tree_height) {
    return static_cast<int>(
        tree_math::perfect_tree::num_total_nodes_fast<kDim>(tree_height));
  };
  constexpr auto level_size = [](auto level_height) {
    return static_cast<int>(
        tree_math::perfect_tree::num_leaf_nodes<kDim>(level_height));
  };

  // Decompress
  std::array<Coefficients::Scale, tree_size(kChunkHeight + 1)>
      chunk_scale_coefficients{};
  std::bitset<tree_size(kChunkHeight + 1)> is_nonzero_child{};
  chunk_scale_coefficients[0] = scale_coefficient;
  for (int level_idx = 0; level_idx < kChunkHeight; ++level_idx) {
    const int first_idx = tree_size(level_idx);
    const int last_idx = tree_size(level_idx + 1);
    for (int relative_idx = 0; relative_idx < level_size(level_idx + 1);
         ++relative_idx) {
      const int src_idx = first_idx + relative_idx;
      const Coefficients::CoefficientsArray child_scale_coefficients =
          Transform::backward(
              {chunk_scale_coefficients[src_idx], chunk.nodeData(src_idx)});
      const int first_dest_idx = last_idx + 8 * relative_idx;
      std::move(child_scale_coefficients.begin(),
                child_scale_coefficients.end(),
                chunk_scale_coefficients.begin() + first_dest_idx);
    }
  }

  // Threshold
  const int first_leaf_idx = tree_size(kChunkHeight);
  for (LinearIndex child_idx = 0; child_idx < NodeChunkType::kNumChildren;
       ++child_idx) {
    const LinearIndex array_idx = first_leaf_idx + child_idx;
    if (chunk.hasChild(child_idx)) {
      NodeChunkType& child_chunk = *chunk.getChild(child_idx);
      const auto rv =
          recursiveThreshold(child_chunk, chunk_scale_coefficients[array_idx]);
      chunk_scale_coefficients[array_idx] = rv.scale;
      is_nonzero_child[array_idx] = rv.is_nonzero_child;
    } else {
      chunk_scale_coefficients[array_idx] = std::clamp(
          chunk_scale_coefficients[array_idx], min_log_odds_, max_log_odds_);
    }
  }

  // Compress
  for (int level_idx = kChunkHeight - 1; 0 <= level_idx; --level_idx) {
    const int first_idx = tree_size(level_idx);
    const int last_idx = tree_size(level_idx + 1);
    for (int relative_idx = level_size(level_idx + 1) - 1; 0 <= relative_idx;
         --relative_idx) {
      const int first_src_idx = last_idx + 8 * relative_idx;
      Coefficients::CoefficientsArray scale_coefficients_subset{};
      std::move(chunk_scale_coefficients.begin() + first_src_idx,
                chunk_scale_coefficients.begin() + first_src_idx + 8,
                scale_coefficients_subset.begin());
      bool has_nonzero_child = false;
      for (int src_idx = first_src_idx; src_idx < first_src_idx + 8;
           ++src_idx) {
        has_nonzero_child |= is_nonzero_child[src_idx];
      }

      const int dst_idx = first_idx + relative_idx;
      auto [new_scale, new_details] =
          Transform::forward(scale_coefficients_subset);
      chunk_scale_coefficients[dst_idx] = new_scale;
      chunk.nodeData(dst_idx) = new_details;
      chunk.nodeHasAtLeastOneChild(dst_idx) = has_nonzero_child;
      is_nonzero_child[dst_idx] =
          has_nonzero_child || data_utils::is_nonzero(new_details);
    }
  }

  return {chunk_scale_coefficients[0], is_nonzero_child[0]};
}

void HashedChunkedWaveletOctreeBlock::recursivePrune(  // NOLINT
    HashedChunkedWaveletOctreeBlock::NodeChunkType& chunk) {
  constexpr FloatingPoint kNonzeroCoefficientThreshold = 1e-3f;
  bool has_at_least_one_child = false;
  for (LinearIndex linear_child_idx = 0;
       linear_child_idx < NodeChunkType::kNumChildren; ++linear_child_idx) {
    if (chunk.hasChild(linear_child_idx)) {
      NodeChunkType& child_chunk = *chunk.getChild(linear_child_idx);
      recursivePrune(child_chunk);
      if (!child_chunk.hasChildrenArray() &&
          !child_chunk.hasNonzeroData(kNonzeroCoefficientThreshold)) {
        chunk.deleteChild(linear_child_idx);
      } else {
        has_at_least_one_child = true;
      }
    }
  }
  if (!has_at_least_one_child) {
    chunk.deleteChildrenArray();
  }
}
}  // namespace wavemap
