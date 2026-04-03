#
/*
 *    Copyright (C)   2026
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the cw module
 *
 *    cw module is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    cw module is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with cw module; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include	<QJsonDocument>
#include	<QJsonObject>
#include	"message-handler.h"

static
QString IQstarter       = "{ \"event_type\":\"iq_stream_enable\",\"property\":\"\",\"value\":\"%1\" }";

#define STEP            (DEVICE_RATE / LOWRATE)

	messageHandler::messageHandler (RingBuffer<std::complex<float>> *b,
	                                int startFrequency):
	                                    theOscillator (2000000),
	                                    _I_Buffer (32 * 32768),
	                                    firstDecimator (STEP + 1,
	                                                    0,
	                                                    20000,
                                                            DEVICE_RATE,
                                                            STEP) {

	_O_Buffer		= b;
	vfo_frequency		= startFrequency;
	float denominator	= float (OUTRATE) / DIVIDER;
	float inVal		= float (LOWRATE) / DIVIDER;
	for (int i = 0; i < OUTRATE / DIVIDER; i ++) {
	   mapTable_int [i]	= int (floor (i * (inVal / denominator)));
	   mapTable_float [i] =
	                      i * (inVal / denominator) - mapTable_int [i];
	}
	convIndex		= 0;
	theSocket		= nullptr;
}

	messageHandler::~messageHandler	() {
	if (runMode)
	   iqStreamEnable (false);
	if (theSocket != nullptr)
	   delete theSocket;
}

void	messageHandler::tryConnect (const QString &hostAddress,
	                                              int portNumber) {
	theSocket	= new socketHandler (hostAddress,
	                                     portNumber, &_I_Buffer);
	connect (theSocket, &socketHandler::reportConnect,
	         this, &messageHandler::connection_set);
	connect (theSocket, &socketHandler::reportDisconnect,
	         this, &messageHandler::no_connection);
	theSocket	-> tryConnect ();
}

void	messageHandler::no_connection	() {
	disconnect (theSocket, &socketHandler::reportConnect,
	            this, &messageHandler::connection_set);
	disconnect (theSocket, &socketHandler::reportDisconnect,
	            this, &messageHandler::no_connection);
	emit connection_failed ();
}

void	messageHandler::connection_set	() {
	disconnect (theSocket, &socketHandler::reportConnect,
	            this, &messageHandler::connection_set);
	connect (theSocket, &socketHandler::binDataAvailable,
                 this, &messageHandler::binDataAvailable);
	connect (theSocket, &socketHandler::dispatchMessage,
                 this, &messageHandler::dispatchMessage);
	set_filterBW	(12000);
	setProperty ("device_center_frequency",
	                              QString::number (vfo_frequency));
        setProperty ("device_vfo_frequency",
	                              QString::number (vfo_frequency));
        askProperty ("device_sample_rate");
	emit	connection_succeeded ();
}

//	setFrequency is only used on startup,
//	in operation, the restart/stop functions are used
void	messageHandler::setVFOFrequency	(int freq) {
	setProperty ("device_center_frequency", QString::number (freq));
	setProperty ("device_vfo_frequency", QString::number (freq));
	askProperty ("device_center_frequency");
	askProperty ("device_vfo_frequency");
	this -> vfo_frequency	= freq;
}

int	messageHandler::getVFOFrequency	() {
	return this -> vfo_frequency;
}

void    messageHandler::iqStreamEnable  (bool b) {
	if (theSocket == nullptr)
	   return;
        theSocket -> sendMessage (IQstarter. arg (b ? "true" : "false"));
        askProperty ("device_center_frequency");
}

//	Transfer is in segments of 1 msec
void	messageHandler::binDataAvailable () {
	std::complex<int16_t>  inBuffer [DEVICE_RATE / 1000];
	std::complex<float> outBuffer [OUTRATE / 1000];
	while (_I_Buffer. GetRingBufferReadAvailable () >=
	                                            DEVICE_RATE / 1000) {
	   _I_Buffer. getDataFromBuffer (inBuffer, DEVICE_RATE / 1000);
	   if (!runMode)	// only deal with data when processing is on
	      continue;

	   for (int i = 0; i < DEVICE_RATE / 1000; i ++) {
	      std::complex<float> temp =
	          std::complex<float> (real (inBuffer [i]) / 2048.0,
	                               imag (inBuffer [i]) / 2048.0);
	      temp *= conj (theOscillator.
	                        next (vfo_frequency - center_frequency));
	      if (!firstDecimator. Pass (temp, &temp))
	         continue;
	      Complex localBuf [OUTRATE / 1000];
	      convBuffer [convIndex ++] = temp;
	      if (convIndex > CONV_SIZE) {
	         for (int j = 0; j < OUTRATE / 1000; j ++) {
	            int16_t inpBase     = mapTable_int [j];
                    float   inpRatio    = mapTable_float [j];
	            localBuf [j]        =
                                     convBuffer [inpBase + 1] * inpRatio +  
                                     convBuffer [inpBase] * (1 - inpRatio);
                 }
	         _O_Buffer -> putDataIntoBuffer (localBuf, OUTRATE / 1000);
	         convBuffer [0] = convBuffer [CONV_SIZE];
	         convIndex = 1;
	      }
	      if (_O_Buffer -> GetRingBufferReadAvailable () > OUTRATE / 10)
	         emit dataAvailable (OUTRATE / 10);
	   }
	}
}

//
//	On start up we inquire for basic values. after that
//	we react upon changes in the values
void	messageHandler::dispatchMessage	(const QString &m) {
bool b;
QJsonObject obj;
QJsonDocument doc = QJsonDocument::fromJson (m. toUtf8 ());

	if (doc. isNull ())
	   return;	// cannot handle
	obj	= doc. object ();

	QString eventType	= obj ["event_type"]. toString ();
	QString property = obj ["property"]. toString ();
//
//	The initial value inquiries
	if (eventType == "get_property_response") {
	   if (property == "device_sample_rate") {
	      QString samplerate = obj ["value"]. toString ();
              double rate       = samplerate. toDouble (&b);
              if (!b)
                 return;
//      we expect 2000000 and do not process (much) lower/higher rates
              if ((rate > 2500000) || (rate < 1500000)) {
//	          emit rateError ();
                 return;
              }
              iqStreamEnable (true);
	      runMode	= true;
           }
	   if (property == "device_center_frequency") {
	      QString freqString = obj ["value"]. toString ();
//	      fprintf (stderr, "response centerfreq %s\n",
//	                                      freqString. toLatin1 (). data ());
	      double freq	= freqString. toDouble (&b);
	      if (!b)
	         return;
	      center_frequency	= (int)freq;
	   }
	   if (property == "device_vfo_frequency") {
	      QString freqString = obj ["value"]. toString ();
//	      fprintf (stderr, "response vfofreq %s\n",
//	                                      freqString. toLatin1 (). data ());
	      double freq	= freqString. toDouble (&b);
	      if (!b)
	         return;
	      vfo_frequency	= (int)freq;
	   }
	}
//
//	The mods in values
	if (eventType == "property_changed") {
	   QString property = obj ["property"]. toString ();
	   if (property == "device_vfo_frequency") {
	      QString vfoString = obj ["value"]. toString ();
	      bool b;
	      int vfo	= vfoString. toInt (&b);
	      if (!b)
	         return;
	      vfo_frequency	= vfo;
//	      fprintf (stderr, "change in vfo %d (%d)\n",
//	                                  vfo_frequency, center_frequency);
	      emit frequency_changed (vfo);
	   }
	   if (property == "device_center_frequency") {
	      QString freqString = obj ["value"]. toString ();
//	      fprintf (stderr, "property device_center_freq %s\n",
//	                             freqString. toLatin1 (). data ());
	      bool b;
	      int centerFreq	= freqString. toInt (&b);
	      if (!b)
	         return;
	      center_frequency	= centerFreq;
//	      fprintf (stderr, "device_center_frequency %d (%d)\n",
//	                                             centerFreq, vfo_frequency);
//	      emit frequency_changed (vfo);
	   }
	   if (property == "signal_power") {
	      QString snrString = obj ["value"]. toString ();
	      QString res;
	      for (int i = 0; i < snrString. size (); i ++)
                  if (snrString. at (i) == QChar (','))
                     res. push_back (QChar ('.'));
                  else
                     res. push_back (snrString. at (i));
	      bool b;
	      double snr = res. toDouble (&b);
	      if (!b)
	         return;
	      emit signalPower (snr);
	   }
	}
}

void	messageHandler::setProperty (const QString prop, const QString val) {
QString message = "{ \"event_type\":\"set_property\",\"property\":\"%1\",\"value\":\"%2\" }";
	theSocket -> sendMessage (message. arg (prop).arg (val));
}

void	messageHandler::askProperty (const QString prop) {
QString  message = "{ \"event_type\":\"get_property\",\"property\":\"%1\" }";
	theSocket -> sendMessage (message. arg (prop));
}

void	messageHandler::set_filterBW	(uint32_t bw) {
	setProperty ("filter_bandwidth", QString::number (bw));
}

