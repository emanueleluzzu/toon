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
#include <cassert>
#include <iostream>

using namespace toon;
using namespace std;

void test_basic() {
  Toon t1 = 42;
  assert(t1.dump() == "42");

  Toon t2 = "hello world";
  assert(t2.dump() == "hello world");

  Toon t3 = "hello, world";
  assert(t3.dump() == "\"hello, world\"");

  Toon t4 = true;
  assert(t4.dump() == "true");

  cout << "Basic tests passed!" << endl;
}

void test_object() {
  Toon obj = Toon::object{{"name", "Alice"}, {"age", 30}, {"city", "New York"}};
  string d = obj.dump();
  cout << "Object dump:\n" << d << endl;

  string err;
  Toon parsed = Toon::parse(d, err);
  if (!err.empty())
    cout << "Parse error: " << err << endl;
  assert(err.empty());
  assert(parsed["name"].string_value() == "Alice");
  assert(parsed["age"].int_value() == 30);

  cout << "Object tests passed!" << endl;
}

void test_array() {
  Toon arr = Toon::array{1, 2, 3};
  assert(arr.dump() == "[3]: 1, 2, 3");

  Toon empty = Toon::array{};
  assert(empty.dump() == "[0]:");

  cout << "Array tests passed!" << endl;
}

void test_tabular() {
  Toon arr = Toon::array{Toon::object{{"x", 1}, {"y", 2}},
                         Toon::object{{"x", 3}, {"y", 4}}};
  string d = arr.dump();
  cout << "Tabular dump:\n" << d << endl;

  string err;
  Toon parsed = Toon::parse(d, err);
  if (!err.empty())
    cout << "Parse error: " << err << endl;
  assert(err.empty());
  assert(parsed.is_array());
  assert(parsed.array_items().size() == 2);
  assert(parsed[0]["x"].int_value() == 1);
  assert(parsed[1]["y"].int_value() == 4);

  cout << "Tabular tests passed!" << endl;
}

int main() {
  test_basic();
  test_object();
  test_array();
  test_tabular();
  cout << "All tests passed!" << endl;
  return 0;
}
