typedef int new_int __attribute__ ((aligned(16)));
struct S { int x; };
 
int main()
{
  if (sizeof(struct S) != sizeof(int))
    abort ();
  return 0;
}

/* cp-out: warning: [^:]*: line 7, column 4: identifier "abort" not declared */
