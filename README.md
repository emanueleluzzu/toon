# TOON Library for C++11

## Implemented by Google Gemini 3 Flash and me

A brand new [TOON](https://github.com/toon-format/toon) (Token-Oriented Object Notation) library in C++11, mirroring the API of `json11`. TOON is designed specifically for LLM token efficiency, providing a compact yet human-readable format.

## Key Features

- **Token Efficiency**: Designed to minimize token consumption in LLM contexts.
- **Unquoted Strings**: Automatically handles strings without quotes when they don't contain special characters or ambiguity.
- **Tabular Arrays**: Uniform objects are serialized in a schema-aware tabular format `[{k1, k2}]: v1, v2` to drastically reduce repetition of keys.
- **Indentation-Aware**: Replaces curly braces with YAML-like indentation for object nesting.
- **C++11 Compatible**: Built with standard C++11, using `shared_ptr` for memory management and a clean, familiar API.

## API Usage

The API is designed to be a drop-in replacement for `json11`:

```cpp
#include "toon.h"
#include <iostream>

using namespace toon;

int main() {
    // Construction
    Toon obj = Toon::object {
        {"name", "Alice"},
        {"stats", Toon::array {10, 20, 30}},
        {"meta", Toon::object {{"id", 1}}}
    };

    // Serialization
    std::string output = obj.dump();
    std::cout << output << std::endl;

    // Parsing
    std::string err;
    Toon parsed = Toon::parse(output, err);
    if (err.empty()) {
        std::cout << "Name: " << parsed["name"].string_value() << std::endl;
    }
}
```
## Implementation Details

### Core Logic
- *Toon Class*: A polymorphic wrapper around ToonValue using std::shared_ptr.
- *dump*: Implements the TOON specification, including the detection of "all-object" arrays to trigger tabular serialization.
- *parse*: A recursive descent parser that tracks indentation levels to define object boundaries and handles the unique [N]: and [{keys}]: array headers.


### toon.h
The header file defines the `Toon` class and its interface. It supports all standard JSON-compatible types.

### toon.cpp
Contains the core logic for serialization (`dump`) and parsing (`parse`).
Highlights include:

- `dump`: Implements tabular array detection and indentation logic.
- `parse`: A recursive descent parser that handles indentation levels and tabular headers.

## Verification Results

### Automated Tests
The library includes a test suite `toon_test.cpp` that verifies:

- *Basic Types*: Precision of numbers, boolean literals, and special character escaping in strings.
- *Indentation Logic*: Correct nesting and un-nesting of objects based on leading whitespace.
- *Tabular Robustness*: Parsing of tabular arrays even when nested deep within objects.
- *Unicode/Escaping*: Support for standard escape sequences and (optional) UTF-8.

### To run the tests:
```bash
g++ -std=c++11 toon.cpp toon_test.cpp -o toon_test && ./toon_test
```

### Expected output:
```log
Results:

Basic tests passed!
Object dump:
age: 30
city: New York
name: Alice
Object tests passed!
Array tests passed!
Tabular dump:
[{x, y}]:
  1, 2
  3, 4
Tabular tests passed!
All tests passed!
```

## TOON Specification Support

[V] *Unquoted strings (when safe)*
[V] *Indentation-based objects*
[V] *Explicit length arrays [N]*
[V] *Tabular objects [{k1, k2}]*
[V] *Comments (prefixed with #)*

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
Developed by Gemini + Emanuele Luzzu.