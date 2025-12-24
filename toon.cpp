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

#include "toon.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace helper_toon {
static uint32_t parse_unicode_codepoint(const std::string &str, size_t &i,
                                        std::string &err) {
  uint32_t val = 0;
  if (i + 4 >= str.size()) {
    err = "unfinished unicode escape";
    return 0;
  }
  for (int j = 0; j < 4; j++) {
    char c = str[i++];
    val <<= 4;
    if (c >= '0' && c <= '9')
      val += c - '0';
    else if (c >= 'a' && c <= 'f')
      val += c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      val += c - 'A' + 10;
    else {
      err = "invalid unicode escape";
      return 0;
    }
  }
  return val;
}

static void encode_utf8(uint32_t cp, std::string &out) {
  // Validazione range Unicode
  if (cp > 0x10ffff) {
    // Codepoint fuori range Unicode
    cp = 0xfffd; // Replacement character
  }

  // Validazione codepoint riservati/surrogati
  if ((cp >= 0xd800 && cp <= 0xdfff) || (cp >= 0xfdd0 && cp <= 0xfdef) ||
      (cp & 0xfffe) == 0xfffe) {
    cp = 0xfffd; // Replacement character
  }

  if (cp <= 0x7f) {
    // 1 byte: 0xxxxxxx (ASCII)
    out += static_cast<char>(cp);
  } else if (cp <= 0x7ff) {
    // 2 bytes: 110xxxxx 10xxxxxx
    out += static_cast<char>(0xc0 | ((cp >> 6) & 0x1f));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  } else if (cp <= 0xffff) {
    // 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
    out += static_cast<char>(0xe0 | ((cp >> 12) & 0x0f));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  } else {
    // 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    out += static_cast<char>(0xf0 | ((cp >> 18) & 0x07));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  }
}

} // end namespace helper_toon

namespace toon {

using std::make_shared;
using std::map;
using std::move;
using std::string;
using std::vector;

static void dump(std::nullptr_t, string &out, int) { out += "null"; }

static void dump(double value, string &out, int) {
  if (std::isfinite(value)) {
    char buf[32];
    snprintf(buf, sizeof buf, "%.17g", value);
    out += buf;
  } else {
    out += "null";
  }
}

static void dump(int value, string &out, int) {
  char buf[32];
  snprintf(buf, sizeof buf, "%d", value);
  out += buf;
}

static void dump(bool value, string &out, int) {
  out += value ? "true" : "false";
}

static bool needs_quoting(const string &value) {
  if (value.empty())
    return true;
  if (value == "null" || value == "true" || value == "false")
    return true;
  if (isdigit(value[0]) || value[0] == '-') {
    // Check if it's a valid number
    char *end;
    strtod(value.c_str(), &end);
    if (*end == '\0')
      return true;
  }
  const string special = ",:\n[]{}#";
  for (char c : value) {
    if (special.find(c) != string::npos)
      return true;
  }
  return false;
}

static void dump(const string &value, string &out, int) {
  if (!needs_quoting(value)) {
    out += value;
    return;
  }
  out += '"';
  for (size_t i = 0; i < value.length(); i++) {
    const char ch = value[i];
    if (ch == '\\')
      out += "\\\\";
    else if (ch == '"')
      out += "\\\"";
    else if (ch == '\b')
      out += "\\b";
    else if (ch == '\f')
      out += "\\f";
    else if (ch == '\n')
      out += "\\n";
    else if (ch == '\r')
      out += "\\r";
    else if (ch == '\t')
      out += "\\t";
    else if (static_cast<uint8_t>(ch) <= 0x1f) {
      char buf[8];
      snprintf(buf, sizeof buf, "\\u%04x", ch);
      out += buf;
    } else
      out += ch;
  }
  out += '"';
}

static void indent(string &out, int level) {
  for (int i = 0; i < level; ++i)
    out += "  ";
}

static void dump(const Toon::array &values, string &out, int level) {
  if (values.empty()) {
    out += "[0]:";
    return;
  }

  // Check for tabular format possibility
  bool all_objects = true;
  vector<string> keys;
  if (values[0].is_object()) {
    for (auto const &kv : values[0].object_items())
      keys.push_back(kv.first);
    for (size_t i = 1; i < values.size(); ++i) {
      if (!values[i].is_object() ||
          values[i].object_items().size() != keys.size()) {
        all_objects = false;
        break;
      }
      for (auto const &k : keys) {
        if (values[i].object_items().find(k) ==
            values[i].object_items().end()) {
          all_objects = false;
          break;
        }
      }
      if (!all_objects)
        break;
    }
  } else {
    all_objects = false;
  }

  if (all_objects && !keys.empty()) {
    out += "[{";
    for (size_t i = 0; i < keys.size(); ++i) {
      out += keys[i];
      if (i < keys.size() - 1)
        out += ", ";
    }
    out += "}]:\n";
    for (size_t i = 0; i < values.size(); ++i) {
      indent(out, level + 1);
      auto const &obj = values[i].object_items();
      for (size_t j = 0; j < keys.size(); ++j) {
        obj.at(keys[j]).dump(out, level + 1);
        if (j < keys.size() - 1)
          out += ", ";
      }
      if (i < values.size() - 1)
        out += "\n";
    }
  } else {
    out += "[";
    out += std::to_string(values.size());
    out += "]: ";
    for (size_t i = 0; i < values.size(); ++i) {
      values[i].dump(out, level);
      if (i < values.size() - 1)
        out += ", ";
    }
  }
}

static void dump(const Toon::object &values, string &out, int level) {
  bool first = true;
  for (auto const &kv : values) {
    if (!first) {
      out += "\n";
      indent(out, level);
    }
    out += kv.first;
    out += ": ";
    if (kv.second.is_object() || kv.second.is_array()) {
      if (kv.second.is_object())
        out += "\n";
      if (kv.second.is_object())
        indent(out, level + 1);
      kv.second.dump(out, level + 1);
    } else {
      kv.second.dump(out, level);
    }
    first = false;
  }
}

void Toon::dump(string &out, int level) const { m_ptr->dump(out, level); }

/* Value Wrappers */
template <Toon::Type tag, typename T> class Value : public ToonValue {
protected:
  explicit Value(const T &value) : m_value(value) {}
  explicit Value(T &&value) : m_value(move(value)) {}

  Toon::Type type() const override { return tag; }
  bool equals(const ToonValue *other) const override {
    return m_value == static_cast<const Value<tag, T> *>(other)->m_value;
  }
  bool less(const ToonValue *other) const override {
    return m_value < static_cast<const Value<tag, T> *>(other)->m_value;
  }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
  const T m_value;
#pragma GCC diagnostic pop
  void dump(string &out, int level) const override {
    toon::dump(m_value, out, level);
  }
};

template <>
bool Value<Toon::NUL, std::nullptr_t>::less(const ToonValue *) const {
  return false;
}

class ToonDouble final : public Value<Toon::NUMBER, double> {
  double number_value() const override { return m_value; }
  int int_value() const override { return static_cast<int>(m_value); }

public:
  explicit ToonDouble(double value) : Value(value) {}
};

class ToonInt final : public Value<Toon::NUMBER, int> {
  double number_value() const override { return m_value; }
  int int_value() const override { return m_value; }

public:
  explicit ToonInt(int value) : Value(value) {}
};

class ToonBoolean final : public Value<Toon::BOOL, bool> {
  bool bool_value() const override { return m_value; }

public:
  explicit ToonBoolean(bool value) : Value(value) {}
};

class ToonString final : public Value<Toon::STRING, string> {
  const string &string_value() const override { return m_value; }

public:
  explicit ToonString(const string &value) : Value(value) {}
  explicit ToonString(string &&value) : Value(move(value)) {}
};

class ToonArray final : public Value<Toon::ARRAY, Toon::array> {
  const Toon::array &array_items() const override { return m_value; }
  const Toon &operator[](size_t i) const override;

public:
  explicit ToonArray(const Toon::array &value) : Value(value) {}
  explicit ToonArray(Toon::array &&value) : Value(move(value)) {}
};

class ToonObject final : public Value<Toon::OBJECT, Toon::object> {
  const Toon::object &object_items() const override { return m_value; }
  const Toon &operator[](const string &key) const override;

public:
  explicit ToonObject(const Toon::object &value) : Value(value) {}
  explicit ToonObject(Toon::object &&value) : Value(move(value)) {}
};

class ToonNull final : public Value<Toon::NUL, std::nullptr_t> {
public:
  ToonNull() : Value(nullptr) {}
};

/* Statics */
struct Statics {
  const std::shared_ptr<ToonValue> null = make_shared<ToonNull>();
  const std::shared_ptr<ToonValue> t = make_shared<ToonBoolean>(true);
  const std::shared_ptr<ToonValue> f = make_shared<ToonBoolean>(false);
  const string empty_string;
  const vector<Toon> empty_vector;
  const map<string, Toon> empty_map;
  Statics() {}
};

static const Statics &statics() {
  static const Statics s{};
  return s;
}

static const Toon &static_null() {
  static const Toon json_null;
  return json_null;
}

const Toon &ToonArray::operator[](size_t i) const {
  if (i >= m_value.size())
    return static_null();
  else
    return m_value[i];
}

const Toon &ToonObject::operator[](const string &key) const {
  auto iter = m_value.find(key);
  return (iter == m_value.end()) ? static_null() : iter->second;
}

Toon::Toon() noexcept : m_ptr(statics().null) {}
Toon::Toon(std::nullptr_t) noexcept : m_ptr(statics().null) {}
Toon::Toon(double value) : m_ptr(make_shared<ToonDouble>(value)) {}
Toon::Toon(int value) : m_ptr(make_shared<ToonInt>(value)) {}
Toon::Toon(bool value) : m_ptr(value ? statics().t : statics().f) {}
Toon::Toon(const string &value) : m_ptr(make_shared<ToonString>(value)) {}
Toon::Toon(string &&value) : m_ptr(make_shared<ToonString>(move(value))) {}
Toon::Toon(const char *value) : m_ptr(make_shared<ToonString>(value)) {}
Toon::Toon(const Toon::array &values) : m_ptr(make_shared<ToonArray>(values)) {}
Toon::Toon(Toon::array &&values)
    : m_ptr(make_shared<ToonArray>(move(values))) {}
Toon::Toon(const Toon::object &values)
    : m_ptr(make_shared<ToonObject>(values)) {}
Toon::Toon(Toon::object &&values)
    : m_ptr(make_shared<ToonObject>(move(values))) {}

Toon::Type Toon::type() const { return m_ptr->type(); }
double Toon::number_value() const { return m_ptr->number_value(); }
int Toon::int_value() const { return m_ptr->int_value(); }
bool Toon::bool_value() const { return m_ptr->bool_value(); }
const string &Toon::string_value() const { return m_ptr->string_value(); }
const Toon::array &Toon::array_items() const { return m_ptr->array_items(); }
const Toon::object &Toon::object_items() const { return m_ptr->object_items(); }
const Toon &Toon::operator[](size_t i) const { return (*m_ptr)[i]; }
const Toon &Toon::operator[](const string &key) const { return (*m_ptr)[key]; }

double ToonValue::number_value() const { return 0; }
int ToonValue::int_value() const { return 0; }
bool ToonValue::bool_value() const { return false; }
const string &ToonValue::string_value() const { return statics().empty_string; }
const Toon::array &ToonValue::array_items() const {
  return statics().empty_vector;
}
const Toon::object &ToonValue::object_items() const {
  return statics().empty_map;
}
const Toon &ToonValue::operator[](size_t) const {
  static const Toon null;
  return null;
}
const Toon &ToonValue::operator[](const string &) const {
  static const Toon null;
  return null;
}

bool Toon::operator==(const Toon &other) const {
  if (m_ptr == other.m_ptr)
    return true;
  if (m_ptr->type() != other.m_ptr->type())
    return false;
  return m_ptr->equals(other.m_ptr.get());
}

bool Toon::operator<(const Toon &other) const {
  if (m_ptr == other.m_ptr)
    return false;
  if (m_ptr->type() != other.m_ptr->type())
    return m_ptr->type() < other.m_ptr->type();
  return m_ptr->less(other.m_ptr.get());
}

/* Parser Implementation */
struct ToonParser {
  const string &str;
  size_t i;
  string &err;
  bool failed;

  Toon fail(string &&msg) {
    if (!failed)
      err = std::move(msg);
    failed = true;
    return Toon();
  }

  void consume_whitespace() {
    while (i < str.size() &&
           (str[i] == ' ' || str[i] == '\r' || str[i] == '\t'))
      i++;
  }

  void consume_garbage() {
    while (true) {
      consume_whitespace();
      if (i < str.size() && str[i] == '#') { // Comment
        while (i < str.size() && str[i] != '\n')
          i++;
      } else if (i < str.size() && str[i] == '\n') {
        i++;
      } else {
        break;
      }
    }
  }

  char get_next_token() {
    consume_whitespace();
    if (i == str.size())
      return 0;
    return str[i++];
  }

  int get_indent() {
    int count = 0;
    size_t start = i;
    while (start > 0 && str[start - 1] != '\n')
      start--;
    while (start < str.size() && (str[start] == ' ' || str[start] == '\t')) {
      count += (str[start] == '\t' ? 8 : 1); // rough tab approximation
      start++;
    }
    return count;
  }

  Toon parse_value() {
    consume_whitespace();
    if (i == str.size())
      return fail("unexpected end of input");

    char ch = str[i];
    if (ch == '"')
      return parse_quoted_string();
    if (ch == '[')
      return parse_array();

    // Check for special keywords
    if (str.compare(i, 4, "null") == 0) {
      i += 4;
      return Toon();
    }
    if (str.compare(i, 4, "true") == 0) {
      i += 4;
      return Toon(true);
    }
    if (str.compare(i, 5, "false") == 0) {
      i += 5;
      return Toon(false);
    }

    if (isdigit(ch) || ch == '-')
      return parse_number();

    return parse_unquoted_string();
  }

  Toon parse_quoted_string() {
    i++; // skip "
    string out;
    while (i < str.size() && str[i] != '"') {
      if (str[i] == '\\') {
        i++;
        if (str[i] == 'u') {
          string unicode_err;
          uint32_t cp =
              helper_toon::parse_unicode_codepoint(str, i, unicode_err);
          if (!unicode_err.empty())
            return fail(std::move(unicode_err));
          helper_toon::encode_utf8(cp, out);
        }
        if (i == str.size())
          return fail("unfinished escape");
        char esc = str[i++];
        if (esc == 'n')
          out += '\n';
        else if (esc == 'r')
          out += '\r';
        else if (esc == 't')
          out += '\t';
        else if (esc == '"' || esc == '\\')
          out += esc;
        else
          return fail("invalid escape");
      } else {
        out += str[i++];
      }
    }
    if (i == str.size())
      return fail("unfinished string");
    i++; // skip "
    return out;
  }

  Toon parse_unquoted_string() {
    string out;
    const string special = ",:\n[]{}#";
    while (i < str.size() && special.find(str[i]) == string::npos) {
      out += str[i++];
    }
    // Trim trailing whitespace
    size_t last = out.find_last_not_of(" \t\r");
    if (last != string::npos)
      out.erase(last + 1);
    return out;
  }

  Toon parse_number() {
    size_t start = i;
    while (i < str.size() && (isdigit(str[i]) || str[i] == '.' ||
                              str[i] == '-' || str[i] == 'e' || str[i] == 'E'))
      i++;
    string n = str.substr(start, i - start);

    // std::stod dipende dal locale. Meglio usare strtod_l se disponibile o
    // forzare il punto. Metodo semplice "C-style" manuale o strtod standard
    // (assumendo locale "C" per i dati):
    char *end;
    double val = strtod(n.c_str(), &end);
    return val;
  }

  Toon parse_array(int parent_indent = -1) {
    i++; // skip [
    bool tabular = false;
    vector<string> keys;
    int count = -1;

    // 1. Parsing dell'header: [N] oppure [{k1, k2}]
    if (i < str.size() && str[i] == '{') {
      tabular = true;
      i++; // skip {
      while (i < str.size() && str[i] != '}') {
        consume_whitespace();
        string key;
        while (i < str.size() && str[i] != ',' && str[i] != '}')
          key += str[i++];

        // Trim key
        size_t first = key.find_first_not_of(" ");
        size_t last = key.find_last_not_of(" ");
        if (first != string::npos)
          keys.push_back(key.substr(first, (last - first + 1)));

        if (i < str.size() && str[i] == ',')
          i++;
      }
      if (i < str.size())
        i++; // skip }
    } else {
      string n;
      while (i < str.size() && isdigit(str[i]))
        n += str[i++];
      if (!n.empty())
        count = std::stoi(n);
    }

    // Skip closing bracket and optional colon
    while (i < str.size() && str[i] != ']')
      i++;
    if (i < str.size())
      i++; // skip ]
    if (i < str.size() && str[i] == ':')
      i++;

    Toon::array arr;

    // 2. Parsing del corpo
    if (tabular) {
      // In modalità tabulare, leggiamo finché l'indentazione regge
      while (i < str.size()) {
        consume_garbage(); // Salta commenti e newlines precedenti

        // Se siamo alla fine del file, stop
        if (i == str.size())
          break;

        // Verifica Indentazione
        // Salviamo la posizione corrente per fare il peek dell'indentazione
        size_t row_start_index = i;
        int current_indent = get_indent();

        // LOGICA CRITICA:
        // Se l'indentazione della nuova riga è <= all'indentazione del
        // genitore, significa che l'array tabulare è finito e siamo tornati al
        // livello superiore. Nota: parent_indent == -1 significa root, quindi
        // accettiamo tutto.
        if (parent_indent != -1 && current_indent <= parent_indent) {
          // Ripristiniamo l'indice in modo che il genitore possa leggere questa
          // riga
          i = row_start_index;
          break;
        }

        // Se l'indentazione è valida, parsiamo la riga come oggetto
        Toon::object obj;
        bool row_failed = false;

        for (size_t j = 0; j < keys.size(); ++j) {
          consume_whitespace();

          // Gestione fine inaspettata della riga
          if (i == str.size() || str[i] == '\n') {
            // Se mancano colonne, decidiamo se fallire o mettere null.
            // Qui interrompiamo la riga.
            row_failed = true;
            break;
          }

          Toon val = parse_value();
          if (failed)
            return Toon(); // Propaga errore critico

          obj[keys[j]] = val;
          consume_whitespace();

          // Salta la virgola se presente tra le colonne
          if (j < keys.size() - 1 && i < str.size() && str[i] == ',')
            i++;
        }

        if (!row_failed) {
          arr.push_back(obj);
        } else {
          // Se la riga è fallita o incompleta, potremmo voler uscire
          // o semplicemente ignorarla. Qui usciamo per sicurezza.
          break;
        }
      }
    } else {
      // Modalità Standard [N]: v1, v2, ...
      // Qui l'indentazione è meno rilevante perché abbiamo il count esplicito,
      // ma è comunque utile consumare whitespace correttamente.
      for (int k = 0; count < 0 || k < count; ++k) {
        consume_garbage(); // Importante per array multilinea

        if (i == str.size())
          break;

        // Check opzionale: se siamo in array senza count esplicito (se mai
        // supportato) potremmo usare la stessa logica dell'indentazione qui.
        // Per ora ci fidiamo di 'count'.

        arr.push_back(parse_value());

        consume_whitespace();
        if (i < str.size() && str[i] == ',')
          i++;
        else if (count < 0) // Se count non c'è, stop al primo che non ha
                            // virgola (o newline)
          break;
      }
    }
    return arr;
  }

  Toon parse_object(int parent_indent) {
    Toon::object obj;
    while (i < str.size()) {
      consume_garbage();
      size_t saved_i = i;
      int current_indent = get_indent();

      // Controllo uscita dall'oggetto corrente
      if (current_indent <= parent_indent && !obj.empty()) {
        i = saved_i;
        break;
      }

      string key;
      while (i < str.size() && str[i] != ':')
        key += str[i++];
      if (i == str.size())
        break;
      i++; // skip :

      consume_whitespace();

      if (i < str.size() && str[i] == '\n') {
        // Valore su una nuova riga (nested object o array)
        i++;               // skip \n
        consume_garbage(); // posizionati all'inizio della riga successiva

        // Importante: ricalcoliamo l'indentazione del contenuto annidato
        int nested_indent = get_indent();

        if (str[i] == '[') {
          // PASSAGGIO CHIAVE: passiamo nested_indent (o current_indent)
          // Solitamente l'array è indentato rispetto alla chiave che lo
          // contiene. Se la sintassi è: key:
          //   [3]: ...
          // Allora nested_indent è corretto.
          obj[key] = parse_array(current_indent);
        } else {
          obj[key] = parse_object(current_indent);
        }
      } else {
        // Valore inline
        if (str[i] == '[') {
          // Array inline sulla stessa riga: key: [3]: ...
          // In questo caso passiamo current_indent perché fa parte della stessa
          // riga logica
          obj[key] = parse_array(current_indent);
        } else {
          obj[key] = parse_value();
        }
      }
    }
    return obj;
  }
};

Toon Toon::parse(const string &in, string &err, ToonParse) {
  ToonParser parser{in, 0, err, false};
  parser.consume_garbage();
  if (parser.i == in.size())
    return Toon();

  // If it starts with [ it is definitely not a top-level object
  if (in[parser.i] == '[')
    return parser.parse_value();

  // If it contains : it might be an object, but we need to check if it's not
  // inside quotes/brackets For simplicity, let's assume if it has : and doesn't
  // start with [, it's an object. Actually, unquoted strings could contain :?
  // No, : is a special char.
  if (in.find(':') != string::npos) {
    return parser.parse_object(-1);
  } else {
    return parser.parse_value();
  }
}

} // namespace toon
