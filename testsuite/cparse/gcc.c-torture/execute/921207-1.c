f()
{
  unsigned b = 0;

  if (b > ~0U)
    b = ~0U;

  return b;
}
main()
{
  if (f()!=0)
    abort();
  exit (0);
}

/* cp-out: warning: [^:]*: line 13, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 14, column 2: identifier "exit" not declared */
