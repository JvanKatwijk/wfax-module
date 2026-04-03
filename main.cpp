#
/*
 *    Copyright (C) 2026
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the fax module for sdrconnect
 *
 *    fax module is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    fax module is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with fax module; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *	Main program
 */
#include	<QApplication>
#include	<QSettings>
#include	<QDir>
#include	<unistd.h>
#include	"radio.h"
#include	"constants.h"

QString fullPathfor (QString v) {
QString fileName;

	if (v == QString (""))
	   return QString ("/tmp/xxx");

	if (v. at (0) == QChar ('/'))           // full path specified
	   return v;

	fileName = QDir::homePath ();
	fileName. append ("/");
	fileName. append (v);
	fileName = QDir::toNativeSeparators (fileName);

	if (!fileName. endsWith (".ini"))
	   fileName. append (".ini");

	return fileName;
}

#define	DEFAULT_INI	".wfax.ini"
#define	PRESETS	".wfax-presets.xml"

int	main (int argc, char **argv) {
int32_t		opt;
/*
 *	The default values
 */
RadioInterface	*MyRadioInterface;
QString iniFile		= QDir::homePath ();
QString presetFile	= QDir::homePath ();

	QCoreApplication::setOrganizationName ("Lazy Chair Computing");
        QCoreApplication::setOrganizationDomain ("Lazy Chair Computing");
        QCoreApplication::setApplicationName ("wfax-module");
        QCoreApplication::setApplicationVersion (QString (CURRENT_VERSION) + " Git: " + GITHASH);

        iniFile. append ("/");
        iniFile. append (DEFAULT_INI);
        iniFile = QDir::toNativeSeparators (iniFile);

        presetFile. append ("/");
        presetFile. append (PRESETS);
        presetFile = QDir::toNativeSeparators (presetFile);

        while ((opt = getopt (argc, argv, "i:B:")) != -1) {
           switch (opt) {
              case 'i':
                 iniFile	= fullPathfor (QString (optarg));
                 break;

             default:
                 break;
           }
        }

	QApplication a (argc, argv);

	QSettings ISettings (iniFile, QSettings::IniFormat);

	QFile file (":res/globstyle.qss");
        if (file .open (QFile::ReadOnly | QFile::Text)) {
           a. setStyleSheet (file.readAll ());
           file.close ();
        }

        MyRadioInterface = new RadioInterface (presetFile);
	MyRadioInterface -> show ();
        a. exec ();
/*
 *	done:
 */
	fflush (stdout);
	fflush (stderr);
	qDebug ("It is done\n");
	ISettings. sync ();
	fprintf (stderr, "we gaan deleten\n");
	delete MyRadioInterface;
}
