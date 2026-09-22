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
#pragma once

#include	<QString>
#include	<QFrame>
#include	<QImage>
#include	"constants.h"
#include	<mutex>
#include	<atomic>

#include	"down-converter.h"
#include	"super-frame.h"
//	for the input handling
#include	"message-handler.h"

#include	"preset-handler.h"
//      for the payload we have
#include        "ringbuffer.h"
#include        <stdint.h>
#include	"fax-shifter.h"
#include	"fax-bandfilter.h"
#include	"lowpassfilter.h"
#include	"fax-params.h"
#include	"fax-scroller.h"
#include	"fax-image.h"
#include	"ui_fax-decoder.h"

/*
 *      states:
 *      we look at the bits
 */
#define APTSTART                0001
#define WAITING_FOR_START       0002
#define MAYBE_START		0004
#define START_RECOGNIZED        0010
#define WAITING_FOR_PHASE       0020
#define READ_PHASE              0040
#define SYNCED                  0100
#define FAX_DONE                0200
#define WAIT_A_WHILE		0400

#define	FAX_AM			0100
#define	FAX_FM		        0101

class RadioInterface : public superFrame, public Ui_fax_decoder {
Q_OBJECT
public:
	        RadioInterface		(const QString &);
		~RadioInterface		();


//      coming from the GUI
	void	fax_setIOC              (const QString &);
	void	fax_setMode             (const QString &);
	void	fax_setPhase            (const QString &);
	void	fax_setColor            (const QString &);
	void	fax_setDeviation        (const QString &);
	void	handle_resetButton	();
	void	handle_cheatButton	();
	void	set_overflow		(int);
	void	regenerate		();
	void	handle_pictureShift	(int);
	bool	toneTest		(int *, int, int);
//
//	conversion
	std::vector<float>	convBuffer;
	std::vector<int>	mapTable_int;
	std::vector<float>	mapTable_float;
	int	convIndex;
	enum Teint {
	   FAX_COLOR            = 1,
	   FAX_GRAY             = 2,
	   FAX_BLACKWHITE       = 3
	};

private:
//
//	we need some functions to get the data in from the SDRuno
	messageHandler		*inputHandler;
	RingBuffer<Complex>     inputBuffer;
	faxBandfilter	        passbandFilter;
	faxShifter	        localMixer;
	new_downConverter	theDecimator;
	LowPassFIR		faxLowPass;
	std::vector<int>	faxLineBuffer;	
	faxScroller		faxContainer;
	presetHandler		faxPresets;

	std::atomic<int>	overflow;
	int			selected_deviaton;
        QString			selected_IOC;
        int			selected_Mode;
        int			selected_Phase;
        int			selected_Color;

	std::atomic<int>	shiftRequest;
	bool			shiftDone;
	int			samplesToShift;
//
	std::atomic<bool>	running;
	faxParams	*getFaxParams	(const QString &);
	int	        faxAudioRate;

	void		fax_setup	(const QString &s);

	void		processSample	(int);

	int	        checkPhase	(std::vector<int> &, int, float);
	bool	        checkPhaseLine	(std::vector<int> &, int, float);
	int	        findPhaseLine	(std::vector<int> &, int, int, float);
	int	        shiftBuffer	(std::vector<int> &, int, int);
	void	        processBuffer	(std::vector<int> &, int, int);
	void		processLine	(std::vector<float> &,
	                                 std::vector<float> &, int, int);
	int	        demodulate	(std::complex<float> z);
//
//	These two talk to the FAX screen
	void	        clearScreen	();
	void		fax_displayImage	(const QImage &);
	void		fax_displayImage	(const QImage &, int, int);

	int	        toRead;
	void	        addPixeltoImage		(float val, int, int);
	void	        saveImage_single	();
	void	        saveImage_auto		();

	std::vector<uint8_t>     rawData;
//
//	system wide parameters
	std::atomic<bool>	resetFlag;
	std::atomic<bool>	cheatFlag;
	int16_t         carrier;
	int 		deviation;
	int		bufferSize;
	std::atomic<bool> correcting;
	bool	        saveSingle;
	bool	        saveContinuous;
	int		waitingTime;
	void	        doCorrection	(int);
	QString		getSaveName	();
	std::vector<int>	checkBuffer;
//
//	Mode specifics
	struct {
	   QString	name;
	   int16_t      fax_IOC;
	   float	lpm;
	   int16_t	aptStartFreq;
	   int16_t	aptStopFreq;
	   bool		phaseInvers;
	   uint8_t	faxColor;
	   int32_t	samplesperLine; 
	   int16_t	nrColumns;
	   int		nrLines;
	   int		demodMode;
	} faxMode;
//
//	Fax instance specific parameters
	struct {
	   int		faxState;
	   int		bufferP;
	   int		checkP;
	   int		linesRecognized;
	   int		alarmCount;
	   int	        currentSampleIndex;
	   int		currentColumn;
	   int16_t	lastRow; 
	   int	        stoppers;
	   int	        sampleOffset;
	   bool		normal_entry;
	   bool		setCorrection;
	   faxImage	*theImage;
	   int		startCounter;
	   int		stopCounter;
	} theFax;
public slots:
	void		doConnect		();
	void		handle_quit		();
	void		set_disconnect		();
	void		handle_connection_failed	();
	void		handle_connection_succeeded	();
	void		onDisconnect		();
	void		handle_continueButton	();
	void		handle_saveSingle	();
	void		handle_saveContinuous	();
	void		sampleHandler		(int);
	void		fax_setCorrection	();
	void		handle_preset		(const QString &);
	void		handle_presetButton	();
	void		handle_saveFrequency	();
};
