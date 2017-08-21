// RUN: %clang_cc1 -analyze -analyzer-checker=core -analyzer-config record-coverage=%T %s
// RUN: FileCheck -input-file %T/%s.gcov %s.expected

int main() {
  int i = 2;
  ++i;
  if (i != 0) {
    ++i;
  } else {
    --i;
  }
}
