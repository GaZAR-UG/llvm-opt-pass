//===----------------------------------------------------------------------===//
//
// Philipp Schubert
//
//    Copyright (c) 2021
//    GaZAR UG (haftungsbeschränkt)
//    Bielefeld, Germany
//    philipp@gazar.eu
//
//===----------------------------------------------------------------------===//

#include <cstdio>

void foo() { printf("This function is dangerous and should be replaced!\n"); }

void bar(int I) {
  printf("This function is safe! We found call site: '%d'.\n", I);
}

void baz() { foo(); }

int main(int Argc, char **Argv) {
  foo();
  for (int Idx = 0; Idx < Argc; ++Idx) {
    printf("%s\n", Argv[Idx]);
  }
  baz();
  return 0;
}
