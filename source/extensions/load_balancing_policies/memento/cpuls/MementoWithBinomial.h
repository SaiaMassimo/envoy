#ifndef MEMENTO_WITH_BINOMIAL_H
#define MEMENTO_WITH_BINOMIAL_H

#include <vector>
#include <string>

/**
 * Rappresenta la tabella di ricerca del set di sostituzione del memento.
 */
class MementoWithBinomial {
private:
    // Costanti
    static constexpr int MIN_TABLE_SIZE = 1 << 4;
    static constexpr int MAX_TABLE_SIZE = 1 << 30;

    /**
     * Rappresenta una voce nella tabella di ricerca.
     */
    struct Entry {
        int bucket;      // Il bucket rimosso.
        int replacer;    // Il bucket che sostituisce quello corrente.
        int prevRemoved; // Tiene traccia del bucket rimosso prima di quello corrente.
        Entry* next;     // Usato se più voci hanno lo stesso hashcode.

        Entry(int b, int r, int pr)
            : bucket(b), replacer(r), prevRemoved(pr), next(nullptr) {}
    };

    // Membri
    std::vector<Entry*> m_table; // Memorizza le informazioni sui bucket rimossi.
    int m_size;                  // Il numero di bucket rimossi.

    // Metodi privati
    void add(Entry* entry, std::vector<Entry*>& table);
    Entry* get(int bucket) const;
    Entry* remove(int bucket);
    void resizeTable(int newTableSize);
    void clearTable(std::vector<Entry*>& table);

public:
    /**
     * Costruttore.
     */
    MementoWithBinomial();

    /**
     * Distruttore per la pulizia della memoria.
     */
    ~MementoWithBinomial();

    // Impedisce copia e assegnazione per evitare problemi di gestione della memoria
    MementoWithBinomial(const MementoWithBinomial&) = delete;
    MementoWithBinomial& operator=(const MementoWithBinomial&) = delete;

    /**
     * Ricorda che il bucket dato è stato rimosso e sostituito.
     * @return il valore del nuovo ultimo bucket rimosso.
     */
    int remember(int bucket, int replacer, int prevRemoved);

    /**
     * Restituisce il sostituto del bucket se è stato rimosso, altrimenti -1.
     */
    int replacer(int bucket) const;

    /**
     * Ripristina il bucket dato rimuovendolo dalla memoria.
     * @return il nuovo ultimo bucket rimosso.
     */
    int restore(int bucket);

    /**
     * Restituisce true se il set di sostituzione è vuoto.
     */
    bool isEmpty() const;

    /**
     * Restituisce la dimensione del set di sostituzione.
     */
    int size() const;

    /**
     * Restituisce la capacità della tabella di ricerca.
     */
    int capacity() const;
};

#endif // MEMENTO_WITH_BINOMIAL_H