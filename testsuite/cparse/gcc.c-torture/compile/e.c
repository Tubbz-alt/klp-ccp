foo (short a, int *p, short *s)
{
  int i;
  for (i = 10;  i >= 0; i--)
    {
      a = (short) bar ();
      p[i] = a;
      s[i] = a;
    }
}

/* cp-out: warning: [^:]*: line 6, column 18: identifier "bar" not declared */
