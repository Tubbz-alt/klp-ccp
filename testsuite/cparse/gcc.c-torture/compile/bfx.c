foo (x, c)
{
  return x << -c;
}

main ()
{
  printf ("%x\n", foo (0xf05, -4));
}

/* cp-out: warning: [^:]*: line 8, column 2: identifier "printf" not declared */
