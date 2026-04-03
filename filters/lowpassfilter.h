#
/*
 *    Copyright (C) 2026
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the  fax plugin
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

#include	<complex>
#include	<vector>
#include	"constants.h"

class LowPassFIR {
public:
		LowPassFIR	(int16_t filterSize,
	                         int32_t Fc,
	                         int32_t sampleRate);
		~LowPassFIR	();
Complex		Pass		(Complex z);
void		newKernel	(int32_t Fc);

private:
	std::vector<Complex> filterKernel;
	std::vector<Complex> Buffer;
	int	filterSize;
	int	sampleRate;
	int	ip;
};

