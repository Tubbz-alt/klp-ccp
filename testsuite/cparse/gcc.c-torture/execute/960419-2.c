#define SIZE 8

main()
{
  int a[SIZE] = {1};
  int i;

  for (i = 1; i < SIZE; i++)
    if (a[i] != 0)
      abort();

  exit (0);
}

/* cp-out: warning: [^:]*: line 10, column 6: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 12, column 2: identifier "exit" not declared */
