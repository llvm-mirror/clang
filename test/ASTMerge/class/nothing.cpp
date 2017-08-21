// RUN: %clang_cc1 -emit-pch -o %t.1.ast %S/Inputs/class_member_order.cpp -std=c++11
  // RUN: %clang_cc1 -ast-merge %t.1.ast -ast-print %s -std=c++11 2>&1 | FileCheck %s


// CHECK: struct A {
// CHECK-NEXT:    int b = this->a + 2;
// CHECK-NEXT:    int c;
// CHECK-NEXT:    int a = 5;
// CHECK-NEXT:};
// CHECK-NEXT:struct B {
// CHECK-NEXT:    int f()     {
// CHECK-NEXT:        return this->a + 2;
// CHECK-NEXT:    }
// CHECK-NEXT:    int b;
// CHECK-NEXT:    int c;
// CHECK-NEXT:    int a;
// CHECK-NEXT:};
