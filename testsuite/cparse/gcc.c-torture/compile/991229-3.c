int m[8],b[8];

int main(){
	int i;

	for(;;){
		m[0] = rand();
		if(m[0] == 0){
			for(i=0;i<8;i++){
				m[i] = b[i];
			}
			break;
		}
	}
}

/* cp-out: warning: [^:]*: line 7, column 9: identifier "rand" not declared */
