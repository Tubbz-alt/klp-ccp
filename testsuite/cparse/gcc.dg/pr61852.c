/* PR c/61852 */
/* { dg-do compile } */
/* { dg-options "-Wimplicit-function-declaration" } */

int
f (int a)
{
  int b = a + a + a + ff (a); /* { dg-warning "23:implicit declaration of function" } */
  return b;
}

/* cp-out: warning: [^:]*: line 8, column 22: identifier "ff" not declared */
