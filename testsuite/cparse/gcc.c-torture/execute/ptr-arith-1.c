char *
f (char *s, unsigned int i)
{
  return &s[i + 3 - 1];
}

main ()
{
  char *str = "abcdefghijkl";
  char *x2 = f (str, 12);
  if (str + 14 != x2)
    abort ();
  exit (0);
}

/* cp-out: warning: [^:]*: line 12, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 13, column 2: identifier "exit" not declared */
