foo (a)
     double a;
{
  printf ("%d\n", (int) a);
}

main ()
{
  foo (1.6);
  foo (1.4);
  foo (-1.4);
  foo (-1.6);
}

/* cp-out: warning: [^:]*: line 4, column 2: identifier "printf" not declared */
