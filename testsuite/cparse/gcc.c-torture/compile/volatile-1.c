/* The problem here was that the statements that
   loaded from exception.reason where not being
   marked as having volatile behavior which
   caused load PRE on the tree level to go
   into an infinite loop. */

struct gdb_exception
{
  int reason;
};
int catch_exceptions_with_msg (int *gdberrmsg)
{
  volatile struct gdb_exception exception;
  exceptions_state_mc_init (&(exception));
  if (exception.reason != 0)
    foo ();
  return exception.reason;
}

/* cp-out: warning: [^:]*: line 14, column 2: identifier "exceptions_state_mc_init" not declared */
/* cp-out: warning: [^:]*: line 16, column 4: identifier "foo" not declared */
