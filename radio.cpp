#
/*
 *    Copyright (C) 2026
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the sdrconnect fax plugin
 *
 *    fax plugin is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation as version 2 of the License.
 *
 *    fax plugin is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with fax plugin; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include	<QFile>
#include	<QDir>
#include	<QFileDialog>
#include	<QDateTime>
#include	<QMessageBox>
#include	<sstream>
#include	<vector>
#include	<sstream>
#include	<complex>
#include	<chrono>
#include	<stdio.h>
#include	<stdlib.h>

#include	"radio.h"

#define  _USE_MATH_DEFINES
#include        <math.h>

//
//	input handling
#include	"message-handler.h"

//	fax specfics

#include	"fax-bandfilter.h"
#include	"fax-shifter.h"
#include	"fax-params.h"
//#include	"utilities.h"
#include        "up-filter.h"
//
#define	FAX_IF 0

#define	faxWidth	600

faxParams _faxParams [] = {
	{"Wefax576", 576, 300, 450, false, 120, 1200},
	{"Wefax288", 288, 675, 450, false, 120, 600},
	{"HamColor", 204, 200, 450, true,  360, 0},	// not visible
	{"Ham288b",  240, 675, 450, false, 240, 0},	// not visible
	{"Color240", 288, 200, 450, true,  240, 0},	// not visible
	{"FAX480",   204, 500, 450, false, 480, 0},	// not visible
	{nullptr,     -1,  -1,  -1, false,  -1, 0}	
};

static inline
float Minimum (float x, float y) {
	return x < y ? x : y;
}

static inline
bool	isWhite (int16_t x) {
	return x >= 128;
}

static inline
float	clamp (float X, float Min, float Max) {
	if (X > Max)
	   return Max;
	if (X < Min)
	   return Min;
	return X;
}

		RadioInterface:: RadioInterface (const QString &presetFile):
	                                   superFrame	(nullptr),
	                                   inputBuffer	(32 * 32768),
	                                   passbandFilter (11,
	                                                   -2000,
	                                                   +2000,
	                                                   INRATE),
	                                   theDecimator (OUTRATE / WORKING_RATE,
	                                                 2 * OUTRATE / WORKING_RATE,
	                                                 2000,
	                                                 OUTRATE),
	                                   localMixer (WORKING_RATE),
	                                   faxLowPass (FILTER_DEFAULT,
	                                                400, WORKING_RATE),
	                                   faxLineBuffer (600),
	                                   faxContainer (nullptr),
	                                   faxPresets (this, presetFile) {
	setupUi (this);
	show ();
	connect (this, &superFrame::frameClosed,
	         this, &RadioInterface::handle_quit);
	running. store (false);

	setWindowTitle ("fax control");
	overflow. store (200);	// could be just a constant
	carrier		= FAX_IF;
	resetFlag. store (false);
	cheatFlag. store (false);
	deviation	= 400;		// default
	shiftRequest. store (0);
	shiftDone	= false;
	samplesToShift	= 0;
	faxContainer. resize (200, 200);
        faxContainer. show ();
	theFax. theImage	= nullptr;
	theFax. startCounter	= 0;
	theFax. stopCounter	= 0;
	theFax. normal_entry	= false;
//
	correcting.store  (false);
	theFax. setCorrection	= false;
	saveSingle		= false;
	saveContinuous		= false;
	inputHandler		= nullptr;
	connect (connectButton, &QPushButton::clicked,
	         this, &RadioInterface::doConnect);
}

	RadioInterface::~RadioInterface () {	
	running.store (false);

	if (inputHandler != nullptr) {
           disconnect (inputHandler, &messageHandler::dataAvailable,
                       this, &RadioInterface::sampleHandler);
           delete  inputHandler;
        }
        hide    ();
        if (theFax. theImage != nullptr) {
	   faxContainer. hide ();
           delete theFax. theImage;
	}
        theFax. theImage        = nullptr;
        faxContainer. hide      ();
        faxPresets. hide        ();
        faxPresets. saveList    ();
}

void	RadioInterface::doConnect	() {
	running. store (false);
	inputHandler		= new messageHandler (&inputBuffer,
	                                                 7880000);
	connect (inputHandler, &messageHandler::connection_failed,
	         this, &RadioInterface::handle_connection_failed);
	connect (inputHandler, &messageHandler::connection_succeeded,
	         this, &RadioInterface::handle_connection_succeeded);
	hostNameLabel   -> setInputMask ("000.000.000.000");
        hostNameLabel   -> setText ("127.0.0.1"); 

	inputHandler	-> tryConnect (hostNameLabel -> text (),
	                               portSelector -> value ());
	waitingTime	= 0;

}

void	RadioInterface::handle_connection_failed () {
	QMessageBox::warning (this, tr ("Warning"),
                                 tr ("connection failed"));
	disconnect (inputHandler, &messageHandler::connection_failed,
	            this, &RadioInterface::handle_connection_failed);
	disconnect (inputHandler, &messageHandler::connection_succeeded,
	            this, &RadioInterface::handle_connection_succeeded);
	if (inputHandler != nullptr) {
	   delete inputHandler;
	   inputHandler = nullptr;
	}
}

void	RadioInterface::handle_connection_succeeded () {
	disconnect (connectButton, &QPushButton::clicked,
	            this, &RadioInterface::doConnect);
	disconnect (inputHandler, &messageHandler::connection_succeeded,
	            this, &RadioInterface::handle_connection_succeeded);
	connectLabel	-> setText ("connected");
	connect (inputHandler, &messageHandler::dataAvailable,
	         this, &RadioInterface::sampleHandler);
	connect (inputHandler, &messageHandler::connection_failed,
	         this, &RadioInterface::onDisconnect);
	connect (cheatButton, &QPushButton::clicked,
	         this, &RadioInterface::handle_cheatButton);
	connect (continueButton, &QPushButton::clicked,
	         this, &RadioInterface::handle_continueButton);
	connect (saveContinuousButton, &QPushButton::clicked,	
	         this, &RadioInterface::handle_saveContinuous);
	connect (saveSingleButton, &QPushButton::clicked,
	         this, &RadioInterface::handle_saveSingle);
	connect (resetButton, &QPushButton::clicked,
	         this, &RadioInterface::handle_resetButton);
	connect (correctButton, &QPushButton::clicked,
                 this, &RadioInterface::fax_setCorrection);
	connect (presetButton, &QPushButton::clicked,
	         this, &RadioInterface::handle_presetButton);
	connect (saveFrequency, &QPushButton::clicked,
	         this, &RadioInterface::handle_saveFrequency);
	connect (inputHandler, &messageHandler::set_disconnect,
	         this, &RadioInterface::set_disconnect);

	inputHandler	-> setVFOFrequency (7880000);
//	set the defaults

//	faxLowPass depends on the selection, it might change
	fax_setDeviation	(deviationSelector -> currentText ());
	fax_setup		(iocSelector	-> currentText ());
	fax_setMode		(modeSelector	-> currentText ());
	fax_setPhase		(phaseSelector	-> currentText ());
	fax_setColor		(faxColorSelector	-> currentText ());
//	and off we go
}


std::complex<float> buffer [WORKING_RATE / 10];
void	RadioInterface::sampleHandler (int amount) {
	(void)amount;
//	handle flags, if set
//	handle hier changes in settings!!!
	if (resetFlag. load ()) {
	   fax_setup			(iocSelector -> currentText ());
	   carrier			= FAX_IF;
	   correcting. store (false);
	   theFax. setCorrection	= false;
	   saveSingle			= false;
	   saveSingleButton	-> setText ("saveSingle");
	   saveContinuous		= false;
	   saveContinuousButton	 -> setText ("saveContinuous");
	   theFax. linesRecognized	= 0;
	   lineNumber -> display (theFax. linesRecognized);
	   rawData. resize		(1024 * 1024);

	   resetFlag. store (false);
	}
	else
	if (cheatFlag. load ()) {
	   if (theFax. faxState == SYNCED) {
	      theFax. faxState = FAX_DONE;
	      cheatFlag. store (false);
	   }
	   else {
	      theFax. bufferP 		= 0;
	      theFax. checkP 		= 0;
	      theFax. linesRecognized 	= 0;
	      theFax. alarmCount	= 0;
	      theFax. currentSampleIndex	= 0;
	      theFax. currentColumn	= 0;
	      theFax. lastRow 		= 0;
	      theFax. stoppers 		= 0;
	      theFax. setCorrection	= false;
	      theFax. faxState 		= SYNCED;
	      cheatFlag. store (false);
	      clearScreen		();
	      showState	-> setText ("ON SYNC");
	   }
	}

//	All set, off we go

	while (inputBuffer. GetRingBufferReadAvailable () >= INRATE / 10) {
	   static int teller = 0;
//	inputBuffer: samplerate inputRate
	   std::complex<float> sample;
	   inputBuffer. getDataFromBuffer (&sample, 1);
	   if (!theDecimator. process (sample, sample)) 
	      continue;

	   buffer [teller] = sample;
	   teller++;
	   if (teller < WORKING_RATE / 10) 
	      continue;
	   
	   teller = 0;
//	all (serious) asycnhronous changes are handled now,
//	so we process the incoming buffer
	   for (int i = 0; i < WORKING_RATE / 10; i++) {
	      std::complex<float> z =  buffer [i];
	      z			= localMixer. do_shift (z, carrier);
	      z			= faxLowPass. Pass (z);
	      int sampleValue	= demodulate (z);

	      if (faxMode. phaseInvers)
	         sampleValue = 256 - sampleValue;
	      if (faxMode. faxColor == FAX_BLACKWHITE)
	         sampleValue = isWhite (sampleValue) ? 255 : 0;
	      processSample (sampleValue);
	   }
	}
}
//
//	first, set up the values for the IOC selected. The
//	"run time" parameters are filled in on "APTSTART"
void	RadioInterface::fax_setup	(const QString &s) {
faxParams	*myfaxParameters	= getFaxParams (s);

	faxMode. name		= s;
	faxMode. fax_IOC	= myfaxParameters -> IOC;
	faxMode. nrColumns	= M_PI * faxMode. fax_IOC;
	faxMode. nrLines	= myfaxParameters -> nrLines;
	faxMode. aptStartFreq	= myfaxParameters -> aptStart;
	faxMode. aptStopFreq	= myfaxParameters -> aptStop;
	faxMode. lpm		= myfaxParameters -> lpm;
	faxMode. phaseInvers	= false;
	faxMode. samplesperLine	= WORKING_RATE * 60 / faxMode. lpm;
	faxMode. faxColor	= myfaxParameters	->  color ?
	                                    FAX_COLOR: FAX_BLACKWHITE;
	faxMode. demodMode	=  FAX_FM;

	theFax. faxState	= APTSTART;
	theFax. lastRow	= 0;	// will change	
	if (theFax. theImage != nullptr) 
	   delete theFax. theImage;
	theFax. theImage	= new faxImage (faxMode. nrColumns, 40);
	rawData. resize (1024 * 1024);
	faxLineBuffer. resize (5 * faxMode. samplesperLine);
	checkBuffer. resize (faxMode. samplesperLine  + 10);

	convBuffer. resize (faxMode. samplesperLine + 1);
	mapTable_int. resize (faxMode. nrColumns);
	mapTable_float. resize (faxMode. nrColumns);
	convIndex	= 0;

	float   denominator     = float (faxMode. nrColumns);
        float inVal             = float (faxMode. samplesperLine);
        for (int i = 0; i < faxMode. nrColumns; i ++) {
           mapTable_int [i]     = int (floor (i * (inVal / denominator)));
           mapTable_float [i] =
                             i * (inVal / denominator) - mapTable_int [i];
        }
        convIndex       = 0;
}

static inline
bool	realWhite (int16_t x) {
	return x > 229;
}

static inline
bool	realBlack(int16_t x) {
	return x < 25;
}
//

std::complex<float> goertzel (int *v, int k, int N) {
float w = 2 * M_PI * k / N;
float cw        = cos (w);
float c		= 2 * cw;
float sw        = sin (w);
float z1        = 0;
float z2        = 0;

        for (int i = 0; i < N; i ++) {
           float z0 = ((float)(v [i] - 128.0)) / 256.0 + c * z1 - z2;
           z2   = z1;
           z1   = z0;
        }
        return std::complex<float> (cw * z1 - z2, sw * z1);
}
//
//
//	WORKING_RATE is usually 12000,
//	Both 576 and 288 handle 2 lines a second
bool	RadioInterface::toneTest	(int *buffer, int amount, int freq) {
int freqPointer		= freq * amount / WORKING_RATE;
int	maxIndex	= -1;
float	max		= 0;
float avg		= 0;
int	base		= freqPointer - 55;
int	top		= freqPointer + 55;

	for (int i = base; i < top; i ++) {
	   float res = abs (goertzel (buffer, i, amount));
	   avg += res;
	   if (res > max) {
	      max = res;
	      maxIndex = i;
	   }
	}
	avg /= (top - base);

	return ((max > 5 * avg) && (maxIndex == freqPointer));
}

void	RadioInterface::processSample (int sampleValue) {
int	baseP;
	switch (theFax. faxState) {
	   case	APTSTART:		// initialize the "theFax" record
	      showState -> setText	("APTSTART");
	      theFax. bufferP		= 0;
	      theFax. checkP		= 0;
	      theFax. linesRecognized	= 0;
	      theFax. currentSampleIndex	= 0;
	      theFax. alarmCount	= 0;
	      theFax. lastRow		= 0;
	      theFax. stoppers		= 0;
	      theFax. setCorrection	= false;	
	      theFax. normal_entry	= false;
	      clearScreen	();

	      faxLineBuffer [theFax. bufferP] = sampleValue;
	      theFax. bufferP ++;
	      theFax. faxState		= WAITING_FOR_START;
	      theFax. startCounter	= 0;
	      theFax. stopCounter	= 0;
	      break;

	   case WAITING_FOR_START:
	      faxLineBuffer [theFax. bufferP] = sampleValue;
	      theFax. bufferP ++;
	      if (theFax. bufferP < faxMode. samplesperLine)
	         break;

	      theFax. startCounter = 0;
	      theFax. stopCounter = 0;
	      if (toneTest (faxLineBuffer. data (),
	                                faxMode. samplesperLine, 300)) {
	         theFax. faxState = MAYBE_START;
	         theFax. startCounter = 1;
	      }

	      for (int i = faxMode. samplesperLine / 2;
	                   i < faxMode. samplesperLine; i ++)
	         faxLineBuffer [i - faxMode. samplesperLine / 2] =
	                                            faxLineBuffer [i];
	      theFax. bufferP -= faxMode. samplesperLine / 2;
	      break;
//
//	The start tone last for app 5 seconds, which amounts
//	to 10 lines, maybe we missed a small part, so we settle for
//	7 lines with a detected start tone
	   case MAYBE_START:
	      faxLineBuffer [theFax. bufferP] = sampleValue;
	      theFax. bufferP ++;
	      if (theFax. bufferP <  faxMode. samplesperLine) 
	         break;

	      if (!toneTest (faxLineBuffer. data (),
	                                faxMode. samplesperLine, 300)) 
	         theFax. faxState = WAITING_FOR_START;
	      else {
	         theFax. startCounter ++;
	         fprintf (stderr, "Tone detected (counter %d)\n",
	                                              theFax. startCounter);
	         if (theFax. startCounter >= 7) {
	           theFax. faxState = START_RECOGNIZED;
	           fprintf (stderr, "Start is recognized\n");
	           theFax. startCounter = 0;
	         }
	      }

	      for (int i = faxMode. samplesperLine / 2;
	                   i < faxMode. samplesperLine; i ++)
	         faxLineBuffer [i - faxMode. samplesperLine / 2] =
	                                           faxLineBuffer [i];
	      theFax. bufferP -= faxMode. samplesperLine / 2;
	      break;

	   case START_RECOGNIZED:
	      toRead --;
	      if (toRead <= 0) {
	         theFax. faxState	= WAITING_FOR_PHASE;
	         showState -> setText ("PHASING");
	         theFax. alarmCount	= 0;
	         theFax. bufferP 	= 0;
	      }
	      break;

	   case WAITING_FOR_PHASE:
	      faxLineBuffer [theFax. bufferP] = sampleValue;
	      theFax. bufferP ++;
	      if (theFax. bufferP ==  (int)(faxLineBuffer. size () - 2)) {
	         theFax. faxState = READ_PHASE;
	      }
	      break;

	   case READ_PHASE:
	      faxLineBuffer [theFax. bufferP] = sampleValue;
	      baseP = checkPhase (faxLineBuffer, 0, 0.90);
	      if (baseP >= 0) {
	         for (int i = baseP; i < faxMode. samplesperLine; i ++)
	            faxLineBuffer [i - baseP] = faxLineBuffer [i];
	         theFax. bufferP        = faxMode. samplesperLine - baseP;
	         theFax. currentSampleIndex = 0;
	         theFax. checkP		= 0;
	         theFax. faxState	= SYNCED;
	         showState -> setText ("ON SYNC");
	         theFax. stoppers	= 0;
	         theFax. linesRecognized = 0;
	         theFax. normal_entry	= true;
	      }
	      else {
	         for (uint32_t i = faxMode. samplesperLine;
	                         i < faxLineBuffer. size (); i ++)
	            faxLineBuffer [i - faxMode. samplesperLine] =
	                                             faxLineBuffer [i];
	         theFax. bufferP = faxLineBuffer. size () -
	                                    faxMode. samplesperLine - 1;
	         theFax. alarmCount ++;
	         lineNumber -> display (theFax. alarmCount);
	         if (theFax. alarmCount >= 15)
	            theFax. faxState = APTSTART;
	         else
	            theFax. faxState = WAITING_FOR_PHASE;
	      }
	      break;

	   case SYNCED:
	      faxLineBuffer [theFax. bufferP] = sampleValue;
	      theFax. bufferP = (theFax. bufferP + 1) % faxMode. samplesperLine;
	      
	      if (theFax. bufferP > 0) 
	         break;
	  
	      if ((int32_t) (rawData. size ()) <=
	                             theFax. currentSampleIndex +
	                                              faxMode. samplesperLine) 
	         rawData. resize (rawData. size () + 1024 * 1024);

	      for (int i = 0; i < faxMode. samplesperLine; i ++)
	         rawData [theFax. currentSampleIndex ++] = faxLineBuffer [i];
	      processBuffer (faxLineBuffer,
	                     theFax. linesRecognized,
	                     faxMode. samplesperLine);
	      theFax. bufferP = 0;
	      lineNumber -> display (theFax. linesRecognized);
	      theFax. linesRecognized ++;
//
//	searching for a stop criterium depends on whether
//	the processing is "normal" or follolwing a "cheat"
//	In the former case we only start tone detecting if the
//	number of recognized lines nears the max, otherwise
//	it is - inevitable - for each line
	      if ((theFax. normal_entry &&
	         (theFax. linesRecognized > faxMode. nrLines - 20)) ||
	         !theFax. normal_entry) {
	         if (toneTest (faxLineBuffer. data (),
	                                faxMode. samplesperLine, 450)) {
	            theFax. stopCounter ++;
	            fprintf (stderr, "stopCounter %d\n",
	                                            theFax. stopCounter);
	            if (theFax. stopCounter > 7) {
	               theFax. stopCounter = 0;
	               theFax. faxState = FAX_DONE;
	               break;
	            }
	         }
	      }
	      if (theFax. linesRecognized >
	                        faxMode. nrLines + overflow. load ())
	         theFax. faxState = FAX_DONE;
	      break;

	   case FAX_DONE:
	      showState -> setText (QString ("FAX_DONE"));
	      if (theFax. setCorrection) {
	         doCorrection (sampleCorrection -> value ());
	         theFax. setCorrection = false;
	      }
	      else
	      if (saveSingle) {
	         saveImage_auto ();
	         saveSingle = false;
	         saveSingleButton -> setText ("saveSingle");
	      }
	      else
	      if (saveContinuous) {
	         saveImage_auto ();
	         theFax.faxState	= WAIT_A_WHILE;
	         waitingTime	= 2 * WORKING_RATE;
	      }
	      break;

	   case WAIT_A_WHILE:
	      waitingTime --;
	      if (waitingTime <= 0)
	         theFax. faxState	= APTSTART;
	      break;

	   default:		// cannot happen
	      theFax. faxState = APTSTART;
	}
}
//
static inline 
float	square (float f) {
	return f * f;
}

//
int	RadioInterface::checkPhase	(std::vector<int> &buffer,
	                                   int index, float threshold) {
int	baseP	= findPhaseLine (buffer, 0, faxMode. samplesperLine, threshold);
	(void)index;
	if (baseP < 0)
	   return -1;
	if (!checkPhaseLine (buffer,
	                     baseP + 1 * faxMode. samplesperLine, threshold))
	   return -1;
	if (!checkPhaseLine (buffer,
	                     baseP + 2 * faxMode. samplesperLine, threshold))
	   return -1;
	if (!checkPhaseLine (buffer,
	                     baseP + 3 * faxMode. samplesperLine, threshold))
	   return -1;
//	if (!checkPhaseLine (buffer,
//		             baseP + 4 * faxMode. samplesperLine, threshold))
//	   return -1;
	return baseP;
}
//
//	A phaseLine starts with 2.5 percent white, then 95 percent black
//	and ending with 2.5 percent white
bool	RadioInterface::checkPhaseLine (std::vector<int> &buffer,
	                                  int index, float threshold) {
int	L1	= 2.5 * faxMode. samplesperLine / 100;
int	nrWhites	= 0;
int	nrBlacks	= 0;

	for (int i = 0; i < L1; i ++)
	   if (realWhite (buffer [index + i]) &&
	       realWhite (buffer [index + faxMode. samplesperLine - i - 1]))
	      nrWhites ++;

	if (nrWhites < threshold *  L1)
	   return false;

	for (int i = 0; i < faxMode. samplesperLine; i ++)
	   if (realBlack (buffer [index + i]))
	      nrBlacks ++;

	return nrBlacks > threshold * (0.95 * faxMode. samplesperLine);
}

int	RadioInterface::findPhaseLine	(std::vector<int> &buffer,
	                                   int ind, int end, float threshold) {
	for (int i = ind; i < end; i ++)
	   if (checkPhaseLine (buffer, i, threshold))
	      return i;
	return -1;
}

int	RadioInterface::shiftBuffer	(std::vector<int> &v,
	                                          int start, int end) {
	for (int i = start; i < end; i ++)
	   v [i - start] = v [i];
	return end - start;
}
//
//	A sample may contribute to more than one pixel. E.g., for a samplerate
//	of 12000, and an IOC of 576, there are 3.3 samples contributing.
//	We therefore look for the partial contribution of the first
//	and the last sample for a pixel
//	

void	RadioInterface::processBuffer	(std::vector<int> &buffer,
	                                         int currentRow,
	                                         int samplesLine) {
	theFax. currentColumn = 0;
	for (int i = 0; i < samplesLine; i ++) {
	   convBuffer [convIndex ++] = (float) (buffer [i] - 127);
	   if (convIndex >= samplesLine) {
	      for (int j = 0; j < faxMode. nrColumns; j ++) {
	         int16_t inpBase     = mapTable_int [j];
	         float   inpRatio    = mapTable_float [j];
	         int16_t pixelValue	=
	                      convBuffer [inpBase + 1] * inpRatio +
                              convBuffer [inpBase] * (1 - inpRatio) + 127;
	         addPixeltoImage ((int)clamp (pixelValue, 0, 255),
	                               theFax. currentColumn, currentRow);
	         theFax. currentColumn ++;
	      }
	   }
	}
	      convBuffer [0] = convBuffer [samplesLine];
	      convIndex = 1;
}

//void	RadioInterface::processBuffer	(std::vector<int> &buffer,
//	                                          int currentRow,
//	                                          int samplesLine) {
//
//int	pixelSamples	= 0;
//float	pixelValue	= 0;
//
//float samplesPerColumn = (float)faxMode. nrColumns / faxMode. samplesperLine;
//
//	theFax. currentColumn	= 0;
//	for (int samplenum = 0; samplenum < samplesLine; samplenum ++) {
//	   int x = buffer [samplenum];
//	   int	columnforSample	=
//	        floor ((float)samplenum / faxMode. samplesperLine * faxMode. nrColumns);
//	   if (columnforSample == theFax. currentColumn) { // still dealing with the same pixel
//	      if (samplesPerColumn * faxMode. nrColumns >
//	                           theFax. currentColumn) {// partial contribution
//	         float part_0, part_1;
//// part 0 is for this pixel
//	         part_0 = samplesPerColumn *
//	                         faxMode. nrColumns - theFax. currentColumn;
//// and part_1 is for the next one
//	         part_1		= (1 - (samplesPerColumn * faxMode. nrColumns - theFax. currentColumn));
//	         pixelValue	+= part_0 * x;
//	         pixelSamples 	+= part_0;
//	         addPixeltoImage (pixelValue / pixelSamples,
//	                               theFax. currentColumn, currentRow);
//	         theFax. currentColumn ++;
//	         pixelValue	= part_1 * x;
//	         pixelSamples	= part_1;
//	         continue;
//	      }
//
////	      pixelValue	+= x;
//	      pixelSamples	++;
//	      continue;
//	   }
////
////	we expect here currentCol > currentColumn
//	   if (pixelSamples > 0) 	// simple "assertion"
//	      addPixeltoImage (pixelValue / pixelSamples,
//	                         theFax. currentColumn, currentRow);
//
//	   theFax. currentColumn = columnforSample;
////	   pixelValue		= x;
//	   pixelSamples		= 1;
//	}
//}

void	RadioInterface::processLine (std::vector<float> &result,
	                               std::vector<float> &samples,
	                               int columns, int samplesperLine) {
float pixelSamples	= 0;
float pixelValue	= 0;

	for (int sampleNum = 0; sampleNum < samplesperLine; sampleNum ++) {
	   float x = samples [sampleNum];
	   float temp = (float)columns / samplesperLine;
	   int columnforSample = 
	      floor ((float)sampleNum / samplesperLine * columns);

	   if (columnforSample == theFax. currentColumn) { // the same pixel
	      if (temp * faxMode. nrColumns > theFax. currentColumn) { //partial contribution
	         float part_0, part_1;
// part 0 is for this pixel
	         part_0 = temp * columns - theFax. currentColumn;
// and part_1 is for the next one
	         part_1         = (1 - (temp * columns - theFax. currentColumn));
	         pixelValue     += part_0 * x;
	         pixelSamples   += part_0;
	         result [theFax. currentColumn] = pixelValue / pixelSamples;
	         theFax. currentColumn ++;
	         pixelValue     = part_1 * x;
	         pixelSamples   = part_1;
	         continue;
	      }
	   }
//
//      we expect here currentCol > currentColumn
	   if (pixelSamples > 0)        // simple "assertion"
	          result [theFax. currentColumn] = pixelValue / pixelSamples;
	   theFax. currentColumn        = columnforSample;
	   pixelValue           = x;
	   pixelSamples         = 1;
	}
}
//
//
void	RadioInterface::addPixeltoImage (float val,
	                                     int32_t col, int32_t row) {
int32_t realRow = faxMode. faxColor == FAX_COLOR ? row / 3 : row;
	theFax. theImage -> setPixel (col, realRow, val, 
	                                faxMode. faxColor == FAX_COLOR ? row % 3 : 3);
	if (theFax. lastRow != row) {
	   lineNumber -> display (row);
           fax_displayImage (theFax. theImage -> getImage (), 0, row);
	   theFax. lastRow	= row;
	}
}

void	RadioInterface::fax_displayImage (const QImage &image) {
QLabel	*imageLabel = new QLabel;
	imageLabel	-> setPixmap (QPixmap::fromImage (image));
	faxContainer. setWidget (imageLabel);
	imageLabel	-> show ();
}

void	RadioInterface::fax_displayImage (const QImage &image, int x, int y) {
QLabel	*imageLabel = new QLabel;

	imageLabel	-> setPixmap (QPixmap::fromImage (image));
	faxContainer.  setWidget (imageLabel);
	imageLabel	-> show ();
	faxContainer. ensureVisible (x, y);
}
//
faxParams *RadioInterface::getFaxParams	(const QString &s) {
int16_t	i;

	for (i = 0; _faxParams [i].Name != NULL; i ++)
	   if (s == _faxParams [i]. Name)
	      return &_faxParams [i];
	return NULL;
}

///*
// *	we add the demodulated value (x) to the
// *	current pixel, so first we find out
// *	the position of the current pixel
// *
// *	Number of samples per line =
// *		theRate  * 60.0 / lpm
// *	samplenumber in currentline =
// *		fmod (currentSampleIndex, theRate * 60.0 / lpm)
// *	position of sample in current column =
// *	       samplenumber in current line / number of samples per Line * x
// *
// *


/////////////////////////////////////////////////////////////////////////
//	Interface functions
//
//      coming from the GUI
void	RadioInterface::fax_setIOC	(const QString &s) {
	this	-> selected_IOC = s;
	this	-> resetFlag = true;
}

void	RadioInterface::fax_setMode	(const QString &s) {
	faxMode. demodMode = s == "AM" ? FAX_AM : FAX_FM;
}

void	RadioInterface::fax_setPhase	(const QString &s) {
	faxMode. phaseInvers	= s == "inverse";
}

void	RadioInterface::fax_setColor	(const QString &s) {
	if (s == "BW")
	   faxMode. faxColor = FAX_BLACKWHITE;
	else
	if (s == "COLOR")
	   faxMode. faxColor = FAX_COLOR;
	else
	   faxMode. faxColor = FAX_GRAY;
}

void	RadioInterface::fax_setDeviation	(const QString &s) {
	if (s == "1900-400") {
	   carrier	= 0;
	   deviation	= 400;
	}
	else {
	   carrier	= 0;
	   deviation	= 450;
	}
}

void	RadioInterface::handle_resetButton	() {
	resetFlag. store (true);
	samplesToShift = 0;
	shiftRequest.store(0);
	shiftDone = true;
	
}
void	RadioInterface::handle_cheatButton() {
	cheatFlag. store (true);
	theFax. normal_entry = false;
}

void	RadioInterface::handle_continueButton	() {
	if (theFax. faxState == FAX_DONE)
	   theFax. faxState = APTSTART;
}

void	RadioInterface::set_overflow	(int n) {
	overflow. store (n);
}
//


static inline
bool    isValid (QChar c) {     
        return c. isLetter () || c. isDigit () || (c == '-') || (c == '/');
}

QString getFileName () {
std::time_t result = std::time (nullptr);
QString theTime;
char* home = getenv ("HOMEPATH");
char * tt = std::asctime (std::localtime (&result));
	if (tt == 0)
	   theTime = "no-time";
	else {
	   for (int i = 0; tt [i] != 0; i ++)
	      if (isValid (tt [i]))
	        theTime. push_back (tt [i]);
	      else
	        theTime. push_back ('-');
	}
	return QString (home) + "\\wFax-" + theTime + ".bmp";
}


void	RadioInterface::clearScreen () {
	theFax. theImage	-> clear ();
	fax_displayImage	(theFax. theImage -> getImage ());
}

void	RadioInterface::regenerate() {
	if (theFax. faxState != FAX_DONE)
	   return;
	theFax. setCorrection = true;
}

void	RadioInterface::doCorrection (int offset) {
int sampleTeller	= 0;
std::vector<int> lineBuffer;
int	lineno		= 0;
int	lineSamples	= faxMode. samplesperLine + offset;
int	maxi		= lineSamples > faxMode. samplesperLine ?
	                        lineSamples : faxMode. samplesperLine;

	if (theFax. faxState != FAX_DONE)
	   return;
	clearScreen ();

	lineBuffer. resize (maxi);
	correcting. store (true);
	while (sampleTeller < theFax. currentSampleIndex + maxi) {
	   if ((lineno % 10) != 0) {
	      for (int i = 0; i < faxMode. samplesperLine; i ++) 
	         lineBuffer [i] = rawData [sampleTeller ++];
	      processBuffer (lineBuffer, lineno, faxMode. samplesperLine);
	   }
	   else
	      for (int i = 0; i < lineSamples; i ++) {
	         lineBuffer [i] = rawData [sampleTeller ++];
	      processBuffer (lineBuffer, lineno, lineSamples);
	   }
	   lineno ++;
	}
	correcting. store (false);
}

void	RadioInterface::saveImage_single	() {
QFile outFile;
QString saveName = QFileDialog::getSaveFileName (nullptr,
                                                 tr ("save file as .."),
                                                 QDir::homePath (),
                                                 tr ("Images (*.png)"));

	outFile. setFileName (saveName);
	if (!outFile. open (QIODevice::WriteOnly)) {
	   qDebug () << "Cannot open" << saveName;
	   fprintf (stderr, "Could not open %s for writing\n",
	                                saveName. toLatin1 (). data ());
	   return;
	}

	QImage localImage	= theFax. theImage -> getImage ();
//	fprintf (stderr, "height = %d\n", localImage. height ());
	localImage. save (&outFile, "PNG");
	fprintf (stderr, "file %s written\n",
	                                  saveName. toLatin1 (). data ());
	outFile. close ();
}

void	RadioInterface::saveImage_auto	() {
QFile outFile;
QString saveName        = getSaveName ();

	outFile. setFileName (saveName);
	if (!outFile. open (QIODevice::WriteOnly)) {
	   qDebug () << "Cannot open" << saveName;
	   fprintf (stderr, "Could not open %s for writing\n",
                                        saveName. toLatin1 (). data ());
           return;
        }

        QImage localImage	=theFax. theImage -> getImage ();
        fprintf (stderr, "height = %d\n", localImage. height ());
        localImage. save (&outFile, "PNG");
        fprintf (stderr, "file %s written\n",
                                          saveName. toLatin1 (). data ());
        outFile. close ();

}

QString	RadioInterface::getSaveName	() {
QString theTime = QDateTime::currentDateTime (). toString ();
QString saveDir	= QDir::homePath ();

	if ((saveDir != "") && (!saveDir. endsWith ('/')))
	   saveDir = saveDir + '/';

	QString fileName = "weatherfax-" + theTime;
	for (int i = 0; i < fileName. length (); i ++)
	   if (!isValid (fileName. at (i)))
	      fileName. replace (i,1, '-');
	fileName = saveDir + fileName + ".png";
	fileName = QDir::toNativeSeparators (fileName);
	return fileName;
}

int	RadioInterface::demodulate	(std::complex<float> z) {
float	res;
static std::complex<float> prevSample = std::complex<float> (0, 0);

        if (faxMode. demodMode == FAX_AM)
           return abs (z) * 255.0;
	else
           z	= z / (float)abs (z);

        res	= arg (conj (prevSample) * z) / (2 * M_PI) * WORKING_RATE;
        res	= clamp (res, - this -> deviation,
	                      + this -> deviation);
        prevSample      = z;
        return (int16_t)(res / deviation * 128 + 127);
}

void	RadioInterface::handle_pictureShift (int col) {
	shiftRequest. store (col);
}

void    RadioInterface::fax_setCorrection   () {
	theFax. setCorrection = !theFax. setCorrection;
}

void	RadioInterface::onDisconnect	() {
	if (inputHandler != nullptr) {
	   disconnect (inputHandler, &messageHandler::dataAvailable,
                       this, &RadioInterface::sampleHandler);
           delete  inputHandler;
        }
	hide	();
	if (theFax. theImage != nullptr)
	   delete theFax. theImage;
	theFax. theImage	= nullptr;
	faxContainer. hide	();
	faxPresets. hide	();
	faxPresets. saveList	();
	close ();
}

void	RadioInterface::handle_quit	() {
}

void	RadioInterface::handle_preset	(const QString &freqText) {
bool b;
float freq = freqText. toFloat (&b);
	if (!b)
	   return;

	inputHandler	-> setVFOFrequency ((int)(freq * 1000));
}

void    RadioInterface::handle_presetButton     () {
        if (faxPresets. isVisible ())
           faxPresets. hide ();
        else
           faxPresets. show ();
}

void	RadioInterface::handle_saveFrequency	() {
int frequency	= inputHandler -> getVFOFrequency	();
	faxPresets. addElement (frequency);
	fprintf (stderr, "adding %d\n", frequency);
}

void	RadioInterface::handle_saveContinuous () {
	saveContinuous = !saveContinuous;
	saveContinuousButton -> setText (
	            saveContinuous ? "continuous on" : "saveContinuous");
	if (saveContinuous) {
	   saveSingle = false;
	   saveSingleButton	-> setText ("saveSingle");
	} 
}

void	RadioInterface::handle_saveSingle	() {
	saveSingle = !saveSingle;
	saveSingleButton	-> setText (saveSingle ? "Saving on": 
	                                                  "saveSingle");
	if (saveSingle) {
	   saveContinuous = false;
	   saveContinuousButton	-> setText ("saveSingle");
	} 
}

void	RadioInterface::set_disconnect		() {
	close ();
}

