struct x {
	int a, b;
} z = { -4028, 4096 };

int foo(struct x *p, int y)
{
  if ((y & 0xff) != y || -p->b >= p->a)
    return 1;
  return 0;
}

main()
{
  if (foo (&z, 10))
    abort ();
  exit (0);
}

/* cp-out: warning: [^:]*: line 15, column 4: identifier "abort" not declared */
/* cp-out: warning: [^:]*: line 16, column 2: identifier "exit" not declared */
