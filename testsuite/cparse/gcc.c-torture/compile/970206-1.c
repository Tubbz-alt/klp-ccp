struct Rect
{
  int iA;
  int iB;
  int iC;
  int iD;
};

void
f (int * const this, struct Rect arect)
{
  g (*this, arect);
}

/* cp-out: warning: [^:]*: line 12, column 2: identifier "g" not declared */
