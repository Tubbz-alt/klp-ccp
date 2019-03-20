/* PR middle-end/79537 */
/* { dg-do compile } */
/* { dg-options "" } */
/* { dg-require-effective-target indirect_jumps } */
/* { dg-require-effective-target label_values } */

void
f (void)
{
L:

	*&&L;
}

void
f2 (void)
{
   void *p;
L:
   p = &&L;
   *p; /* { dg-warning "dereferencing 'void \\*' pointer" } */
}

/* cp-out: warning: [^:]*: line 12, columns 2-5: dereferencing pointer to incomplete type */
/* cp-out: warning: [^:]*: line 21, column 4: dereferencing pointer to incomplete type */
