main ()
{
	int i;

	while (1) {
		for (i=0;i<10000000;i++)
			printf("");
		sleep(2);
	}

}
