/* { dg-skip-if "requires alloca" { ! alloca } { "-O0" } { "" } } */
f(){}
main(){int n=2;double x[n];f();exit(0);}

/* cp-out: warning: [^:]*: line 3, column 31: identifier "exit" not declared */
