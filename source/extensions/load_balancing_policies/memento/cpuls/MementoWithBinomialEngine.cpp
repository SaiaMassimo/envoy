#include "MementoWithBinomialEngine.h"
#include <cmath>    // Per std::abs
#include <iostream> // Per std::cout

MementoWithBinomialEngine::MementoWithBinomialEngine(int size, const HashFunction& hashFunction)
    : m_memento(),
      m_binomialEngine(size, hashFunction),
      m_lastRemoved(size),
      m_hashFunction(hashFunction) {
}

int MementoWithBinomialEngine::getBucket(const std::string& key) {
    int b = m_binomialEngine.getBucket(key);

    /*
     * Controlliamo se il bucket è stato rimosso. Se non lo è, abbiamo finito.
     * Se il bucket è stato rimosso, il sostituto è >= 0, altrimenti è -1.
     */
    int replacer = m_memento.replacer(b);
    while (replacer >= 0) {
        /*
         * Se il bucket è stato rimosso, dobbiamo fare un re-hash e trovare
         * un nuovo bucket negli slot rimanenti. Per conoscere gli slot
         * rimanenti, guardiamo 'replacer' che rappresenta anche la dimensione
         * del set di lavoro quando il bucket è stato rimosso e otteniamo un
         * nuovo bucket in [0, replacer-1].
         */
        const int64_t h = std::abs(m_hashFunction.hash(key, b));
        b = static_cast<int>(h % replacer);

        /*
         * Se colpiamo un bucket rimosso, seguiamo le sostituzioni
         * fino a ottenere un bucket funzionante o un bucket nell'intervallo
         * [0, replacer-1].
         */
        int r = m_memento.replacer(b);
        while (r >= replacer) {
            b = r;
            r = m_memento.replacer(b);
        }

        /* Infine, aggiorniamo la voce per il ciclo esterno. */
        replacer = r;
    }
    return b;
}

int MementoWithBinomialEngine::addBucket() {
    const int bucket = m_lastRemoved;
    this->m_lastRemoved = m_memento.restore(bucket);

    if (bArraySize() <= bucket) {
        std::cout << "i add a new buket to binomial " << std::endl;
        m_binomialEngine.addBucket();
    }
    return bucket;
}

int MementoWithBinomialEngine::removeBucket(int bucket) {
    if (m_memento.isEmpty() && bucket == m_binomialEngine.size() - 1) {
    m_binomialEngine.removeBucket();
        m_lastRemoved = bucket;
        return bucket;
    }

    this->m_lastRemoved = m_memento.remember(
        bucket,
        size() - 1,
        m_lastRemoved
    );
    return bucket;
}

int MementoWithBinomialEngine::size() const {
    return m_binomialEngine.size() - m_memento.size();
}

int MementoWithBinomialEngine::bArraySize() const {