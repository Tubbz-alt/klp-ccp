typedef struct rtx_def
{
  int f1 :1;
  int f2 :1;
} *rtx;

static rtx
f (orig)
     register rtx orig;
{
  if (orig->f1 || orig->f2)
    return orig;
  orig->f2 = 1;
  return orig;
}

void
f2 ()
{
  abort ();
}

main ()
{
  struct rtx_def foo;
  rtx bar;

  foo.f1 = 1;
  foo.f2 = 0;
  bar = f (&foo);
  if (bar != &foo || bar->f2 != 0)
    abort ();
  exit (0);
}

/* cp-out: warning: [^:]*: line 20, column 2: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 32, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 33, column 2: identifier "exit" not declared */
