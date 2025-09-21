#include "BinomialEngine.h"

// Funzione helper per trovare il bit più significativo (simile a Integer.highestOneBit in Java)
int BinomialEngine::highestOneBit(int i) {
    if (i <= 0) return 0;
    int hob = 1;
    while (hob <= i) {
        hob <<= 1;
    }
    return hob >> 1;
}

BinomialEngine::BinomialEngine(int size, const HashFunction& hashFunction)
    : m_size(size), m_hashFunction(hashFunction) {

    int hob = highestOneBit(m_size);
    if (m_size > hob) {
        hob <<= 1;
    }

    this->m_enclosingTreeFilter = hob - 1;
    this->m_minorTreeFilter = this->m_enclosingTreeFilter >> 1;
}

int64_t BinomialEngine::rehash(int64_t value, int seed) const {
    const uint64_t h = 2862933555777941757ULL * value + 1;
    // Usa uint64_t per garantire lo shift logico a destra (come >>> in Java)
    return (h * h * seed) >> 32;
}

int BinomialEngine::relocateWithinLevel(int bucket, int64_t hash) const {
    if (bucket < 2) {
        return bucket;
    }

    const int levelBaseIndex = highestOneBit(bucket);
    const int levelFilter = levelBaseIndex - 1;

    const int64_t levelHash = rehash(hash, levelFilter);
    const int levelIndex = static_cast<int>(levelHash) & levelFilter;

    return levelBaseIndex + levelIndex;
}

int BinomialEngine::getBucket(const std::string& key) {
    if (m_size < 2) {
        return 0;
    }

    const int64_t hash = m_hashFunction.hash(key);
    int bucket = static_cast<int>(hash) & m_enclosingTreeFilter;
    bucket = relocateWithinLevel(bucket, hash);

    if (bucket < m_size) {
        return bucket;
    }

    int64_t h = hash;
    for (int i = 0; i < 4; ++i) {
        h = rehash(h, m_enclosingTreeFilter);
        bucket = static_cast<int>(h) & m_enclosingTreeFilter;

        if (bucket <= m_minorTreeFilter) {
            break;
        }

        if (bucket < m_size) {
            return bucket;
        }
    }

    bucket = static_cast<int>(hash) & m_minorTreeFilter;
    return relocateWithinLevel(bucket, hash);
}

int BinomialEngine::addBucket() {
    const int newBucket = m_size;
    if (++m_size > m_enclosingTreeFilter + 1) { // +1 perché il filtro è N-1
        this->m_enclosingTreeFilter = (this->m_enclosingTreeFilter << 1) | 1;
        this->m_minorTreeFilter = (this->m_minorTreeFilter << 1) | 1;
    }
    return newBucket;
}

int BinomialEngine::removeBucket(int b) {
    if (--m_size <= m_minorTreeFilter + 1) { // +1 perché il filtro è N-1
        this->m_minorTreeFilter >>= 1;
        this->m_enclosingTreeFilter >>= 1;
    }
    return m_size;
}

int BinomialEngine::size() const {
    return m_size;
}

int64_t BinomialEngine::enclosingTreeFilter() const {
    return m_enclosingTreeFilter;
}