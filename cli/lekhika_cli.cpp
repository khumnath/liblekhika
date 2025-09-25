#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <liblekhika/lekhika_core.h>

namespace fs = std::filesystem;

// Forward declaration
void printHelp();

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);
    bool testMode = false;
    std::string dataDir;
    int suggestionLimit = 7; // Default limit

    // --- Argument Parsing ---
    auto it = args.begin();
    while (it != args.end()) {
        if (*it == "-test") {
            testMode = true;
            it = args.erase(it);
        } else if (*it == "--limit") {
            it = args.erase(it);
            if (it != args.end()) {
                try {
                    suggestionLimit = std::stoi(*it);
                    it = args.erase(it);
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid number for --limit." << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --limit requires a number." << std::endl;
                return 1;
            }
        } else {
            ++it;
        }
    }
    
    if (testMode) {
        #ifdef LEKHIKA_SRC_DIR
            const char* srcDir = LEKHIKA_SRC_DIR;
            dataDir = fs::path(srcDir) / "core" / "data";
            std::cout << "[Test Mode]: Using local data files from: " << dataDir << std::endl;
        #else
            std::cerr << "Error: Test mode requires LEKHIKA_SRC_DIR to be set at compile time." << std::endl;
            return 1;
        #endif
    }


    if (args.empty()) {
        printHelp();
        return 0;
    }

    std::string command = args[0];

    //  Command Handling 
    if (command == "help") {
        printHelp();
        return 0;
    }
    if (command == "--version" || command == "version") {
        std::cout << "liblekhika version " << LEKHIKA_VERSION << std::endl;
        return 0;
    }

    Transliteration transliterator(dataDir);
    // Parse transliterator settings
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--disable-smart-correction") transliterator.setEnableSmartCorrection(false);
        if (args[i] == "--disable-autocorrect") transliterator.setEnableAutoCorrect(false);
        if (args[i] == "--disable-indic-numbers") transliterator.setEnableIndicNumbers(false);
        if (args[i] == "--disable-symbols") transliterator.setEnableSymbolsTransliteration(false);
    }


    if (command == "transliterate") {
        if (args.size() < 2) {
            std::cerr << "Usage: lekhika-cli transliterate <text_to_transliterate>" << std::endl;
            return 1;
        }
        std::cout << transliterator.transliterate(args[1]) << std::endl;
    }
#ifdef HAVE_SQLITE3
    else { // Dictionary related commands
        std::unique_ptr<DictionaryManager> dictManager = std::make_unique<DictionaryManager>();
        dictManager->setSuggestionLimit(suggestionLimit);
        
        if (command == "add-word") {
            if (args.size() < 2) {
                 std::cerr << "Usage: lekhika-cli add-word <devanagari_word>" << std::endl; return 1;
            }
            if (!isValidDevanagariWord(args[1])) {
                std::cerr << "Warning: Input is not a valid Devanagari word. Word not added." << std::endl;
                return 1;
            }
            dictManager->addWord(args[1]);
            std::cout << "Added '" << args[1] << "' to the dictionary." << std::endl;
        }
        else if (command == "find-word" || command == "suggest") {
            if (args.size() < 2) {
                std::cerr << "Usage: lekhika-cli " << command << " <prefix>" << std::endl; return 1;
            }
            std::string term = args[1];
            if (!isValidDevanagariWord(term)) {
                term = transliterator.transliterate(term);
            }
            auto words = dictManager->findWords(term, dictManager->getSuggestionLimit());
            if (words.empty()) {
                std::cout << "No suggestions found for '" << args[1] << "' -> '" << term << "'." << std::endl;
            } else {
                for (const auto& word : words) {
                    std::cout << word << std::endl;
                }
            }
        }
        else if (command == "learn-from-file") {
            if (args.size() < 2) {
                std::cerr << "Usage: lekhika-cli learn-from-file <path_to_file>" << std::endl; return 1;
            }
            try {
                long count = dictManager->learnFromFile(args[1]);
                std::cout << "Successfully learned " << count << " new words from " << args[1] << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                return 1;
            }
        }
        else if (command == "list-words") {
            auto words = dictManager->getAllWords(25);
            if (words.empty()) {
                std::cout << "User dictionary is empty." << std::endl;
            } else {
                 for (const auto& pair : words) {
                    std::cout << pair.first << " (freq: " << pair.second << ")" << std::endl;
                }
            }
        }
        else if (command == "search-db") {
             if (args.size() < 2) {
                std::cerr << "Usage: lekhika-cli search-db <term>" << std::endl; return 1;
            }
            std::string term = args[1];
             if (!isValidDevanagariWord(term)) {
                term = transliterator.transliterate(term);
            }
            auto words = dictManager->searchWords(term);
            if (words.empty()) {
                 std::cout << "No matches found for '" << args[1] << "' -> '" << term << "'." << std::endl;
            } else {
                for (const auto& pair : words) {
                    std::cout << pair.first << " (freq: " << pair.second << ")" << std::endl;
                }
            }
        }
         else if (command == "db-info") {
            auto info = dictManager->getDatabaseInfo();
            if (info.empty()) {
                std::cerr << "Could not retrieve database information." << std::endl;
            } else {
                for (const auto& pair : info) {
                    std::cout << pair.first << ": " << pair.second << std::endl;
                }
                 if (info.count("word_count") && info["word_count"] == "0") {
                    std::cout << "\nYour dictionary is empty. You can add words using 'add-word' or learn from a file.\n";
                    std::cout << "You can also download a pre-trained dictionary:\n";
                    std::cout << "  curl -L -o " << info["db_path"] << " https://github.com/khumnath/fcitx5-lekhika/releases/download/dictionary/lekhikadict.akshardb\n";
                }
            }
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            printHelp();
            return 1;
        }
    }
#else
    else {
        std::cerr << "Error: This version of lekhika-cli was compiled without dictionary support." << std::endl;
        std::cerr << "Please install the 'sqlite3' development libraries and recompile." << std::endl;
        return 1;
    }
#endif

    return 0;
}

void printHelp() {
    std::cout << "Lekhika Command-Line Tool\n";
    std::cout << "Version: " << getLekhikaVersion() << "\n\n";
    std::cout << "Usage: lekhika-cli [-test] <command> [arguments] [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  transliterate <text>      Transliterates Latin text to Devanagari.\n";
    std::cout << "  version, --version        Display the library version.\n";
    std::cout << "  help                      Show this help message.\n";
#ifdef HAVE_SQLITE3
    std::cout << "\nDictionary Commands (require SQLite):\n";
    std::cout << "  add-word <devanagari_word>  Adds a valid Devanagari word to the dictionary.\n";
    std::cout << "  find-word <prefix>        Finds matching words for a prefix.\n";
    std::cout << "  suggest <prefix>          Alias for find-word.\n";
    std::cout << "  learn-from-file <path>    Learns all valid words from a text file.\n";
    std::cout << "  list-words                Lists up to 25 words from the dictionary.\n";
    std::cout << "  search-db <term>          Searches for a term anywhere in a word.\n";
    std::cout << "  db-info                   Displays information and location of the user dictionary.\n";
    std::cout << "\nTo replace your dictionary, you can use the path from 'db-info'. Example:\n";
    std::cout << "  curl -L -o \"$(lekhika-cli db-info | grep db_path | cut -d' ' -f2)\" <url_to_db>\n";
#endif
    std::cout << "\nOptions:\n";
    std::cout << "  -test                       Use local data files (for development).\n";
    std::cout << "  --limit <number>            Set the number of suggestions to return.\n";
    std::cout << "  --disable-smart-correction  Disable smart correction rules.\n";
    std::cout << "  --disable-autocorrect       Disable autocorrect from TOML file.\n";
    std::cout << "  --disable-indic-numbers     Do not transliterate ASCII numbers.\n";
    std::cout << "  --disable-symbols           Do not transliterate symbols.\n";
}

