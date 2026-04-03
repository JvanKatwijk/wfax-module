

#pragma once

#include	<complex>
#include	<vector>
#include	<stdint.h>


class	oscillator {
private:
	int	phase;
	int	rate;
	std::vector<std::complex <float>> theVector;
public:
		oscillator (int);
		~oscillator	();
	std::complex<float> next	(int);
};

