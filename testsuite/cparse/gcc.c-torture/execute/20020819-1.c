foo ()
{
  return 0;
}

main()
{
  int i, j, k, ccp_bad = 0;

  for (i = 0; i < 10; i++)
    {
      for (j = 0; j < 10; j++)
	if (foo ())
	  ccp_bad = 1;
    
      k = ccp_bad != 0;
      if (k)
	abort ();
    }

  exit (0);
}

/* cp-out: warning: [^:]*: line 18, column 1: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 21, column 2: identifier "exit" not declared */
