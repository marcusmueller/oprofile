/**
 * @file oprof_start_main.cpp
 * main routine for GUI start
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include <qapplication.h>

#include "oprof_start.h"

int main(int argc, char* argv[])
{
	QApplication a(argc, argv);

	oprof_start* dlg = new oprof_start();

	a.setMainWidget(dlg);

	dlg->show();

	return a.exec();
}
