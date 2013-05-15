/**
 * @file oprof_start_main.cpp
 * main routine for GUI start
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <qapplication.h>
#include <stdlib.h>
#include <exception>
#include <iostream>

#include "oprof_start.h"

using namespace std;
int main(int argc, char* argv[])
{
	QApplication a(argc, argv);
	oprof_start* dlg;

	try {
		dlg = new oprof_start();
	} catch (exception e) {
		cerr << "Initialization error: " << e.what() << endl;
		exit(EXIT_FAILURE);
	}

	a.setMainWidget(dlg);

	dlg->show();

	return a.exec();
}
