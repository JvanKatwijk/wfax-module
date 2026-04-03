
#include	"oscillator.h"
#include	"math.h"


	oscillator::oscillator	(int rate) {
	this	-> rate	= rate;
	this	-> phase	= 0;
	theVector. resize (rate);
	for (int i = 0; i < rate; i ++)
	   theVector [i] = std::complex<float> (
	                             cos ((float)i / rate * 2 * M_PI),
	                             sin ((float)i / rate * 2 * M_PI));
}

	oscillator::~oscillator	() {}

std::complex<float> oscillator::next (int step) {
	if ((step > rate) || (step < -rate)) {
	   fprintf (stderr, "step fout %d\n", step);
	   step = 0;
	}
	phase += step;
	if (phase >= rate)
	   phase -= rate;
	if (phase < 0)
	   phase += rate;
	return theVector [phase];
}


	
