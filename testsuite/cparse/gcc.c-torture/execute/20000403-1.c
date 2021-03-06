extern unsigned long aa[], bb[];

int seqgt (unsigned long a, unsigned short win, unsigned long b);

int seqgt2 (unsigned long a, unsigned short win, unsigned long b);

main()
{
  if (! seqgt (*aa, 0x1000, *bb) || ! seqgt2 (*aa, 0x1000, *bb))
    abort ();

  exit (0);
}

int
seqgt (unsigned long a, unsigned short win, unsigned long b)
{
  return (long) ((a + win) - b) > 0;
}

int
seqgt2 (unsigned long a, unsigned short win, unsigned long b)
{
  long l = ((a + win) - b);
  return l > 0;
}

unsigned long aa[] = { (1UL << (sizeof (long) * 8 - 1)) - 0xfff };
unsigned long bb[] = { (1UL << (sizeof (long) * 8 - 1)) - 0xfff };

/* cp-out: warning: [^:]*: line 10, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 12, column 2: identifier "exit" not declared */
