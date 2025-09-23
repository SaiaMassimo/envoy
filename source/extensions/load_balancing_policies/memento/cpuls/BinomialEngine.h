#ifndef BINOMIAL_ENGINE_H
#define BINOMIAL_ENGINE_H

#include <string>
#include <cstdint>

/**
 * Interfaccia per una funzione di hash generica.
 */
class HashFunction {
public:
    virtual ~HashFunction() = default;
    virtual int64_t hash(const std::string& key) const = 0;
};

/**
 * Interfaccia per un motore di hashing basato su bucket.
 */
class BucketBasedEngine {
public:
    virtual ~BucketBasedEngine() = default;
    virtual int getBucket(const std::string& key) = 0;
    virtual int addBucket() = 0;
    virtual int removeBucket() = 0;
    virtual int size() const = 0;
};

/**
 * Implementazione dell'algoritmo BinomialHash.
 * https://arxiv.org/pdf/2406.19836
 */
class BinomialEngine : public BucketBasedEngine {
private:
    int m_size;
    int m_enclosingTreeFilter;
    int m_minorTreeFilter;
    const HashFunction& m_hashFunction;

    /**
     * Generatore lineare congruenziale per creare valori distribuiti uniformemente.
     */
    int64_t rehash(int64_t value, int seed) const;

    /**
     * Restituisce una posizione casuale all'interno dello stesso livello dell'albero del bucket fornito.
     */
    int relocateWithinLevel(int bucket, int64_t hash) const;

    /**
     * Funzione helper per trovare il bit più significativo (simile a Integer.highestOneBit in Java).
     */
    static int highestOneBit(int i);

public:
    /**
     * Costruttore con parametri.
     * @param size il numero di nodi di lavoro
     * @param hashFunction la funzione di hash da utilizzare
     */
    BinomialEngine(int size, const HashFunction& hashFunction);

    /**
     * Restituisce l'indice del bucket in cui la chiave data dovrebbe essere mappata.
     */
    int getBucket(const std::string& key) override;

    /**
     * Aumenta la dimensione del cluster di uno.
     */
    int addBucket() override;

    /**
     * Diminuisce la dimensione del cluster di uno.
     */
    int removeBucket() override;

    /**
     * Restituisce la dimensione del cluster.
     */
    int size() const override;

    /**
     * Restituisce il filtro enclosingTreeFilter.
     */
    int64_t enclosingTreeFilter() const;

    /**
     * Restituisce il filtro minorTreeFilter.
     */
    int64_t minorTreeFilter() const;
};

#endif // BINOMIAL_ENGINE_H