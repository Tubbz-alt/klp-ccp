int
foo1 (value)
     long long value;
{
  register const long long constant = 0xc000000080000000LL;

  if (value < constant)
    return 1;
  else
    return 2;
}

int
foo2 (value)
     unsigned long long value;
{
  register const unsigned long long constant = 0xc000000080000000LL;

  if (value < constant)
    return 1;
  else
    return 2;
}

main ()
{
  unsigned long long value = 0xc000000000000001LL;
  int x, y;

  x = foo1 (value);
  y = foo2 (value);
  if (x != y || x != 1)
    abort ();
  exit (0);
}

/* cp-out: warning: [^:]*: line 33, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 34, column 2: identifier "exit" not declared */
