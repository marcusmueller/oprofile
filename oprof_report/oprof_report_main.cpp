/**
 * @file oprof_report_main.cpp
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#include <qapplication.h>

#include "oprof_report.h"

int main(int argc, char* argv[])
{
	QApplication a(argc, argv);

	oprof_report* dlg = new oprof_report();
	
	a.setMainWidget(dlg);

	dlg->show();

	return a.exec();
}
