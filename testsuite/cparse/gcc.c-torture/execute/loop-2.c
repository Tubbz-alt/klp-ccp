int a[2];

f (b)
{
  unsigned int i;
  for (i = 0; i < b; i++)
    a[i] = i - 2;
}

main ()
{
  a[0] = a[1] = 0;
  f (2);
  if (a[0] != -2 || a[1] != -1)
    abort ();
  exit (0);
}

/* cp-out: warning: [^:]*: line 15, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 16, column 2: identifier "exit" not declared */
