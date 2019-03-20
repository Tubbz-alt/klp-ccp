unsigned calc_mp(unsigned mod)
{
      unsigned a,b,c;
      c=-1;
      a=c/mod;
      b=0-a*mod;
      if (b > mod) { a += 1; b-=mod; }
      return b;
}

int main(int argc, char *argv[])
{
      unsigned x = 1234;
      unsigned y = calc_mp(x);

      if ((sizeof (y) == 4 && y != 680)
	  || (sizeof (y) == 2 && y != 134))
	abort ();
      exit (0);
}

/* cp-out: warning: [^:]*: line 18, column 1: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 19, column 6: identifier "exit" not declared */
