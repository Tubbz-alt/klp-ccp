main(int na, char* argv[])
{
	int wflg = 0, tflg = 0;
	int dflg = 0;
	exit(0);
	while(1)
	{
		switch(argv[1][0])
		{
			help:
				exit(0);
			case 'w':
			case 'W':
				wflg = 1;
				break;
			case 't':
			case 'T':
				tflg = 1;
				break;
			case 'd':
				dflg = 1;
				break;
		}
	}
}

/* cp-out: warning: [^:]*: line 5, column 1: identifier "exit" not declared */
/* cp-out: warning: [^:]*: line 11, column 4: identifier "exit" not declared */
