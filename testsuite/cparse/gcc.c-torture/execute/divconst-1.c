typedef struct
{
  unsigned a, b, c, d;
} t1;

f (t1 *ps)
{
    ps->a = 10000;
    ps->b = ps->a / 3;
    ps->c = 10000;
    ps->d = ps->c / 3;
}

main ()
{
  t1 s;
  f (&s);
  if (s.a != 10000 || s.b != 3333 || s.c != 10000 || s.d != 3333)
    abort ();
  exit (0);
}

/* cp-out: warning: [^:]*: line 19, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 20, column 2: identifier "exit" not declared */
