sub3 (i)
     const int *i;
{
}

eq (a, b)
{
  static int i = 0;
  if (a != i)
    abort ();
  i++;
}

main ()
{
  int i;

  for (i = 0; i < 4; i++)
    {
      const int j = i;
      int k;
      sub3 (&j);
      k = j;
      eq (k, k);
    }
  exit (0);
}

/* cp-out: warning: [^:]*: line 10, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 26, column 2: identifier "exit" not declared */
