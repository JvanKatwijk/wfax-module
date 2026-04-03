#

#pragma once

#include	<complex>
#include	<vector>
#include	<math.h>
#include	"constants.h"


class	upFilter {
	std::vector<Complex> kernel;
	std::vector<Complex> buffer;
	int		ip;
	int		order;
	int		bufferSize;
	int		multiplier;
public:
	upFilter	(int, int, int);
	~upFilter	();
void	Filter	(Complex, Complex *);
};

