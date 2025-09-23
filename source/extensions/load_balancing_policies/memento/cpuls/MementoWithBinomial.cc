#include "MementoWithBinomial.h"
#include <stdexcept>

// --- Costruttore e Distruttore ---

MementoWithBinomial::MementoWithBinomial() : m_size(0) {
    m_table.resize(MIN_TABLE_SIZE, nullptr);
}

MementoWithBinomial::~MementoWithBinomial() {
    clearTable(m_table);
}

// --- Metodi Pubblici ---

int MementoWithBinomial::remember(int bucket, int replacer, int prevRemoved) {
    Entry* entry = new Entry(bucket, replacer, prevRemoved);
    add(entry, m_table);
    ++m_size;

    if (m_size > capacity()) {
        resizeTable(m_table.size() * 2);
    }

    return bucket;
}

int MementoWithBinomial::replacer(int bucket) const {
    const Entry* entry = get(bucket);
    return entry != nullptr ? entry->replacer : -1;
}

int MementoWithBinomial::restore(int bucket) {
    if (isEmpty()) {
        return bucket + 1;
    }

    Entry* entry = remove(bucket);
    if (entry == nullptr) {
        // Comportamento non definito nel codice originale, ma potrebbe accadere.
        // Potrebbe essere utile restituire un valore o lanciare un'eccezione.
        // Per ora, manteniamo la logica che un entry esista sempre se non è vuoto.
        // Se `remove` restituisce null, significa che il bucket non era nel memento.
        // Il codice Java originale non gestisce questo caso e lancerebbe una NullPointerException.
        // Qui restituiamo un valore che indica che non è stato trovato.
        // La logica originale si aspetta che `entry` non sia mai null qui.
        return -1; // O lancia un'eccezione
    }

    --m_size;
    int prevRemoved = entry->prevRemoved;
    delete entry; // Libera la memoria

    if (m_table.size() > MIN_TABLE_SIZE && m_size <= (capacity() / 4)) {
        resizeTable(m_table.size() / 2);
    }

    return prevRemoved;
}

bool MementoWithBinomial::isEmpty() const {
    return m_size <= 0;
}

int MementoWithBinomial::size() const {
    return m_size;
}

int MementoWithBinomial::capacity() const {
    // Fattore di carico di 0.75
    return (m_table.size() / 4) * 3;
}

// --- Metodi Privati ---

void MementoWithBinomial::add(Entry* entry, std::vector<Entry*>& table) {
    int bucket = entry->bucket;
    // L'operatore >>> in Java è uno shift a destra non segnato.
    // Per int non negativi, >> in C++ si comporta allo stesso modo.
    unsigned int h = static_cast<unsigned int>(bucket);
    int hash = h ^ (h >> 16);
    int index = (table.size() - 1) & hash;

    entry->next = table[index];
    table[index] = entry;
}

MementoWithBinomial::Entry* MementoWithBinomial::get(int bucket) const {
    unsigned int h = static_cast<unsigned int>(bucket);
    int hash = h ^ (h >> 16);
    int index = (m_table.size() - 1) & hash;

    Entry* entry = m_table[index];
    while (entry != nullptr) {
        if (entry->bucket == bucket) {
            return entry;
        }
        entry = entry->next;
    }
    return nullptr;
}

MementoWithBinomial::Entry* MementoWithBinomial::remove(int bucket) {
    unsigned int h = static_cast<unsigned int>(bucket);
    int hash = h ^ (h >> 16);
    int index = (m_table.size() - 1) & hash;

    Entry* entry = m_table[index];
    if (entry == nullptr) {
        return nullptr;
    }

    Entry* prev = nullptr;
    while (entry != nullptr && entry->bucket != bucket) {
        prev = entry;
        entry = entry->next;
    }

    if (entry == nullptr) {
        return nullptr;
    }

    if (prev == nullptr) {
        m_table[index] = entry->next;
    } else {
        prev->next = entry->next;
    }

    entry->next = nullptr;
    return entry;
}

void MementoWithBinomial::resizeTable(int newTableSize) {
    if (static_cast<size_t>(newTableSize) < m_table.size() &&
        m_table.size() <= static_cast<size_t>(MIN_TABLE_SIZE)) {
        return;
    }
    if (static_cast<size_t>(newTableSize) > m_table.size() &&
        m_table.size() >= static_cast<size_t>(MAX_TABLE_SIZE)) {
        return;
    }

    std::vector<Entry*> newTable(newTableSize, nullptr);

    for (Entry* head : m_table) {
        Entry* current = head;
        while (current != nullptr) {
            Entry* next = current->next; // Salva il prossimo
            // Ricollega il nodo corrente alla nuova tabella
            add(current, newTable);
            current = next;
        }
    }
    
    // La vecchia tabella ora contiene puntatori che sono stati spostati.
    // Non dobbiamo cancellarli, ma solo sostituire il vettore.
    m_table = newTable;
}

void MementoWithBinomial::clearTable(std::vector<Entry*>& table) {
    for (Entry* head : table) {
        Entry* current = head;
        while (current != nullptr) {
            Entry* toDelete = current;
            current = current->next;
            delete toDelete;
        }
    }
    table.clear();
}