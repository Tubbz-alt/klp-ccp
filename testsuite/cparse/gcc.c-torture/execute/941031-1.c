typedef long mpt;

int
f (mpt us, mpt vs)
{
  long aus;
  long avs;

  aus = us >= 0 ? us : -us;
  avs = vs >= 0 ? vs : -vs;

  if (aus < avs)
    {
      long t = aus;
      aus = avs;
      avs = aus;
    }

  return avs;
}

main ()
{
  if (f ((mpt) 3, (mpt) 17) != 17)
    abort ();
  exit (0);
}

/* cp-out: warning: [^:]*: line 25, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 26, column 2: identifier "exit" not declared */
