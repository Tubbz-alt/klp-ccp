/* { dg-require-effective-target alloca } */

/* This program tests a data flow bug that would cause constant propagation
   to propagate constants through function calls.  */

foo (int *p)
{
  *p = 10;
}

main()
{
  int *ptr = alloca (sizeof (int));
  *ptr = 5;
  foo (ptr);
  if (*ptr == 5)
    abort ();
  exit (0);
}

/* cp-out: warning: [^:]*: line 13, column 13: identifier "alloca" not declared */
/* cp-out: warning: [^:]*: line 17, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 18, column 2: identifier "exit" not declared */
/* cp-out: warning: [^:]*: line 13, columns 13-34: assignment to pointer from integer which is not NULL */
