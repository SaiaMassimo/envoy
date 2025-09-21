#ifndef MEMENTO_WITH_BINOMIAL_ENGINE_H
#define MEMENTO_WITH_BINOMIAL_ENGINE_H

#include "BinomialEngine.h"       // Include il file dell'engine binomiale
#include "MementoWithBinomial.h"  // Include il file del memento
#include <string>
#include <memory>

// NOTA: L'interfaccia HashFunction è estesa per includere un metodo con seed,
// come richiesto dalla logica di getBucket.
class HashFunction {
public:
    virtual ~HashFunction() = default;
    virtual int64_t hash(const std::string& key) const = 0;
    virtual int64_t hash(const std::string& key, int seed) const = 0;
};

class MementoWithBinomialEngine : public BucketBasedEngine {
private:
    MementoWithBinomial m_memento;
    BinomialEngine m_binomialEngine;
    int m_lastRemoved;
    const HashFunction& m_hashFunction;

public:
    /**
     * Crea un nuovo motore MementoWithBinomialEngine.
     *
     * @param size          numero iniziale di bucket di lavoro (0 < size)
     * @param hashFunction  funzione di hash da utilizzare
     */
    MementoWithBinomialEngine(int size, const HashFunction& hashFunction);

    /**
     * Restituisce il bucket in cui la chiave data dovrebbe essere mappata.
     */
    int getBucket(const std::string& key) override;

    /**
     * Aggiunge un nuovo bucket ripristinando l'ultimo rimosso.
     */
    int addBucket() override;

    /**
     * Rimuove un bucket.
     */
    int removeBucket(int bucket) override;

    /**
     * Restituisce la dimensione del set di lavoro.
     */
    int size() const override;

    /**
     * Restituisce la dimensione dell'array di bucket sottostante (dimensione del BinomialEngine).
     */
    int bArraySize() const;
};

#endif // MEMENTO_WITH_BINOMIAL_ENGINE_H