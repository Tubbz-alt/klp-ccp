/* This is from PR c/25892.  See Wpointer-sign.c for more details.  */

/* { dg-options "-pedantic" } */

void foo(unsigned long* ulp);/* { dg-message "note: expected '\[^'\n\]*' but argument is of type '\[^'\n\]*'" "note: expected" } */

void bar(long* lp) {
  foo(lp); /* { dg-warning "differ in signedness" } */
}

/* cp-out: warning: [^:]*: line 8, column 6: pointers to different integer types in assignment */
