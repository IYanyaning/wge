/**
 * Copyright (c) 2024-2026 Stone Rhino and contributors.
 *
 * MIT License (http://opensource.org/licenses/MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#pragma once

#include <forward_list>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <vector>

namespace Wge {
namespace Common {

/**
 * Implement a simple hash table that provides the functionality of a general hash table and
 * provides locking by slot. Locking by slot is mainly used for multi-threaded synchronization,
 * to reduce the granularity of the lock of the hash table, and to split a large lock of the entire
 * hash table into N small locks to reduce lock contention and improve concurrency performance.
 * Note:
 * This class does not support rehash rearrangement function, using a fixed prime number (8191) as
 * the number of slots, we can adjust this value according to the business, but to reduce hash
 * collisions, it must be a prime number.
 * @tparam KEY the type of key
 * @tparam VALUE the type of value
 * @tparam slot_count the count of hash table slot, must be a prime number
 */
template <typename KEY, typename VALUE, size_t slot_count = 8192, typename Hash = std::hash<KEY>,
          typename KeyEqual = std::equal_to<KEY>>
class HashTable {
public:
  HashTable(Hash hash = Hash(), KeyEqual eq = KeyEqual(),
            size_t per_locker = 8 // Each locker manages 8 slots by default
            )
      : slots_(slot_count),
        per_locker_(per_locker == 0 ? 1 : (per_locker > slot_count ? slot_count : per_locker)),
        hash_(hash), eq_(eq) {
    static_assert(!std::is_same<KEY, size_t>::value, "KEY cannot be size_t");
    size_t locker_count = caclLockerCount(per_locker_);
    lockers_.reserve(locker_count);
    for (size_t i = 0; i < locker_count; i++) {
      lockers_.push_back(std::make_unique<std::shared_mutex>());
    }
  }

public:
  struct Node {
    template <typename K, typename V>
    Node(K&& k, V&& v) : key(std::forward<K>(k)), value(std::forward<V>(v)) {}

    KEY key;
    VALUE value;
  };

  // Provide standard library style interface
public:
  class iterator {
    friend class HashTable;

  public:
    iterator() : slot_index_(size_t(-1)) {}
    iterator(const iterator& iter) {
      slot_index_ = iter.slot_index_;
      node_iter_ = iter.node_iter_;
    }

  public:
    using list_iterator = typename std::forward_list<Node>::iterator;

    // Overload operators
  public:
    bool operator==(const iterator& iter) const {
      return slot_index_ == iter.slot_index_ && node_iter_ == iter.node_iter_;
    }
    bool operator!=(const iterator& iter) const { return !(*this == iter); }
    const list_iterator operator->() const { return node_iter_; }
    const list_iterator operator*() const { return node_iter_; }
    list_iterator operator->() { return node_iter_; }
    list_iterator operator*() { return node_iter_; }
    iterator& operator=(const iterator& iter) {
      slot_index_ = iter.slot_index_;
      node_iter_ = iter.node_iter_;
      return *this;
    }
    iterator& operator++() {
      ++node_iter_;
      return *this;
    }

  public:
    size_t slotIndex() const { return slot_index_; }
    list_iterator nodeIter() const { return node_iter_; }

  private:
    size_t slot_index_;
    list_iterator node_iter_;
  };

  /**
   * Find the node
   * @param key the key of the node
   * @return iterator the iterator of the node
   */
  template <typename K> iterator find(const K& key) {
    static_assert(!std::is_same_v<K, size_t>,
                  "K cannot be size_t to avoid ambiguity with index parameter");
    return internalFind(getSlotIndex(key), key);
  }

  /**
   * Find the node with known slot index (avoid redundant hash computation)
   * @param index the slot index
   * @param key the key of the node
   * @return iterator the iterator of the node
   */
  template <typename K> iterator findWithIndex(size_t index, const K& key) {
    static_assert(!std::is_same_v<K, size_t>,
                  "K cannot be size_t to avoid ambiguity with index parameter");
    return internalFind(index, key);
  }

  /**
   * Insert a node
   * @param key the key of the node
   * @param value the value of the node
   * @return iterator the iterator of the inserted node
   */
  template <typename K, typename V> iterator insert(K&& key, V&& value) {
    static_assert(!std::is_same_v<std::decay_t<K>, size_t>,
                  "K cannot be size_t to avoid ambiguity with index parameter");
    size_t index = getSlotIndex(key);
    iterator iter = internalFind(index, key);
    if (iter != end(index)) { // Use index-based end() to avoid redundant hash computation
      return iter;
    }

    slots_[index].emplace_front(std::forward<K>(key), std::forward<V>(value));

    iter.slot_index_ = index;
    iter.node_iter_ = slots_[index].begin();

    return iter;
  }

  /**
   * Erase the node
   * @param iter the iterator of the node that needs to be deleted
   */
  void erase(iterator iter) {
    if (iter.slot_index_ != size_t(-1)) {
      std::forward_list<Node>& list = slots_[iter.slot_index_];
      if (iter.nodeIter() != list.end()) {
        if (list.begin() == iter.nodeIter()) {
          list.pop_front();
        } else {
          for (auto nodeIter = list.begin(); nodeIter != list.end(); ++nodeIter) {
            auto next = nodeIter;
            ++next;
            if (next != list.end() && next == iter.nodeIter()) {
              list.erase_after(nodeIter);
              break;
            }
          }
        }
      }
    }
  }

  /**
   * Get the begin iterator of the specified key
   * @param key the specified key
   * @return iterator the begin iterator of the specified key
   */
  template <typename K> iterator begin(const K& key) { return begin(getSlotIndex(key)); }

  /**
   * Get the begin iterator of the specified index
   * @param index the slot index
   * @return iterator the begin iterator of the specified index
   */
  iterator begin(size_t index) {
    iterator iter;
    iter.slot_index_ = index;
    iter.node_iter_ = slots_[index].begin();
    return iter;
  }

  /**
   * Get the end iterator of the specified key
   * @param key the specified key
   * @return iterator the end iterator of the specified key
   */
  template <typename K> iterator end(const K& key) { return end(getSlotIndex(key)); }

  /**
   * Get the end iterator of the specified index
   * @param index the slot index
   * @return iterator the end iterator of the specified index
   */
  iterator end(size_t index) {
    iterator iter;
    iter.slot_index_ = index;
    iter.node_iter_ = slots_[index].end();
    return iter;
  }

  void swap(HashTable& table) {
    slots_.swap(table.slots_);
    lockers_.swap(table.lockers_);
    std::swap(per_locker_, table.per_locker_);
  }

  // Provide lock/unlock interface
public:
  /**
   * Lock the read lock of the group
   * @param key the key of the node
   */
  template <typename K> void readLock(const K& key) {
    lockers_[getLockerIndex(getSlotIndex(key))]->lock_shared();
  }
  void readLock(size_t index) {
    if (index < slots_.size()) {
      lockers_[getLockerIndex(index)]->lock_shared();
    }
  }
  template <typename K> bool tryReadLock(const K& key) {
    return lockers_[getLockerIndex(getSlotIndex(key))]->try_lock_shared();
  }
  bool tryReadLock(size_t index) {
    if (index < slots_.size()) {
      return lockers_[getLockerIndex(index)]->try_lock_shared();
    }

    return false;
  }

  /**
   * Unlock the read lock of the group
   * @param key the key of the node
   */
  template <typename K> void readUnlock(const K& key) {
    lockers_[getLockerIndex(getSlotIndex(key))]->unlock_shared();
  }
  void readUnlock(size_t index) {
    if (index < slots_.size()) {
      lockers_[getLockerIndex(index)]->unlock_shared();
    }
  }

  /**
   * Lock the write lock of the group
   * @param key the key of the node
   */
  template <typename K> void writeLock(const K& key) {
    lockers_[getLockerIndex(getSlotIndex(key))]->lock();
  }
  void writeLock(size_t index) {
    if (index < slots_.size()) {
      lockers_[getLockerIndex(index)]->lock();
    }
  }
  template <typename K> bool tryWriteLock(const K& key) {
    return lockers_[getLockerIndex(getSlotIndex(key))]->try_lock();
  }
  bool tryWriteLock(size_t index) {
    if (index < slots_.size()) {
      return lockers_[getLockerIndex(index)]->try_lock();
    }

    return false;
  }

  /**
   * Unlock the write lock of the group
   * @param key the key of the node
   */
  template <typename K> void writeUnlock(const K& key) {
    lockers_[getLockerIndex(getSlotIndex(key))]->unlock();
  }
  void writeUnlock(size_t index) {
    if (index < slots_.size()) {
      lockers_[getLockerIndex(index)]->unlock();
    }
  }

private:
  std::vector<std::forward_list<Node>> slots_;
  std::vector<std::unique_ptr<std::shared_mutex>> lockers_;
  size_t per_locker_;
  Hash hash_;
  KeyEqual eq_;

private:
  /**
   * Get the group index according to the key
   * @param key the key of the node
   * @return size_t the group index
   */
  template <typename K> size_t getSlotIndex(const K& key) const {
    return hash_(key) % slots_.size();
  }

  /**
   * Calculate how many locks are needed
   * @param slot_count the count of slots
   * @param per_locker  how many slots per lock
   * @return size_t the count of locks
   */
  size_t caclLockerCount(size_t per_locker) const {
    if (slot_count % per_locker == 0) {
      return slot_count / per_locker;
    } else {
      return (slot_count / per_locker) + 1;
    }
  }

  /**
   * Get the lock index of the corresponding slot
   * @param slot_index the index of the slot
   * @return size_t the lock index
   */
  size_t getLockerIndex(size_t slot_index) const { return slot_index / per_locker_; }

  /**
   * Find the node
   * @param slot_index the index of the slot
   * @param key the key of the node
   * @return iterator the iterator of the node
   */
  template <typename K> iterator internalFind(size_t slot_index, const K& key) const {
    iterator findIter;
    std::forward_list<Node>& list = const_cast<std::forward_list<Node>&>(slots_[slot_index]);
    findIter.slot_index_ = slot_index;
    findIter.node_iter_ = list.end();
    for (auto iter = list.begin(); iter != list.end(); ++iter) {
      if (eq_(iter->key, key)) {
        findIter.node_iter_ = iter;
        break;
      }
    }
    return findIter;
  }
};
} // namespace Common
} // namespace Wge