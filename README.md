

# Lekhika Transliteration Engine & CLI Tool

Lekhika is a versatile, high-performance C++ library for transliterating Indic scripts, with a primary focus on converting Latin (Roman) text to Devanagari.
 It is designed to be a completely independent core module that can be easily integrated into various input methods and applications. It serves as the core transliteration engine for the [**fcitx5-lekhika**](https://github.com/khumnath/fcitx5-lekhika "null") input method for Linux.

This project includes:

1. `liblekhika`: A core shared library (`.so`) that handles all transliteration, dictionary, and word validation logic.

2. `lekhika-cli`: A command-line tool for testing the library's features, managing the user dictionary, and performing transliteration from the terminal.

## Features

* **Rules-Based Transliteration:** High-speed conversion of Latin text to Devanagari based on configurable mapping files.

* **Dictionary Support:** Optional integration with SQLite3 for word suggestions and user dictionary management.

* **Devanagari Word Validation:** Uses the ICU library to ensure that only correctly formed Devanagari words can be added to the dictionary.

* **Configurable Engine:** Transliteration behavior (smart corrections, number/symbol handling) can be enabled or disabled on the fly.

* **Portable Core:** The `liblekhika` library is completely standalone with no dependencies on any specific input method framework (like Fcitx5 or IBus).

* **Modern CMake Build:** A clean, modular build system that makes it easy to compile, install, and use the library in other projects.

## Dependencies

### Build-Time Dependencies

You will need the following development packages to build the project:

* A C++17 compatible compiler (e.g., g++)

* CMake (version 3.16 or newer)

* **ICU Libraries (development package):** Required for Unicode text processing and validation.

  * On Debian/Ubuntu: `sudo apt install libicu-dev`

* **SQLite3 (development package, optional):** Required for dictionary and word suggestion features.

  * On Debian/Ubuntu: `sudo apt install libsqlite3-dev`

### Runtime Dependencies

End-users will need the following libraries installed to run the CLI or any application using the library:

* ICU Libraries (runtime package)

* SQLite3 (runtime package, if built with dictionary support)

## Build and Install

Follow these steps from the top-level project directory (`lekhika-project/`).

**1. Create a build directory:**



```

mkdir build && cd build

```

**2. Configure the project with CMake:**



```

cmake -DCMAKE_INSTALL_PREFIX=/usr ..

```

**3. Compile the library and the CLI tool:**



```

make

```

**4. (Optional) Run the CLI tool for testing:**
You can test the command-line tool directly from the build directory without installing it.



```

./cli/lekhika-cli --version ./cli/lekhika-cli -test transliterate "namaste"

```

**5. Install the project system-wide:**
This will install the library, headers, data files, and CLI tool to standard system locations (e.g., under `/usr`).



```

sudo make install

```

**6. Update the System Library Cache (Crucial First-Time Step):**
After installing a new shared library for the first time, you must update the system's dynamic linker cache. This allows the system to find your newly installed `liblekhika.so` file.



```

sudo ldconfig

```

You only need to do this once after the initial installation.

## How to Uninstall

From your `build` directory, you can run the following command to remove all the files that were installed by this project.



```

sudo make uninstall

```

## Using the Command-Line Tool (`lekhika-cli`)

The CLI tool is a powerful utility for interacting with the `liblekhika` library.

### Basic Usage



```

lekhika-cli

options

<command>

arguments...

```

### Commands

* `version`: Display the version of the core library.

* `transliterate <text>`: Transliterate the given Latin text to Devanagari.

* `add-word <devanagari_word>`: Adds a valid Devanagari word to the user dictionary. Fails if the word is not valid.

* `find-word <prefix>`: Transliterates a prefix and finds matching words in the dictionary.

* `suggest <prefix>`: An alias for `find-word`.

* `learn-from-file <path>`: Reads a text file line by line and adds all valid Devanagari words to the dictionary.

* `db-info`: Displays information about the user dictionary, including its location.

* `list-words`: Lists the first 25 words from the user dictionary.

* `search-db <term>`: Searches the dictionary for a specific term (auto-transliterates if input is Latin).

* `help`: Shows the help message.

### Options

* `-test`: (For development) Use local data files (`mapping.toml`, etc.) from the source tree instead of the installed system files.

* `--version`: Display the library version.

* `--limit <number>`: Sets the number of suggestions to return for `find-word` and `suggest`.

* `--disable-smart-correction`: Disable smart correction rules.

* `--disable-auto-correct`: Disable auto-correction from `autocorrect.toml`.

* `--disable-indic-numbers`: Prevent transliteration of numbers to Devanagari digits.

* `--disable-symbols`: Prevent transliteration of symbols.

## Using the `liblekhika` Library in Other Projects

The library is designed to be easily consumed by other CMake projects.

**1. Find the package in your project's `CMakeLists.txt`:**



```
find_package(ICU REQUIRED COMPONENTS uc i18n)
find_package(SQLite3)
find_package(liblekhika REQUIRED)

```

**2. Link to the library:**



```

target_link_libraries(your_target_name PRIVATE liblekhika::liblekhika)

```

This automatically handles include directories, link libraries, and compile definitions (`HAVE_SQLITE3`).

**3. Include the header and use the classes:**



```

#include <liblekhika/lekhika_core.h> #include <iostream>

int main() { // The transliterator will find its data files automatically from the install location. Transliteration tl; std::cout << tl.transliterate("merhaba dunya") << std::endl;

#ifdef HAVE_SQLITE3 // The DictionaryManager will use the default user database path. DictionaryManager dict; dict.addWord("नमस्ते"); #endif

return 0;

}

```

## File Locations

After running `sudo make install`, the project files are placed in standard system locations.

| Installed Path                          | File(s) or Directory         | Description                                                                 |
|----------------------------------------|------------------------------|-----------------------------------------------------------------------------|
| `lib/`                                 | `liblekhika.so`              | Shared library binary for runtime linking                                  |
| `include/liblekhika/`                  | `lekhika_core.h`             | Public header for API exposure                                             |
| `share/liblekhika/`                    | `data/` contents             | Architecture-independent data files (e.g. templates, icons, metadata)      |
| `lib/cmake/liblekhika/`                | `liblekhika-config.cmake`    | CMake config file for consumers to find and use the library                |
| `lib/cmake/liblekhika/`                | `liblekhika-config-version.cmake` | Version file for compatibility checks                                 |
| `lib/cmake/liblekhika/`                | `liblekhika-targets.cmake`   | Exported targets for CMake integration                                     |
| `bin/`                                 | lekhika.cli | Runtime binaries          |

* **User Dictionary:** ~/.local/share/lekhika-core/lekhikadict.akshardb (This is your personal dictionary, which is automatically created by the application on first use).

## Contributing

Contributions are welcome! If you have a suggestion or find a bug, please open an issue on the project's GitHub page. If you would like to contribute code, please fork the repository and submit a pull request.

When contributing, please try to follow the existing code style and ensure that your changes are well-tested.

## License

This project is licensed under the GNU General Public License v3.0. You can find a copy of the license in the source files and at [https://www.gnu.org/licenses/gpl-3.0.html](https://www.gnu.org/licenses/gpl-3.0.html).