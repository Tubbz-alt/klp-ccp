int f (char *p) { }

main ()
{
  char c;
  char c2;
  int i = 0;
  char *pc = &c;
  char *pc2 = &c2;
  int *pi = &i;

  *pc2 = 1;
  *pi = 1;
  *pc2 &= *pi;
  f (pc2);
  *pc2 = 1;
  *pc2 &= *pi;
  if (*pc2 != 1)
    abort ();
  exit (0);
}

/* cp-out: warning: [^:]*: line 19, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 20, column 2: identifier "exit" not declared */
