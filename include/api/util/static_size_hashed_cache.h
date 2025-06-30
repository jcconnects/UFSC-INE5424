#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>

#include "api/util/debug.h"

/**
 * @brief A hash cache with a static size, using linear probing for collision
 * resolution.
 * @tparam V The type of the values to be stored in the cache.
 * @tparam N The static size of the cache.
 */
template <typename V, size_t N = 1000> class StaticSizeHashedCache {
public:
  /**
   * @brief Constructs a new StaticSizeHashedCache object.
   * Initializes the cache as empty.
   */
  StaticSizeHashedCache() { _occupied.fill(false); }

  /**
   * @brief Adds a key-value pair to the cache.
   * If the key already exists, its value is updated.
   * It uses linear probing to find a vacant spot if a collision occurs.
   * @param key The key to add.
   * @param value The value to associate with the key.
   * @throws std::runtime_error if the cache is full and a new key is being
   * added.
   */
  void add(long int key, V value) {
    db<StaticSizeHashedCache>(TRC) << "[StaticSizeHashedCache] add called for key: " << key << "\n";
    size_t index = hash(key);

    for (size_t i = 0; i < N; ++i) {
      size_t current_index = (index + i) % N;
      if (!_occupied[current_index] || _keys[current_index] == key) {
        _keys[current_index] = key;
        _values[current_index] = value;
        _occupied[current_index] = true;
        return;
      }
    }

    throw std::runtime_error("Cache is full");
  }

  /**
   * @brief Retrieves a pointer to the value associated with a given key.
   * It uses linear probing to find the key.
   * @param key The key to look for.
   * @return A pointer to the value if the key is found, otherwise nullptr.
   */
  V *get(long int key) {
    db<StaticSizeHashedCache>(TRC) << "[StaticSizeHashedCache] get called for key: " << key << "\n";
    size_t index = hash(key);

    for (size_t i = 0; i < N; ++i) {
      size_t current_index = (index + i) % N;
      if (!_occupied[current_index]) {
        return nullptr;
      }
      if (_keys[current_index] == key) {
        return &_values[current_index];
      }
    }

    return nullptr;
  }

  /**
   * @brief Determines if a key is already in the data structure.
   * It uses linear probing to find the key.
   * @param key The key to look for.
   * @return true if the key is in the cache, false otherwise.
   */
  bool contains(long int key) const {
    db<StaticSizeHashedCache>(TRC) << "[StaticSizeHashedCache] contains called for key: " << key << "\n";
    size_t index = hash(key);

    for (size_t i = 0; i < N; ++i) {
      size_t current_index = (index + i) % N;
      if (!_occupied[current_index]) {
        return false;
      }
      if (_keys[current_index] == key) {
        return true;
      }
    }

    return false;
  }

  /**
   * @brief Iterate over all occupied entries and apply a functor.
   * The functor receives (key, value&) as parameters.
   * @tparam F Callable type
   */
  template <typename F>
  void for_each(F&& fn) {
    for (size_t i = 0; i < N; ++i) {
      if (_occupied[i]) {
        fn(_keys[i], _values[i]);
      }
    }
  }

private:
  /**
   * @brief Hashes a key to an index in the arrays.
   * @param key The key to hash.
   * @return The calculated hash index.
   */
  size_t hash(long int key) const {
    return static_cast<unsigned long long>(key) % N;
  }

  std::array<long int, N> _keys;
  std::array<V, N> _values;
  std::array<bool, N> _occupied;
};
