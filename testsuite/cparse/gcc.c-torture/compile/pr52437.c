/* PR target/52437 */

int f, g, i, j;

void
fn1 ()
{
  for (;;)
    {
      fn2 ();
      j = 1;
      for (i = 0; i <= 3; i++)
	{
	  for (g = 1; g >= 0; g--)
	    f = 0, j &= 11;
	}
    }
}

/* cp-out: warning: [^:]*: line 10, column 6: identifier "fn2" not declared */
