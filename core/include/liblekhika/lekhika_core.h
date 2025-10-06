/********************************************************************
 * lekhika-core.h  –  lekhika core header
 ********************************************************************
Copyright (C) <2025> <Khumnath Cg/nath.khum@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>
 *******************************************************************/
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <map>

// Forward declare ICU's UnicodeString to avoid including the full ICU header
namespace U_ICU_NAMESPACE {
class UnicodeString;
}

// =============================================================================//
// Standalone Functions
// =============================================================================//

/**
 * @brief Gets the version string of the liblekhika library.
 * @return A string in "MAJOR.MINOR.PATCH" format.
 */
std::string getLekhikaVersion();

/**
 * @brief Validates if a string is a well-formed Devanagari word based on orthographic rules.
 * @param u The ICU UnicodeString to validate.
 * @return True if the word is valid, false otherwise.
 */
bool isValidDevanagariWord(const U_ICU_NAMESPACE::UnicodeString &u);

/**
 * @brief Convenience overload for isValidDevanagariWord that accepts a standard std::string.
 * @param s The UTF-8 encoded std::string to validate.
 * @return True if the word is valid, false otherwise.
 */
bool isValidDevanagariWord(const std::string& s);

/**
 * @brief Removes Devanagari punctuation (like Danda) from a string.
 * @param s The UTF-8 encoded std::string to sanitize.
 * @return A new std::string with punctuation characters removed.
 */
std::string sanitizeDevanagariWord(const std::string& s);

/**
 * @brief Core implementation for sanitizeDevanagariWord that operates on an ICU UnicodeString.
 * @param u The ICU UnicodeString to sanitize.
 * @return A new ICU UnicodeString with punctuation characters removed.
 */
U_ICU_NAMESPACE::UnicodeString sanitizeDevanagariWord(const U_ICU_NAMESPACE::UnicodeString& u);


#ifdef HAVE_SQLITE3

// =============================================================================//
// DictionaryManager Class
// =============================================================================//
/**
 * @brief Manages the user's word dictionary stored in a SQLite database.
 *
 * This class handles all database operations, including creating, reading,
 * updating, and deleting words.
 */
class DictionaryManager {
public:
    /**
     * @brief Constructs the manager and opens the database connection.
     * @param dbPath Optional path to the database file. If empty, a default
     * platform-specific path (e.g., in XDG_DATA_HOME) is used.
     */
    explicit DictionaryManager(const std::string& dbPath = "");

    /**
     * @brief Destroys the manager and closes the database connection.
     */
    ~DictionaryManager();

    /**
     * @brief Deletes ALL words from the dictionary. This action cannot be undone.
     */
    void reset();

    /**
     * @brief Retrieves metadata about the current database.
     * @return A map containing info like "word_count", "db_path", etc.
     */
    std::map<std::string, std::string> getDatabaseInfo();

    /**
     * @brief Adds a word to the dictionary. If the word already exists, its
     * frequency count is incremented.
     * @param word The Devanagari word to add.
     */
    void addWord(const std::string &word);

    /**
     * @brief Removes a word from the dictionary.
     * @param word The word to remove.
     */
    void removeWord(const std::string &word);

    /**
     * @brief Finds words in the dictionary that start with a given prefix.
     * @param prefix The Devanagari prefix to search for.
     * @param limit The maximum number of words to return.
     * @return A vector of matching words, sorted by frequency in descending order.
     */
    std::vector<std::string> findWords(const std::string &prefix, int limit);

    /**
     * @brief Gets the frequency count of a specific word.
     * @param word The word to look up.
     * @return The frequency of the word, or -1 if the word is not found.
     */
    int getWordFrequency(const std::string &word);

    /**
     * @brief Manually sets the frequency count for a given word.
     * @param word The word to update.
     * @param frequency The new frequency value to set.
     * @return True on success, false if the word was not found.
     */
    bool updateWordFrequency(const std::string &word, int frequency);

    /**
     * @brief Reads a text file, sanitizes and validates the words, and learns them.
     * @param filePath The path to the UTF-8 encoded text file.
     * @return The total number of new words learned from the file.
     */
    long learnFromFile(const std::string& filePath);

    /// Defines the columns available for sorting database queries.
    enum SortColumn { ByWord = 0, ByFrequency = 1 };

    /**
     * @brief Retrieves all words from the dictionary with pagination and sorting.
     * @param limit The maximum number of words per page (-1 for all).
     * @param offset The starting position for the query (for pagination).
     * @param sortBy The column to sort by (ByWord or ByFrequency).
     * @param ascending The sort order (true for ASC, false for DESC).
     * @return A vector of (word, frequency) pairs.
     */
    std::vector<std::pair<std::string, int>> getAllWords(int limit = -1, int offset = 0, SortColumn sortBy = ByWord, bool ascending = true);

    /**
     * @brief Searches for words containing a specific substring.
     * @param searchTerm The substring to search for within words.
     * @return A vector of matching (word, frequency) pairs.
     */
    std::vector<std::pair<std::string, int>> searchWords(const std::string& searchTerm);

    /** @brief Starts a database transaction for efficient bulk operations. */
    void beginTransaction();
    /** @brief Commits the current database transaction. */
    void commitTransaction();
    /** @brief Rolls back the current database transaction. */
    void rollbackTransaction();

    /** @brief Sets the maximum number of suggestions to return in findWords. */
    void setSuggestionLimit(int limit);
    /** @brief Gets the current suggestion limit. */
    int getSuggestionLimit() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};
#endif


// =============================================================================//
// Transliteration Class
// =============================================================================//
/**
 * @brief Provides Roman-to-Devanagari transliteration services.
 *
 * This class converts Latin (Roman) script input into its Devanagari
 * equivalent based on a set of mapping rules and heuristics.
 */
class Transliteration {
public:
    /**
     * @brief Constructs the transliterator and loads mapping files.
     * @param dataDir Optional path to the directory containing mapping files
     * (e.g., mapping.toml). If empty, default system paths
     * are searched.
     */
    explicit Transliteration(const std::string& dataDir = "");

    /**
     * @brief Destroys the transliterator.
     */
    ~Transliteration();

    /**
     * @brief Transliterates a Latin script string into Devanagari.
     * @param input The input string in Latin script.
     * @return The resulting string in Devanagari script.
     */
    std::string transliterate(const std::string &input);

    /** @brief Enables/disables smart corrections (e.g., pani -> panee). */
    void setEnableSmartCorrection(bool enable);
    /** @brief Enables/disables auto-correction of specific words from a list. */
    void setEnableAutoCorrect(bool enable);
    /** @brief Enables/disables transliteration of ASCII digits to Devanagari digits. */
    void setEnableIndicNumbers(bool enable);
    /** @brief Enables/disables transliteration of common symbols (e.g., ? -> ।). */
    void setEnableSymbolsTransliteration(bool enable);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};
