void foo(int x)
{
  if (x > 3)
    {;}
  else
    bar();
  x = 9;
}

main()
{
  int j;

  foo(j);
  return j;
}

/* cp-out: warning: [^:]*: line 6, column 4: identifier "bar" not declared */
