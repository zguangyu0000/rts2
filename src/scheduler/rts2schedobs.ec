/*
 * Scheduled observation.
 * Copyright (C) 2008 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "rts2schedobs.h"

Rts2SchedObs::Rts2SchedObs (Ticket *_ticket, double _startJD, unsigned int _loopCount)
{
	ticket = _ticket;
	startJD = _startJD;
	loopCount = _loopCount;
}


Rts2SchedObs::~Rts2SchedObs (void)
{
}


double
Rts2SchedObs::altitudeMerit (double _start, double _end)
{
	double minA, maxA;
	struct ln_hrz_posn hrz;
	getTarget ()->getMinMaxAlt (_start, _end, minA, maxA);

	getTarget ()->getAltAz (&hrz, startJD);

	if (maxA == minA)
		return 1;

	if ((hrz.alt - minA) / (maxA - minA) > 1)
	{
		std::cout << "hrz.alt: " << hrz.alt << " maxA: " << maxA << std::endl;
		getTarget ()->getMinMaxAlt (_start, _end, minA, maxA);
	}

	return (hrz.alt - minA) / (maxA - minA);
}


std::ostream & operator << (std::ostream & _os, Rts2SchedObs & schedobs)
{
	_os << LibnovaDate (schedobs.getJDStart ()) << " .. " << schedobs.getLoopCount () << " " << schedobs.getTargetId () << std::endl;
	return _os;
}
