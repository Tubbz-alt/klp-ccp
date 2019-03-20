struct tiny
{
  char c;
  char d;
  char e;
};

f (int n, struct tiny x, struct tiny y, struct tiny z, long l)
{
  if (x.c != 10)
    abort();
  if (x.d != 20)
    abort();
  if (x.e != 30)
    abort();

  if (y.c != 11)
    abort();
  if (y.d != 21)
    abort();
  if (y.e != 31)
    abort();

  if (z.c != 12)
    abort();
  if (z.d != 22)
    abort();
  if (z.e != 32)
    abort();

  if (l != 123)
    abort ();
}

main ()
{
  struct tiny x[3];
  x[0].c = 10;
  x[1].c = 11;
  x[2].c = 12;
  x[0].d = 20;
  x[1].d = 21;
  x[2].d = 22;
  x[0].e = 30;
  x[1].e = 31;
  x[2].e = 32;
  f (3, x[0], x[1], x[2], (long) 123);
  exit(0);
}

/* cp-out: warning: [^:]*: line 11, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 13, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 15, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 18, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 20, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 22, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 25, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 27, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 29, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 32, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 48, column 2: identifier "exit" not declared */
