/********************************************************************
 * lekhika-core.cpp  –  lekhika core implementation.
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
#include "liblekhika/lekhika_core.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <unordered_map>

// ICU includes for Unicode string handling and validation
#include <unicode/unistr.h>
#include <unicode/brkiter.h>
#include <unicode/locid.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>

#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#endif

namespace fs = std::filesystem;

// =============================================================================//
// Standalone Function Implementations
// =============================================================================//

std::string getLekhikaVersion() {
    // This macro is defined by the CMake build script
    return LEKHIKA_VERSION;
}

// ----------------- Character classification -----------------
inline bool isDevanagariConsonant(UChar32 c) {
    // Standard consonants and extended consonants
    return (c >= 0x0915 && c <= 0x0939) || (c >= 0x0958 && c <= 0x095F);
}

inline bool isHalant(UChar32 c) { return c == 0x094D; }

inline bool isNukta(UChar32 c) { return c == 0x093C; }

inline bool isDependentVowelSign(UChar32 c) {
    // Includes all matras
    return (c >= 0x093E && c <= 0x094C) || (c >= 0x0962 && c <= 0x0963);
}

inline bool isIndependentVowel(UChar32 c) {
    return (c >= 0x0904 && c <= 0x0914);
}

inline bool isAnusvaraVisargaChandrabindu(UChar32 c) {
    // Combining marks that can follow a vowel sound or consonants
    return c == 0x0901 || c == 0x0902 || c == 0x0903;
}

inline bool isAvagraha(UChar32 c) { return c == 0x093D; }

inline bool isZWJorZWNJ(UChar32 c) { return c == 0x200C || c == 0x200D; }

inline bool isDigit(UChar32 c) { return (c >= 0x0966 && c <= 0x096F); }

inline bool isDandaOrPunctuation(UChar32 c) {
    return c == 0x0964 || c == 0x0965 || u_ispunct(c);
}

inline bool isAllowedDevanagariChar(UChar32 c) {
    return (c >= 0x0900 && c <= 0x097F)     // Devanagari Block
           || (c >= 0xA8E0 && c <= 0xA8FF)  // Devanagari Extended
           || isZWJorZWNJ(c);
}

// ----------------- Grapheme counting -----------------
//rejects single-grapheme tokens as they are not considered valid dictionary words in this system
int graphemeCount(const icu::UnicodeString &u) {
    if (u.isEmpty()) return 0;
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::BreakIterator> it(
        icu::BreakIterator::createCharacterInstance(icu::Locale::getDefault(), status));
    if (U_FAILURE(status)) return u.length(); // Fallback
    it->setText(u);

    int count = 0;
    for (int32_t p = it->first(); p != icu::BreakIterator::DONE; p = it->next()) {
        count++;
    }
    return count > 0 ? count - 1 : 0; // Account for empty string and final boundary
}


// Overload for icu::UnicodeString
icu::UnicodeString sanitizeDevanagariWord(const icu::UnicodeString& u) {
    icu::UnicodeString sanitized;
    for (int32_t i = 0; i < u.length(); ) {
        UChar32 c = u.char32At(i);
        // Keep the character ONLY if it's NOT punctuation
        if (!isDandaOrPunctuation(c)) {
            sanitized.append(c);
        }
        i += U16_LENGTH(c);
    }
    return sanitized;
}

// Overload for std::string for convenience
std::string sanitizeDevanagariWord(const std::string& s) {
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(s);
    icu::UnicodeString sanitizedUstr = sanitizeDevanagariWord(ustr);
    std::string sanitizedStr;
    sanitizedUstr.toUTF8String(sanitizedStr);
    return sanitizedStr;
}

// ----------------- Validation -----------------
bool isValidDevanagariWord(const icu::UnicodeString &u) {
    if (u.isEmpty()) return false;

    if (graphemeCount(u) < 2) return false;

    // State machine for validation
    enum State {
        START,
        AFTER_CONSONANT,           // After a consonant or consonant+nukta
        AFTER_HALANT,              // After a halant
        AFTER_INDEPENDENT_VOWEL,   // After a standalone vowel like अ, आ
        AFTER_SYLLABLE_WITH_MATRA, // After a consonant+matra like का, कि
        AFTER_MODIFIER,            // After Anusvara, Visarga, etc.
        AFTER_AVAGRAHA
    };

    State state = START;

    for (int32_t i = 0; i < u.length();) {
        UChar32 c = u.char32At(i);

        if (!isAllowedDevanagariChar(c)) return false;
        if (isDigit(c) || isDandaOrPunctuation(c)) return false;

        if (isDevanagariConsonant(c)) {
            // A consonant can start a word, follow another consonant,
            // or follow a vowel/halant to start a new syllable/conjunct.
            if (state == START || state == AFTER_INDEPENDENT_VOWEL || state == AFTER_SYLLABLE_WITH_MATRA || state == AFTER_HALANT || state == AFTER_MODIFIER || state == AFTER_AVAGRAHA || state == AFTER_CONSONANT) {
                state = AFTER_CONSONANT;
            } else {
                return false;
            }
        }
        else if (isIndependentVowel(c)) {
            // An independent vowel can start a word or follow another independent vowel.
            // It cannot follow a consonant+matra syllable or a halant.
            if (state == START || state == AFTER_INDEPENDENT_VOWEL || state == AFTER_MODIFIER || state == AFTER_AVAGRAHA) {
                state = AFTER_INDEPENDENT_VOWEL;
            } else {
                return false;
            }
        }
        else if (isHalant(c)) {
            // Halant must follow a consonant.
            if (state == AFTER_CONSONANT) {
                state = AFTER_HALANT;
            } else {
                return false;
            }
        }
        else if (isNukta(c)) {
            // Nukta must follow a consonant. The result is still treated as a consonant.
            if (state == AFTER_CONSONANT) {
                state = AFTER_CONSONANT; // State doesn't change
            } else {
                return false;
            }
        }
        else if (isDependentVowelSign(c)) {
            // A matra (dependent vowel) must follow a consonant.
            if (state == AFTER_CONSONANT) {
                state = AFTER_SYLLABLE_WITH_MATRA;
            } else {
                return false;
            }
        }
        else if (isAnusvaraVisargaChandrabindu(c)) {
            // These modifiers must follow a character with a vowel sound.
            if (state == AFTER_CONSONANT || state == AFTER_INDEPENDENT_VOWEL || state == AFTER_SYLLABLE_WITH_MATRA) {
                state = AFTER_MODIFIER;
            } else {
                return false;
            }
        }
        else if (isAvagraha(c)) {
            // Avagraha(ऽ) typically follows a vowel sound.
            if (state == AFTER_CONSONANT || state == AFTER_INDEPENDENT_VOWEL || state == AFTER_SYLLABLE_WITH_MATRA || state == AFTER_MODIFIER) {
                state = AFTER_AVAGRAHA;
            } else {
                return false;
            }
        }
        else if (isZWJorZWNJ(c)) {
            // A ZWJ/ZWNJ is only meaningful after a halant to control ligation.
            // We reject it in all other "orphaned" contexts.
            if (state == AFTER_HALANT) {
                // The state does not change. We are still in an "after halant"
                // context, but now with a joiner hint.
            } else {
                return false;
            }
        }
        else {
            return false;
        }

        i += U16_LENGTH(c);
    }

    // Final Validation
    int32_t lastCharIndex = u.length();
    U16_BACK_1(u.getBuffer(), 0, lastCharIndex);
    UChar32 lastChar = u.char32At(lastCharIndex);
    if (isZWJorZWNJ(lastChar)) {
        return false;
    }

    return state == AFTER_CONSONANT || state == AFTER_INDEPENDENT_VOWEL || state == AFTER_SYLLABLE_WITH_MATRA || state == AFTER_MODIFIER || state == AFTER_HALANT || state == AFTER_AVAGRAHA;
}


// ----------------- Overload for std::string -----------------
bool isValidDevanagariWord(const std::string &s) {
    return isValidDevanagariWord(icu::UnicodeString::fromUTF8(s));
}

#ifdef HAVE_SQLITE3
// =============================================================================//
// DictionaryManager Implementation (PImpl Idiom)
// =============================================================================//
class DictionaryManager::Impl {
public:
    sqlite3* db_ = nullptr;

    explicit Impl(const std::string& dbPath) {
        fs::path finalDbPath;
        if (!dbPath.empty()) {
            finalDbPath = dbPath;
        } else {
            const char* xdg_data_home = getenv("XDG_DATA_HOME");
            fs::path dataHome;
            if (xdg_data_home && xdg_data_home[0] != '\0') {
                dataHome = xdg_data_home;
            } else {
                const char* home = getenv("HOME");
                if (!home) {
                    throw std::runtime_error("Cannot find HOME or XDG_DATA_HOME directory.");
                }
                dataHome = fs::path(home) / ".local" / "share";
            }
            finalDbPath = dataHome / "lekhika-core" / "lekhikadict.akshardb";
        }

        fs::create_directories(finalDbPath.parent_path());
        bool dbExists = fs::exists(finalDbPath);

        if (sqlite3_open(finalDbPath.c_str(), &db_) != SQLITE_OK) {
            std::string errMsg = db_ ? sqlite3_errmsg(db_) : "SQLite failed to open database";
            db_ = nullptr; // Ensure db_ is null on failure
            throw std::runtime_error("Can't open database: " + errMsg);
        }

        if (!dbExists) {
            initializeDatabase();
        }
    }

    ~Impl() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    void initializeDatabase() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS words ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "word TEXT NOT NULL UNIQUE,"
            "frequency INTEGER NOT NULL DEFAULT 1);"
            "CREATE INDEX IF NOT EXISTS idx_word ON words(word);"
            "CREATE TABLE IF NOT EXISTS meta ("
            "key TEXT PRIMARY KEY, value TEXT);"
            "INSERT OR IGNORE INTO meta (key, value) VALUES ('format_version', '1.0');"
            "INSERT OR IGNORE INTO meta (key, value) VALUES ('Db', 'lekhika');"
            "INSERT OR IGNORE INTO meta (key, value) VALUES ('language', 'ne');"
            "INSERT OR IGNORE INTO meta (key, value) VALUES ('script', 'Devanagari');"
            "INSERT OR IGNORE INTO meta (key, value) VALUES ('created_at', strftime('%Y-%m-%d', 'now'));";


        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string err = "SQL error during initialization: " + std::string(errMsg);
            sqlite3_free(errMsg);
            throw std::runtime_error(err);
        }
    }
};

//  Public DictionaryManager methods forwarding to Impl

DictionaryManager::DictionaryManager(const std::string& dbPath) : pImpl(std::make_unique<Impl>(dbPath)) {}
DictionaryManager::~DictionaryManager() = default;

void DictionaryManager::reset() {
    if (!pImpl->db_) {
        throw std::runtime_error("Cannot reset: Database is not connected.");
    }
    const char* sql = "DELETE FROM words;";
    char* errMsg = nullptr;
    if (sqlite3_exec(pImpl->db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error_message = "Failed to reset dictionary: " + std::string(errMsg);
        sqlite3_free(errMsg);
        throw std::runtime_error(error_message);
    }
}

std::map<std::string, std::string> DictionaryManager::getDatabaseInfo() {
    if (!pImpl->db_) return {};
    std::map<std::string, std::string> info;
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(pImpl->db_, "SELECT COUNT(*) FROM words;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            info["word_count"] = std::to_string(sqlite3_column_int(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    
    // Get the full path and replace home directory with ~
    std::string fullPath = sqlite3_db_filename(pImpl->db_, "main");
    const char* homeEnv = getenv("HOME");
    if (homeEnv && fullPath.rfind(homeEnv, 0) == 0) { // Check if path starts with homeEnv
        info["db_path"] = "~" + fullPath.substr(strlen(homeEnv));
    } else {
        info["db_path"] = fullPath;
    }

    if (sqlite3_prepare_v2(pImpl->db_, "SELECT key, value FROM meta;", -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            std::string val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            info[key] = val;
        }
        sqlite3_finalize(stmt);
    }
    return info;
}

long DictionaryManager::learnFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filePath);
    }

    long wordsLearned = 0;
    std::string line;
    beginTransaction();
    try {
        while (std::getline(file, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);
            if (!line.empty() && isValidDevanagariWord(line)) {
                addWord(line);
                wordsLearned++;
            }
        }
        commitTransaction();
    } catch (...) {
        rollbackTransaction();
        throw; // Re-throw the exception after rolling back
    }
    return wordsLearned;
}


void DictionaryManager::addWord(const std::string &word) {
    if (!pImpl->db_) {
        throw std::runtime_error("Cannot add word: Database is not connected.");
    }
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO words (word) VALUES (?) "
                      "ON CONFLICT(word) DO UPDATE SET frequency = frequency + 1;";

    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, word.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void DictionaryManager::removeWord(const std::string &word) {
    if (!pImpl->db_) {
        throw std::runtime_error("Cannot remove word: Database is not connected.");
    }
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM words WHERE word = ?;";
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, word.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

std::vector<std::string> DictionaryManager::findWords(const std::string &input, int limit) {
    std::vector<std::string> results;
    if (!pImpl->db_ || input.empty()) return results;
    sqlite3_stmt *stmt = nullptr;

    const char *sqlPrefix = "SELECT word FROM words WHERE word LIKE ? ORDER BY frequency DESC LIMIT ?;";
    if (sqlite3_prepare_v2(pImpl->db_, sqlPrefix, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string pattern = input + "%";
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            results.emplace_back(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
        }
        sqlite3_finalize(stmt);
    }
    return results;
}

int DictionaryManager::getWordFrequency(const std::string &word) {
    if (!pImpl->db_){
        // Returning -1 is a reasonable contract for "not found or error"
        return -1;
    }
    sqlite3_stmt *stmt;
    const char *sql = "SELECT frequency FROM words WHERE word = ?;";
    int frequency = -1;
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, word.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            frequency = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return frequency;
}

bool DictionaryManager::updateWordFrequency(const std::string &word, int frequency) {
    if (!pImpl->db_) {
        // Returning false for failure is acceptable here, but a throw would be more consistent
        return false;
    }
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE words SET frequency = ? WHERE word = ?;";
    if (sqlite3_prepare_v2(pImpl->db_, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, frequency);
    sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success && (sqlite3_changes(pImpl->db_) > 0);
}

std::vector<std::pair<std::string, int>> DictionaryManager::getAllWords(int limit, int offset, SortColumn sortBy, bool ascending) {
    std::vector<std::pair<std::string, int>> results;
    if (!pImpl->db_) return results;
    sqlite3_stmt *stmt;
    std::string sql_str = "SELECT word, frequency FROM words ORDER BY " +
                        std::string(sortBy == ByFrequency ? "frequency " : "word ") +
                        std::string(ascending ? "ASC" : "DESC");
    if (limit > 0) sql_str += " LIMIT ?";
    if (offset > 0) sql_str += " OFFSET ?";
    sql_str += ";";
    if (sqlite3_prepare_v2(pImpl->db_, sql_str.c_str(), -1, &stmt, NULL) == SQLITE_OK) {
        int bind_idx = 1;
        if (limit > 0) sqlite3_bind_int(stmt, bind_idx++, limit);
        if (offset > 0) sqlite3_bind_int(stmt, bind_idx++, offset);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            results.emplace_back(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)),
                                 sqlite3_column_int(stmt, 1));
        }
        sqlite3_finalize(stmt);
    }
    return results;
}

std::vector<std::pair<std::string, int>> DictionaryManager::searchWords(const std::string& searchTerm) {
    std::vector<std::pair<std::string, int>> results;
    if (!pImpl->db_ || searchTerm.empty()) return results;
    sqlite3_stmt *stmt;
    std::string sql_str = "SELECT word, frequency FROM words WHERE word LIKE ? ORDER BY frequency DESC;";
    if (sqlite3_prepare_v2(pImpl->db_, sql_str.c_str(), -1, &stmt, NULL) == SQLITE_OK) {
        std::string pattern = "%" + searchTerm + "%";
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            results.emplace_back(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)),
                                 sqlite3_column_int(stmt, 1));
        }
        sqlite3_finalize(stmt);
    }
    return results;
}

void DictionaryManager::beginTransaction() {
    if (!pImpl->db_) {
        throw std::runtime_error("Cannot begin transaction: Database is not connected.");
    }
    char *zErrMsg = 0;
    if (sqlite3_exec(pImpl->db_, "BEGIN TRANSACTION;", NULL, 0, &zErrMsg) != SQLITE_OK) {
        std::string error = "SQL error: " + std::string(zErrMsg);
        sqlite3_free(zErrMsg);
        throw std::runtime_error(error);
    }
}

void DictionaryManager::commitTransaction() {
    if (!pImpl->db_) {
        throw std::runtime_error("Cannot commit transaction: Database is not connected.");
    }
    char *zErrMsg = 0;
    if (sqlite3_exec(pImpl->db_, "COMMIT;", NULL, 0, &zErrMsg) != SQLITE_OK) {
        std::string error = "SQL error: " + std::string(zErrMsg);
        sqlite3_free(zErrMsg);
        throw std::runtime_error(error);
    }
}

void DictionaryManager::rollbackTransaction() {
    if (!pImpl->db_) {
        // No throw here, as rollback is often called in a catch block.
        // Failing to rollback is not usually a critical failure.
        return;
    }
    sqlite3_exec(pImpl->db_, "ROLLBACK;", NULL, 0, NULL);
}

#endif // HAVE_SQLITE3


// =============================================================================//
// Transliteration Implementation (PImpl Idiom)
// =============================================================================//
class Transliteration::Impl {
public:
    std::unordered_map<std::string, std::string> charMap_;
    std::unordered_map<std::string, std::string> specialWords_;
    bool enableSmartCorrection_ = true;
    bool enableAutoCorrect_ = true;
    bool enableIndicNumbers_ = true;
    bool enableSymbolsTransliteration_ = true;
    fs::path dataDir_;

#include <filesystem>

    explicit Impl(const std::string& dataDir) {
        if (!dataDir.empty()) {
            dataDir_ = dataDir;
        } else if (std::filesystem::exists("/usr/share/liblekhika/")) {
            dataDir_ = "/usr/share/liblekhika/";
        } else {
            dataDir_ = "/usr/local/share/liblekhika/";
        }

        loadMappings();
        loadSpecialWords();
    }


    std::string readFileContent(const std::string& filename) {
        fs::path fullPath = dataDir_ / filename;
        if (!fs::exists(fullPath)) {
            throw std::runtime_error("Could not locate critical data file: " + fullPath.string());
        }
        std::ifstream file(fullPath);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open critical data file: " + fullPath.string());
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void loadSpecialWords() {
        if (!enableAutoCorrect_) return;
        std::string content = readFileContent("autocorrect.toml");
        if (!content.empty()) {
            parseSpecialWordsToml(content);
        }
    }

    void loadMappings() {
        std::string content = readFileContent("mapping.toml");
        if (!content.empty()) {
            parseMappingsToml(content);
        }
    }

    void parseSpecialWordsToml(const std::string &content);
    void parseMappingsToml(const std::string &content);
    std::string transliterateSegment(const std::string &segment);
    std::string preprocess(const std::string &word);
    std::string applySmartCorrection(const std::string &word) const;
    std::string applyAutoCorrection(const std::string &word) const;
    std::string preprocessInput(const std::string &input);
    bool isVowel(char c) const;
};

// Public Transliteration methods forwarding to Impl 

Transliteration::Transliteration(const std::string& dataDir) : pImpl(std::make_unique<Impl>(dataDir)) {}
Transliteration::~Transliteration() = default;

void Transliteration::setEnableSmartCorrection(bool enable) { pImpl->enableSmartCorrection_ = enable; }
void Transliteration::setEnableAutoCorrect(bool enable) { pImpl->enableAutoCorrect_ = enable; }
void Transliteration::setEnableIndicNumbers(bool enable) { pImpl->enableIndicNumbers_ = enable; }
void Transliteration::setEnableSymbolsTransliteration(bool enable) { pImpl->enableSymbolsTransliteration_ = enable; }

std::string Transliteration::transliterate(const std::string &input) {
    std::string preprocessed = pImpl->preprocessInput(input);
    std::unordered_map<std::string, std::string> engTokens;
    std::string processed = preprocessed;
    size_t tokenCount = 1;
    size_t beginIndex = 0;
    size_t endIndex = 0;
    while ((beginIndex = processed.find("{", endIndex)) != std::string::npos) {
        endIndex = processed.find("}", beginIndex + 1);
        if (endIndex == std::string::npos) {
            endIndex = processed.size() - 1;
        }
        std::string token =
            processed.substr(beginIndex, endIndex - beginIndex + 1);
        std::string mask = "$-" + std::to_string(tokenCount++) + "-$";
        engTokens[mask] = token.substr(1, token.length() - 2);
        processed.replace(beginIndex, token.length(), mask);
        endIndex = beginIndex + mask.length();
    }
    std::string result;
    std::istringstream iss(processed);
    std::string segment;
    bool first = true;
    while (std::getline(iss, segment, ' ')) {
        if (!segment.empty()) {
            if (!first)
                result += " ";
            if (segment.length() == 1 && std::isdigit(segment[0]) &&
                !pImpl->enableIndicNumbers_) {
                result += segment;
            } else if (segment.length() == 1 && !std::isalnum(segment[0]) &&
                       !pImpl->enableSymbolsTransliteration_) {
                result += segment;
            } else if (segment.length() == 1 && pImpl->charMap_.count(segment)) {
                result += pImpl->charMap_[segment];
            } else {
                std::string cleaned = pImpl->preprocess(segment);
                result += pImpl->transliterateSegment(cleaned);
            }
            first = false;
        }
    }
    for (const auto &[mask, original] : engTokens) {
        std::string translatedMask = pImpl->transliterateSegment(mask);
        size_t pos = 0;
        while ((pos = result.find(translatedMask, pos)) != std::string::npos) {
            result.replace(pos, translatedMask.length(), original);
            pos += original.length();
        }
    }
    return result;
}


//  Full implementation of Transliteration::Impl methods
void Transliteration::Impl::parseSpecialWordsToml(const std::string &content) {
    std::istringstream iss(content);
    std::string line, section;
    while (std::getline(iss, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (line.empty() || line[0] == '#')
            continue;
        if (line[0] == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }
        if (section != "specialWords")
            continue;
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos)
            continue;
        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        specialWords_[key] = value;
    }
}

void Transliteration::Impl::parseMappingsToml(const std::string &content) {
    std::istringstream iss(content);
    std::string line, section;
    std::unordered_map<std::string, std::string> consonantMap;
    auto unquote = [](std::string str) -> std::string {
        if (str.size() >= 2 && ((str.front() == '"' && str.back() == '"') ||
                                (str.front() == '\'' && str.back() == '\''))) {
            str = str.substr(1, str.size() - 2);
        }
        std::string result;
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '\\' && i + 1 < str.size()) {
                char next = str[i + 1];
                if (next == '\\')
                    result += '\\';
                else if (next == 'n')
                    result += '\n';
                else if (next == 't')
                    result += '\t';
                else
                    result += next;
                ++i;
            } else {
                result += str[i];
            }
        }
        return result;
    };
    while (std::getline(iss, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (line.empty() || line[0] == '#')
            continue;
        if (line[0] == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos)
            continue;
        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);
        size_t commentPos = value.find('#');
        if (commentPos != std::string::npos) {
            value = value.substr(0, commentPos);
        }
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        key = unquote(key);
        value = unquote(value);
        if (section == "charMap") {
            charMap_[key] = value;
        } else if (section == "consonantMap") {
            consonantMap[key] = value;
        }
    }
    for (const auto &[conso, val] : consonantMap) {
        std::string consoMinusA = (conso.size() > 1 && conso.back() == 'a')
        ? conso.substr(0, conso.size() - 1)
        : conso;
        if (!charMap_.count(conso))
            charMap_[conso] = val;
        if (!charMap_.count(conso + "a"))
            charMap_[conso + "a"] = val + "ा";
        if (!charMap_.count(consoMinusA + "i"))
            charMap_[consoMinusA + "i"] = val + "ि";
        if (!charMap_.count(consoMinusA + "ee"))
            charMap_[consoMinusA + "ee"] = val + "ी";
        if (!charMap_.count(consoMinusA + "u"))
            charMap_[consoMinusA + "u"] = val + "ु";
        if (!charMap_.count(consoMinusA + "oo"))
            charMap_[consoMinusA + "oo"] = val + "ू";
        if (!charMap_.count(consoMinusA + "rri"))
            charMap_[consoMinusA + "rri"] = val + "ृ";
        if (!charMap_.count(consoMinusA + "e"))
            charMap_[consoMinusA + "e"] = val + "े";
        if (!charMap_.count(consoMinusA + "ai"))
            charMap_[consoMinusA + "ai"] = val + "ै";
        if (!charMap_.count(consoMinusA + "o"))
            charMap_[consoMinusA + "o"] = val + "ो";
        if (!charMap_.count(consoMinusA + "au"))
            charMap_[consoMinusA + "au"] = val + "ौ";
        if (!charMap_.count(consoMinusA))
            charMap_[consoMinusA] = val + "्";
    }
}

std::string Transliteration::Impl::applyAutoCorrection(const std::string &word) const {
    auto it = specialWords_.find(word);
    if (it != specialWords_.end()) {
        return it->second;
    }
    return word;
}

bool Transliteration::Impl::isVowel(char c) const {
    return std::string("aeiou").find(tolower(c)) != std::string::npos;
}

std::string Transliteration::Impl::applySmartCorrection(const std::string &input) const {
    std::string word = input;
    if (word.length() > 3) {
        char ec_0 = tolower(word.back());
        char ec_1 = tolower(word[word.length() - 2]);
        char ec_2 = tolower(word[word.length() - 3]);
        char ec_3 = word.length() > 3 ? tolower(word[word.length() - 4]) : '\0';

        // Corrects a word-final 'y' (when not a vowel) to 'ee' for a long vowel sound.
        // Example: User types "gunDy" which might be intended as "gunDee" for गुण्डी.
        if (!isVowel(ec_0) && ec_0 == 'y') {
            word = word.substr(0, word.length() - 1) + "ee";
        } else if (!(ec_0 == 'a' && ec_1 == 'h' && ec_2 == 'h') &&
                   !(ec_0 == 'a' && ec_1 == 'n' &&
                     (ec_2 == 'k' || ec_2 == 'h' || ec_2 == 'r')) &&
                   !(ec_0 == 'a' && ec_1 == 'r' &&
                     ((ec_2 == 'd' && ec_3 == 'n') ||
                      (ec_2 == 't' && ec_3 == 'n')))) {
            // This is a heuristic for schwa addition. It appends an 'a' if the word
            // ends in a consonant that is likely to have an explicit 'a' sound.
            // Example: "ram" becomes "rama" (राम), but it avoids this for complex conjuncts
            // or nasalizations where the 'a' is often silent.
            if (ec_0 == 'a' && (ec_1 == 'm' || (!isVowel(ec_1) && !isVowel(ec_3) &&
                                                ec_1 != 'y' && ec_2 != 'e'))) {
                word += "a";
            }
        }

        // Corrects a short 'i' at the end of a word to a long 'ee'.
        // This handles a common user mistake where they type 'i' for the 'ई' sound.
        // Example: The user types "pani" (पनि), which is often intended to be "panee" (पानी).
        // We specifically avoid this for 'rri' ('ऋ') sequences.
        if (ec_0 == 'i' && !isVowel(ec_1) && !(ec_1 == 'r' && ec_2 == 'r')) {
            word = word.substr(0, word.length() - 1) + "ee";
        }
    }

    // Changes 'n' to 'ng' before velar consonants (k, g) to produce the correct nasal sound (ङ).
    // Example: "ank" is corrected to "angk" (अङ्क).
    for (size_t i = 0; i < word.length(); ++i) {
        if (tolower(word[i]) == 'n' && i > 0 && i + 1 < word.length()) {
            char next_char = tolower(word[i + 1]);
            if (next_char == 'k' || next_char == 'g') {
                word.replace(i, 1, "ng");
                i++;
            }
        }
    }

    // Future use: Anusvara handling for specific consonants following 'm'
    /*
  const std::string anusvaraConsonants = "yrlvsh";
  for (size_t i = 0; i < word.length(); ++i) {
      if (tolower(word[i]) == 'm' && i + 1 < word.length()) {
          if (anusvaraConsonants.find(tolower(word[i + 1])) != std::string::npos) {
              // This logic would replace 'm' with an anusvara character, e.g., '*'
              // word[i] = '*';
          }
      }
  }
  */

    // This rule is to handle gemination (doubling) of 'g' in 'ng' clusters
    // when followed by a vowel, approximating sounds like in "sanggha" (सङ्घ).
    size_t pos_ng = word.find("ng");
    while (pos_ng != std::string::npos) {
        if (pos_ng >= 2 && pos_ng + 2 < word.length() &&
            isVowel(word[pos_ng + 2])) {
            word.replace(pos_ng, 2, "ngg");
            pos_ng = word.find("ng", pos_ng + 3);
        } else {
            pos_ng = word.find("ng", pos_ng + 1);
        }
    }

    // Handles conversion of 'n' to the correct nasal consonant based on the following character.
    for (size_t i = 0; i < word.size(); ++i) {
        if (word[i] == 'n' && i + 1 < word.size()) {
            char next = word[i + 1];
            // 'n' before a retroflex stop (T, D) becomes a retroflex nasal 'N' (ण).
            // Example: "ghanTa" -> "ghaNTa" (घन्टा -> घण्टा ).
            if (next == 'T' || next == 'D') {
                word.replace(i, 1, "N");
                i++;
            }
            // 'n' before 'ch' becomes a palatal nasal 'ñ' (ञ्).
            // Example: "kanchan" -> "kañchan" (कन्चन -> कञ्चन).
            else if (next == 'c' && i + 2 < word.size() && word[i + 2] == 'h') {
                if (!(i + 3 < word.size() && word[i + 3] == 'h')) {
                    word.replace(i, 1, "ञ्");
                    i++;
                }
            }
        }
    }
    return word;
}

std::string Transliteration::Impl::preprocess(const std::string &input) {
    std::string processedWord = input;
    if (enableAutoCorrect_) {
        std::string autoCorrected = applyAutoCorrection(processedWord);
        if (autoCorrected != processedWord) {
            return autoCorrected;
        }
    }
    if (enableSmartCorrection_) {
        processedWord = applySmartCorrection(processedWord);
    }
    return processedWord;
}

std::string Transliteration::Impl::preprocessInput(const std::string &input) {
    std::string out;
    out.reserve(input.size());
    const std::string specialSymbols = "*";
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        std::string symbol(1, c);
        if (specialSymbols.find(c) != std::string::npos) {
            out += c;
            continue;
        }
        if (i > 0 && (c == '.' || c == '?' || charMap_.count(symbol)) &&
            !std::isalnum(static_cast<unsigned char>(c)) && input[i - 1] != ' ') {
            out += ' ';
        }
        out += c;
    }
    return out;
}

std::string Transliteration::Impl::transliterateSegment(const std::string &input) {
    std::string result;
    std::istringstream splitter(input);
    std::string subSegment;
    while (std::getline(splitter, subSegment, '/')) {
        if (!subSegment.empty()) {
            std::string subResult;
            std::string rem = subSegment;
            while (!rem.empty()) {
                std::string matched;
                for (int i = static_cast<int>(rem.size()); i > 0; --i) {
                    std::string part = rem.substr(0, i);
                    if (part.length() == 1 && std::isdigit(part[0]) &&
                        !enableIndicNumbers_) {
                        matched = part;
                        rem.erase(0, i);
                        break;
                    }
                    if (part.length() == 1 && !std::isalnum(part[0]) &&
                        !enableSymbolsTransliteration_) {
                        matched = part;
                        rem.erase(0, i);
                        break;
                    }
                    if (charMap_.count(part)) {
                        matched = charMap_[part];
                        rem.erase(0, i);
                        break;
                    }
                }
                if (!matched.empty()) {
                    subResult += matched;
                } else {
                    std::string singleChar(1, rem[0]);
                    if (std::isdigit(rem[0]) && !enableIndicNumbers_) {
                        subResult += rem[0];
                    } else if (!std::isalnum(rem[0]) && !enableSymbolsTransliteration_) {
                        subResult += rem[0];
                    } else if (charMap_.count(singleChar)) {
                        subResult += charMap_[singleChar];
                    } else {
                        subResult += rem[0];
                    }
                    rem.erase(0, 1);
                }
            }
            bool originalEndsWithHalanta =
                (!subSegment.empty() && subSegment.back() == '\\');
            bool resultEndsWithHalanta =
                (subResult.size() >= 3 &&
                 static_cast<unsigned char>(subResult[subResult.size() - 3]) ==
                     0xE0 &&
                 static_cast<unsigned char>(subResult[subResult.size() - 2]) ==
                     0xA5 &&
                 static_cast<unsigned char>(subResult[subResult.size() - 1]) ==
                     0x8D);
            if (resultEndsWithHalanta && !originalEndsWithHalanta &&
                subSegment.size() > 1) {
                subResult.resize(subResult.size() - 3);
            }
            result += subResult;
        }
    }
    return result;
}

