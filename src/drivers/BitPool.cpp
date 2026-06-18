// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

#include "BitPool.h"

BitPool::BitPool(uint32_t nBits) : _nWords(wordsFor(nBits)), _owns(true)
{
    _words = (_nWords > 0u) ? new uint32_t[_nWords]() : nullptr;
}

BitPool::BitPool(uint32_t *words, uint32_t nBits) : _words(words), _nWords(wordsFor(nBits)), _owns(false)
{
}

BitPool::~BitPool()
{
    if (_owns)
        delete[] _words;
}

void BitPool::clear()
{
    if (_words && _nWords)
        memset(_words, 0, (uint32_t)_nWords * sizeof(uint32_t));
}
