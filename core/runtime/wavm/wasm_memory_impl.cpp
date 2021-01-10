/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "runtime/wavm/wasm_memory_impl.hpp"

#include "runtime/wasm_result.hpp"


namespace kagome::runtime::wavm {

  WasmMemoryImpl::WasmMemoryImpl(WAVM::Runtime::Memory *memory)
      : memory_(memory),
        offset_{1},
        logger_{common::createLogger("WavmMemory")} {}

  void WasmMemoryImpl::reset() {
    offset_ = 1;
    allocated_.clear();
    deallocated_.clear();
    WAVM::Runtime::unmapMemoryPages(memory_, 0, size() / MEMORY_PAGE_SIZE);
  }

  WasmPointer WasmMemoryImpl::allocate(WasmSize size) {
    if (size == 0) {
      return 0;
    }
    const auto ptr = offset_;
    const auto new_offset = roundUpAlign(ptr + size);  // align

    // BOOST_ASSERT(allocated_.find(ptr) == allocated_.end());
    if (new_offset < static_cast<const uint32_t>(ptr)) {  // overflow
      logger_->error(
          "overflow occured while trying to allocate {} bytes at offset 0x{:x}",
          size,
          offset_);
      return 0;
    }
    if (new_offset <= this->size()) {
      offset_ = new_offset;
      allocated_[ptr] = size;
      return ptr;
    }

    return freealloc(size);
  }

  boost::optional<WasmSize> WasmMemoryImpl::deallocate(WasmPointer ptr) {
    const auto it = allocated_.find(ptr);
    if (it == allocated_.end()) {
      return boost::none;
    }
    const auto size = it->second;

    allocated_.erase(ptr);
    deallocated_[ptr] = size;
    return size;
  }

  WasmPointer WasmMemoryImpl::freealloc(WasmSize size) {
    auto ptr = findContaining(size);
    if (ptr == 0) {
      // if did not find available space among deallocated memory chunks, then
      // grow memory and allocate in new space
      return growAlloc(size);
    }
    allocated_[ptr] = deallocated_[ptr];
    deallocated_.erase(ptr);
    return ptr;
  }

  WasmPointer WasmMemoryImpl::findContaining(WasmSize size) {
    auto min_value = std::numeric_limits<WasmPointer>::max();
    WasmPointer min_key = 0;
    for (const auto &[key, value] : deallocated_) {
      if (min_value <= 0) {
        return 0;
      }
      if (value < static_cast<uint32_t>(min_value) and value >= size) {
        min_value = value;
        min_key = key;
      }
    }
    return min_key;
  }

  WasmPointer WasmMemoryImpl::growAlloc(WasmSize size) {
    // check that we do not exceed max memory size
    if (static_cast<uint32_t>(offset_) > kMaxMemorySize - size) {
      logger_->error(
          "Memory size exceeded when growing it on {} bytes, offset was 0x{:x}",
          size,
          offset_);
      return 0;
    }
    // try to increase memory size up to offset + size * 4 (we multiply by 4
    // to have more memory than currently needed to avoid resizing every time
    // when we exceed current memory)
    if (static_cast<uint32_t>(offset_) < kMaxMemorySize - size * 4) {
      resize(offset_ + size * 4);
    } else {
      // if we can't increase by size * 4 then increase memory size by
      // provided size
      resize(offset_ + size);
    }
    return allocate(size);
  }

  int8_t WasmMemoryImpl::load8s(WasmPointer addr) const {
    return load<int8_t>(addr);
  }
  uint8_t WasmMemoryImpl::load8u(WasmPointer addr) const {
    return load<uint8_t>(addr);
  }
  int16_t WasmMemoryImpl::load16s(WasmPointer addr) const {
    return load<int16_t>(addr);
  }
  uint16_t WasmMemoryImpl::load16u(WasmPointer addr) const {
    return load<uint16_t>(addr);
  }
  int32_t WasmMemoryImpl::load32s(WasmPointer addr) const {
    return load<int32_t>(addr);
  }
  uint32_t WasmMemoryImpl::load32u(WasmPointer addr) const {
    return load<uint32_t>(addr);
  }
  int64_t WasmMemoryImpl::load64s(WasmPointer addr) const {
    return load<int64_t>(addr);
  }
  uint64_t WasmMemoryImpl::load64u(WasmPointer addr) const {
    return load<uint64_t>(addr);
  }
  std::array<uint8_t, 16> WasmMemoryImpl::load128(WasmPointer addr) const {
    return load<std::array<uint8_t, 16>>(addr);
  }

  common::Buffer WasmMemoryImpl::loadN(kagome::runtime::WasmPointer addr,
                                       kagome::runtime::WasmSize n) const {
    // TODO (kamilsa) PRE-98: check if we do not go outside of memory
    common::Buffer res;
    res.reserve(n);
    for (auto i = addr; i < addr + n; i++) {
      res.putUint8(load<uint8_t>(i));
    }
    return res;
  }

  std::string WasmMemoryImpl::loadStr(kagome::runtime::WasmPointer addr,
                                      kagome::runtime::WasmSize n) const {
    std::string res;
    res.reserve(n);
    for (auto i = addr; i < addr + n; i++) {
      res.push_back(load<uint8_t>(i));
    }
    return res;
  }

  void WasmMemoryImpl::store8(WasmPointer addr, int8_t value) {
    store<int8_t>(addr, value);
  }
  void WasmMemoryImpl::store16(WasmPointer addr, int16_t value) {
    store<int16_t>(addr, value);
  }
  void WasmMemoryImpl::store32(WasmPointer addr, int32_t value) {
    store<int32_t>(addr, value);
  }
  void WasmMemoryImpl::store64(WasmPointer addr, int64_t value) {
    store<int64_t>(addr, value);
  }
  void WasmMemoryImpl::store128(WasmPointer addr,
                                const std::array<uint8_t, 16> &value) {
    store<std::array<uint8_t, 16>>(addr, value);
  }
  void WasmMemoryImpl::storeBuffer(kagome::runtime::WasmPointer addr,
                                   gsl::span<const uint8_t> value) {
    // TODO (kamilsa) PRE-98: check if we do not go outside of memory
    // boundaries, 04.04.2019
    for (size_t i = addr, j = 0; i < addr + static_cast<size_t>(value.size());
         i++, j++) {
      store8(i, value[j]);
    }
  }

  WasmSpan WasmMemoryImpl::storeBuffer(gsl::span<const uint8_t> value) {
    auto wasm_pointer = allocate(value.size());
    if (wasm_pointer == 0) {
      return 0;
    }
    storeBuffer(wasm_pointer, value);
    return WasmResult(wasm_pointer, value.size()).combine();
  }

}  // namespace kagome::runtime::wavm