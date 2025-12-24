/*
 * TOON (Token-Oriented Object Notation) Library for C++11
 *
 * Copyright (c) 2025 Gemini + Emanuele Luzzu
 *
 * This software is released under the MIT License.
 * See the LICENSE file in the project root for full license information.
 *
 * Based on the [Json11](https://github.com/dropbox/json11) API design by
 * Dropbox, Inc.
 */

/* TOON (Token-Oriented Object Notation)
 *
 * TOON is a compact, human-readable serialization format designed for LLM token
 * efficiency. This library provides TOON parsing and serialization, mirroring
 * the json11 API.
 *
 * TOON Specification:
 * - Objects: Indentation-based nesting, no curly braces.
 * - Arrays: Explicit length [N]: v1, v2, ...
 * - Tabular Objects: List of objects with same keys use a header e.g. [{k1,
 * k2}]: v1a, v1b, ...
 * - Strings: Quoting only when necessary.
 */

#pragma once

#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace toon {

enum ToonParse { STANDARD };

class ToonValue;

class Toon final {
public:
  // Types
  enum Type { NUL, NUMBER, BOOL, STRING, ARRAY, OBJECT };

  // Array and object typedefs
  typedef std::vector<Toon> array;
  typedef std::map<std::string, Toon> object;

  // Constructors
  Toon() noexcept;                // NUL
  Toon(std::nullptr_t) noexcept;  // NUL
  Toon(double value);             // NUMBER
  Toon(int value);                // NUMBER
  Toon(bool value);               // BOOL
  Toon(const std::string &value); // STRING
  Toon(std::string &&value);      // STRING
  Toon(const char *value);        // STRING
  Toon(const array &values);      // ARRAY
  Toon(array &&values);           // ARRAY
  Toon(const object &values);     // OBJECT
  Toon(object &&values);          // OBJECT

  // Implicit constructors (same as json11)
  template <class T, class = decltype(&T::to_toon)>
  Toon(const T &t) : Toon(t.to_toon()) {}

  template <
      class M,
      typename std::enable_if<
          std::is_constructible<
              std::string, decltype(std::declval<M>().begin()->first)>::value &&
              std::is_constructible<
                  Toon, decltype(std::declval<M>().begin()->second)>::value,
          int>::type = 0>
  Toon(const M &m) : Toon(object(m.begin(), m.end())) {}

  template <class V, typename std::enable_if<
                         std::is_constructible<
                             Toon, decltype(*std::declval<V>().begin())>::value,
                         int>::type = 0>
  Toon(const V &v) : Toon(array(v.begin(), v.end())) {}

  Toon(void *) = delete;

  // Accessors
  Type type() const;

  bool is_null() const { return type() == NUL; }
  bool is_number() const { return type() == NUMBER; }
  bool is_bool() const { return type() == BOOL; }
  bool is_string() const { return type() == STRING; }
  bool is_array() const { return type() == ARRAY; }
  bool is_object() const { return type() == OBJECT; }

  double number_value() const;
  int int_value() const;
  bool bool_value() const;
  const std::string &string_value() const;
  const array &array_items() const;
  const object &object_items() const;

  const Toon &operator[](size_t i) const;
  const Toon &operator[](const std::string &key) const;

  // Serialize
  void dump(std::string &out, int indent_level = 0) const;
  std::string dump() const {
    std::string out;
    dump(out);
    return out;
  }

  // Parse
  static Toon parse(const std::string &in, std::string &err,
                    ToonParse strategy = ToonParse::STANDARD);

  bool operator==(const Toon &rhs) const;
  bool operator<(const Toon &rhs) const;
  bool operator!=(const Toon &rhs) const { return !(*this == rhs); }

private:
  std::shared_ptr<ToonValue> m_ptr;
};

// Internal class hierarchy
class ToonValue {
protected:
  friend class Toon;
  virtual Toon::Type type() const = 0;
  virtual bool equals(const ToonValue *other) const = 0;
  virtual bool less(const ToonValue *other) const = 0;
  virtual void dump(std::string &out, int indent_level) const = 0;
  virtual double number_value() const;
  virtual int int_value() const;
  virtual bool bool_value() const;
  virtual const std::string &string_value() const;
  virtual const Toon::array &array_items() const;
  virtual const Toon &operator[](size_t i) const;
  virtual const Toon::object &object_items() const;
  virtual const Toon &operator[](const std::string &key) const;
  virtual ~ToonValue() {}
};

} // namespace toon
