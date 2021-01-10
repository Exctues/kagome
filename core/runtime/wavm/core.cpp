/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "runtime/wavm/core.hpp"

namespace kagome::runtime::wavm {

  using common::Buffer;
  using primitives::AuthorityId;
  using primitives::Block;
  using primitives::BlockHeader;
  using primitives::Version;

  CoreWavm::CoreWavm(
      const std::shared_ptr<WasmProvider>& wasm_provider,
      std::shared_ptr<extensions::ExtensionFactory> extension_factory,
      std::shared_ptr<TrieStorageProvider> trie_storage_provider,
      std::shared_ptr<storage::changes_trie::ChangesTracker> changes_tracker,
      std::shared_ptr<blockchain::BlockHeaderRepository> header_repo)
      : WavmRuntimeApi(wasm_provider,
                       std::move(extension_factory),
                       std::move(trie_storage_provider)),
        changes_tracker_{std::move(changes_tracker)},
        header_repo_{std::move(header_repo)} {
    BOOST_ASSERT(changes_tracker_ != nullptr);
    BOOST_ASSERT(header_repo_ != nullptr);
  }

  outcome::result<primitives::Version> CoreWavm::version(
      const boost::optional<primitives::BlockHash> &block_hash) {
    if (block_hash) {
      OUTCOME_TRY(header, header_repo_->getBlockHeader(block_hash.value()));
      return executeAt<primitives::Version>(
          "Core_version", header.state_root, CallPersistency::EPHEMERAL);
    }
    return execute<primitives::Version>("Core_version",
                                        CallPersistency::EPHEMERAL);
  }

  outcome::result<void> CoreWavm::execute_block(
      const primitives::Block &block) {
    OUTCOME_TRY(parent, header_repo_->getBlockHeader(block.header.parent_hash));
    OUTCOME_TRY(changes_tracker_->onBlockChange(
        block.header.parent_hash,
        block.header.number - 1));  // parent's number
    return executeAt<void>("Core_execute_block",
                           parent.state_root,
                           CallPersistency::PERSISTENT,
                           block);
  }

  outcome::result<void> CoreWavm::initialise_block(const BlockHeader &header) {
    auto parent = header_repo_->getBlockHeader(header.parent_hash).value();
    OUTCOME_TRY(
        changes_tracker_->onBlockChange(header.parent_hash,
                                        header.number - 1));  // parent's number
    return executeAt<void>("Core_initialize_block",
                           parent.state_root,
                           CallPersistency::PERSISTENT,
                           header);
  }

  outcome::result<std::vector<AuthorityId>> CoreWavm::authorities(
      const primitives::BlockId &block_id) {
    return execute<std::vector<AuthorityId>>(
        "Core_authorities", CallPersistency::EPHEMERAL, block_id);
  }

}  // namespace kagome::runtime::wavm