/* { dg-require-effective-target indirect_jumps } */
/* { dg-require-effective-target label_values } */


int
fn1 ()
{
  static char a[] = "foo";
  static void *b[] = { &&l1, &&l2 };
  goto *(b[1]);
 l1: goto *(a[0]);
 l2: return 0;
}

/* cp-out: warning: [^:]*: line 11, columns 11-17: dereferencing integer at computed goto */
