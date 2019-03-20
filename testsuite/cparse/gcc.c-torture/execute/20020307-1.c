#define MASK(N) ((1UL << (N)) - 1)
#define BITS(N) ((1UL << ((N) - 1)) + 2)

#define FUNC(N) void f##N(long j) { if ((j & MASK(N)) >= BITS(N)) abort();}

FUNC(3)
FUNC(4)
FUNC(5)
FUNC(6)
FUNC(7)
FUNC(8)
FUNC(9)
FUNC(10)
FUNC(11)
FUNC(12)
FUNC(13)
FUNC(14)
FUNC(15)
FUNC(16)
FUNC(17)
FUNC(18)
FUNC(19)
FUNC(20)
FUNC(21)
FUNC(22)
FUNC(23)
FUNC(24)
FUNC(25)
FUNC(26)
FUNC(27)
FUNC(28)
FUNC(29)
FUNC(30)
FUNC(31)

int main ()
{
  f3(0);
  f4(0);
  f5(0);
  f6(0);
  f7(0);
  f8(0);
  f9(0);
  f10(0);
  f11(0);
  f12(0);
  f13(0);
  f14(0);
  f15(0);
  f16(0);
  f17(0);
  f18(0);
  f19(0);
  f20(0);
  f21(0);
  f22(0);
  f23(0);
  f24(0);
  f25(0);
  f26(0);
  f27(0);
  f28(0);
  f29(0);
  f30(0);
  f31(0);

  exit(0);
}

/* cp-out: warning: [^:]*: line 6, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 7, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 8, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 9, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 10, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 11, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 12, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 13, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 14, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 15, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 16, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 17, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 18, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 19, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 20, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 21, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 22, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 23, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 24, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 25, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 26, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 27, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 28, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 29, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 30, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 31, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 32, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 33, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 34, column 0: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 68, column 2: identifier "exit" not declared */
