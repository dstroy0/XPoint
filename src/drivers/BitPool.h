// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/**
 * @file BitPool.h
 * @brief Flat bit-array pool backed by a uint32_t word array.
 *
 * Packs an arbitrary number of boolean flags into ceil(nBits/32) uint32_t words.
 * Access is by flat bit index; mapping from (row, col) coordinates to index is
 * the caller's responsibility.
 *
 * Two construction modes:
 * - Heap-owning: BitPool(nBits)            — allocates and frees internally.
 * - Non-owning:  BitPool(words, nBits)     — caller supplies the word array.
 *
 * get() and set() are defined inline here so the compiler can inline the
 * two-instruction bit read/write into the call site.
 */

#ifndef BITPOOL_H
#define BITPOOL_H

#include <stdint.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define XPOINT_PACKED __attribute__((packed))
#else
#define XPOINT_PACKED
#endif

class XPOINT_PACKED BitPool
{
  public:
    /**
     * @brief Words required to hold nBits bits.
     * @return 0 for nBits == 0, otherwise ceil(nBits / 32).
     */
    static uint16_t wordsFor(uint32_t nBits)
    {
        return (nBits == 0u) ? 0u : (uint16_t)((nBits + 31u) / 32u);
    }

    /**
     * @brief Heap-owning constructor.
     *
     * Allocates wordsFor(nBits) uint32_t words and zeros them.
     * The destructor frees the allocation.
     *
     * @param[in] nBits Total number of bits to store.
     */
    explicit BitPool(uint32_t nBits);

    /**
     * @brief Non-owning constructor.
     *
     * Uses a caller-supplied word array; the destructor does not free it.
     * The array must contain at least wordsFor(nBits) elements and must
     * outlive this object.
     *
     * @param[in] words Pointer to the caller-owned word array.
     * @param[in] nBits Total number of bits the pool represents.
     */
    BitPool(uint32_t *words, uint32_t nBits);

    ~BitPool();

    BitPool(const BitPool &) = delete;
    BitPool &operator=(const BitPool &) = delete;

    /** @brief True if the pool has a valid (non-null) word array. */
    bool valid() const
    {
        return _words != nullptr;
    }

    /**
     * @brief Read bit n.
     *
     * No bounds check — caller must ensure n < nBits.
     *
     * @return true if bit n is set, false otherwise.
     */
    bool get(uint32_t n) const
    {
        return (_words[n >> 5] >> (n & 0x1Fu)) & 1u;
    }

    /**
     * @brief Write bit n.
     *
     * No bounds check — caller must ensure n < nBits.
     *
     * @param[in] n Bit index.
     * @param[in] v Value to write.
     */
    void set(uint32_t n, bool v)
    {
        if (v)
            _words[n >> 5] |= (1u << (n & 0x1Fu));
        else
            _words[n >> 5] &= ~(1u << (n & 0x1Fu));
    }

    /** @brief Zero all words in the pool. */
    void clear();

  private:
    uint32_t *_words;
    uint16_t _nWords;
    bool _owns;
};

#endif // BITPOOL_H
