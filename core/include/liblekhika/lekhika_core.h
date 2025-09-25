/********************************************************************
 * lekhika-core.h  –  lekhika core header
 ********************************************************************
Copyright (C) <2025> <Khumnath Cg/nath.khum@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>
 *******************************************************************/
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>

// Forward declare ICU's UnicodeString to avoid including the full header here
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
 * @brief Validates if a UTF-8 string is a well-formed Devanagari word.
 * @param u The UnicodeString to validate.
 * @return True if the word is valid, false otherwise.
 */
bool isValidDevanagariWord(const U_ICU_NAMESPACE::UnicodeString &u);
bool isValidDevanagariWord(const std::string& s);


#ifdef HAVE_SQLITE3

// =============================================================================//
// DictionaryManager Class
// =============================================================================//
class DictionaryManager {
public:
    explicit DictionaryManager(const std::string& dbPath = "");
    ~DictionaryManager();

    void reset();
    std::map<std::string, std::string> getDatabaseInfo();
    void addWord(const std::string &word);
    void removeWord(const std::string &word);
    std::vector<std::string> findWords(const std::string &prefix, int limit);
    int getWordFrequency(const std::string &word);
    bool updateWordFrequency(const std::string &word, int frequency);
    long learnFromFile(const std::string& filePath);
    
    enum SortColumn { ByWord = 0, ByFrequency = 1 };
    std::vector<std::pair<std::string, int>> getAllWords(int limit = -1, int offset = 0, SortColumn sortBy = ByWord, bool ascending = true);
    std::vector<std::pair<std::string, int>> searchWords(const std::string& searchTerm);

    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();
    
    void setSuggestionLimit(int limit) { suggestionLimit_ = limit; }
    int getSuggestionLimit() const { return suggestionLimit_; }

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    int suggestionLimit_ = 10;
};
#endif


// =============================================================================//
// Transliteration Class
// =============================================================================//
class Transliteration {
public:
    explicit Transliteration(const std::string& dataDir = "");
    ~Transliteration();

    std::string transliterate(const std::string &input);

    void setEnableSmartCorrection(bool enable);
    void setEnableAutoCorrect(bool enable);
    void setEnableIndicNumbers(bool enable);
    void setEnableSymbolsTransliteration(bool enable);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

