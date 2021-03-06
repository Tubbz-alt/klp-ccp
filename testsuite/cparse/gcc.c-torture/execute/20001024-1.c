struct a;

extern int baz (struct a *__restrict x);

struct a {
  long v;
  long w;
};

struct b {
  struct a c;
  struct a d;
};

int bar (int x, const struct b *__restrict y, struct b *__restrict z)
{
  if (y->c.v || y->c.w != 250000 || y->d.v || y->d.w != 250000)
    abort();
}

void foo(void)
{
  struct b x;
  x.c.v = 0;
  x.c.w = 250000;
  x.d = x.c;
  bar(0, &x, ((void *)0));
}

int main()
{
  foo();
  exit(0);
}

/* cp-out: warning: [^:]*: line 18, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 33, column 2: identifier "exit" not declared */
