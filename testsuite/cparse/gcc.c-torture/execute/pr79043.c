/* PR ipa/78791 */

int val;

int *ptr = &val;
float *ptr2 = &val;

static
__attribute__((always_inline, optimize ("-fno-strict-aliasing")))
typepun ()
{
  *ptr2=0;
}

main()
{
  *ptr=1;
  typepun ();
  if (*ptr)
    __builtin_abort ();
}

/* cp-out: warning: [^:]*: line 6, columns 14-18: incompatible pointer types in assignment */
