/* { dg-options "-fdiagnostics-show-caret -Wreturn-local-addr" } */

int *address_of_local (void)
{
  int some_local;
  return &some_local; /* { dg-warning "function returns address of local variable" } */
/* { dg-begin-multiline-output "" }
   return &some_local;
          ^~~~~~~~~~~
   { dg-end-multiline-output "" } */
}

void surplus_return_when_void_1 (void)
{
  return 500; /* { dg-warning "'return' with a value, in function returning void" } */
/* { dg-begin-multiline-output "" }
   return 500;
          ^~~
   { dg-end-multiline-output "" } */
/* { dg-begin-multiline-output "" }
 void surplus_return_when_void_1 (void)
      ^~~~~~~~~~~~~~~~~~~~~~~~~~
   { dg-end-multiline-output "" } */
}

void surplus_return_when_void_2 (int i, int j)
{
  return i * j; /* { dg-warning "'return' with a value, in function returning void" } */
/* { dg-begin-multiline-output "" }
   return i * j;
          ~~^~~
   { dg-end-multiline-output "" } */
/* { dg-begin-multiline-output "" }
 void surplus_return_when_void_2 (int i, int j)
      ^~~~~~~~~~~~~~~~~~~~~~~~~~
   { dg-end-multiline-output "" } */
}

int missing_return_value (void)
{
  return; /* { dg-warning "'return' with no value, in function returning non-void" } */
/* { dg-begin-multiline-output "" }
   return;
   ^~~~~~
   { dg-end-multiline-output "" } */
/* { dg-begin-multiline-output "" }
 int missing_return_value (void)
     ^~~~~~~~~~~~~~~~~~~~
   { dg-end-multiline-output "" } */
/* TODO: ideally we'd underline the return type i.e. "int", but that
   location isn't captured.  */
}

/* cp-out: warning: [^:]*: line 15, columns 2-13: return with value in function returning void */
/* cp-out: warning: [^:]*: line 28, columns 2-15: return with value in function returning void */
/* cp-out: warning: [^:]*: line 41, columns 2-9: return without value in function returning non-void */
