long long acc;

addhi (short a)
{
  acc += (long long) a << 32;
}

subhi (short a)
{
  acc -= (long long) a << 32;
}

main ()
{
  acc = 0xffff00000000ll;
  addhi (1);
  if (acc != 0x1000000000000ll)
    abort ();
  subhi (1);
  if (acc != 0xffff00000000ll)
    abort ();
  exit (0);
}

/* cp-out: warning: [^:]*: line 18, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 21, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 22, column 2: identifier "exit" not declared */
