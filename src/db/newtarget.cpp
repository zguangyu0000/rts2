/* 
 * Create new observation target.
 * Copyright (C) 2006-2009 Petr Kubanek <petr@kubanek.net>
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

#include "rts2db/appdb.h"
#include "rts2db/target.h"
#include "rts2db/targetset.h"
#include "configuration.h"
#include "libnova_cpp.h"
#include "askchoice.h"

#include "rts2targetapp.h"

#include <iostream>
#include <iomanip>
#include <list>
#include <stdlib.h>
#include <sstream>

#define OPT_PI_NAME         OPT_LOCAL + 320
#define OPT_PROGRAM_NAME    OPT_LOCAL + 321

class Rts2NewTarget:public Rts2TargetApp
{
	public:
		Rts2NewTarget (int in_argc, char **in_argv);
		virtual ~ Rts2NewTarget (void);

		virtual int doProcessing ();
	protected:
		virtual void usage ();

		virtual int processOption (int in_opt);
		virtual int processArgs (const char *arg);
	private:
		int n_tar_id;
		bool tryMatch;
		bool forcedRun;
		const char *n_tar_name;
		const char *n_tar_ra_dec;
		double radius;

		const char *n_pi;
		const char *n_program;

		int saveTarget ();
};

Rts2NewTarget::Rts2NewTarget (int in_argc, char **in_argv):Rts2TargetApp (in_argc, in_argv)
{
	n_tar_id = -1;
	tryMatch = false;
	forcedRun = false;
	n_tar_name = NULL;
	n_tar_ra_dec = NULL;

	n_pi = NULL;
	n_program = NULL;

	radius = NAN;

	addOption ('a', NULL, 0, "autogenerate target IDs");
	addOption ('m', NULL, 0, "try to match target name and RA DEC");
	addOption ('r', NULL, 2, "radius for target checks");
	addOption ('f', NULL, 0, "force run, don't ask questions about target overwrite");

	addOption (OPT_PI_NAME, "pi", 1, "set PI name");
	addOption (OPT_PROGRAM_NAME, "program", 1, "set program name");
}

Rts2NewTarget::~Rts2NewTarget (void)
{
}

void Rts2NewTarget::usage ()
{
	std::cout << "You can specify target on command line. Arguments must be in following order:" << std::endl
		<< "  <target_id> <target_name> <target ra + dec>" << std::endl
		<< "If you specify them, you will be quired only if there exists target within 10' from target which you specified. You can omit target_id if you add -a option." << std::endl
		<< std::endl
		<< "To enter new target called NGC567, resolved by Simbad, with ID 1003:" << std::endl
		<< "  " << getAppName () << " 1003 NGC567" << std::endl
		<< "With autogenerated ID:" << std::endl
		<< "  " << getAppName () << " -a NGC567" << std::endl
		<< "Specifiing RA DEC position:" << std::endl
		<< "  " << getAppName () << " 1003 NGC567 '20:10:11 +11:14:15'" << std::endl
		<< "Same as above, but don't bug user with questions:" << std::endl
		<< "  " << getAppName () << " -f 1003 NGC567 '20:10:11 +11:14:15'" << std::endl
		<< std::endl;
}

int Rts2NewTarget::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 'a':
			n_tar_id = INT_MIN;
			break;
		case 'm':
			n_tar_id = INT_MIN;
			tryMatch = true;
			break;
		case 'r':
			if (optarg)
				radius = atof (optarg);
			else
				radius = 1.0 / 60.0;
			break;
		case 'f':
			forcedRun = true;
			break;
		case OPT_PI_NAME:
			n_pi = optarg;
			break;
		case OPT_PROGRAM_NAME:
			n_program = optarg;
			break;
		default:
			return Rts2TargetApp::processOption (in_opt);
	}
	return 0;
}

int Rts2NewTarget::processArgs (const char *arg)
{
	if (n_tar_id == -1)
		n_tar_id = atoi (arg);
	else if (n_tar_name == NULL)
		n_tar_name = arg;
	else if (n_tar_ra_dec == NULL)
		n_tar_ra_dec = arg;
	else
		return -1;
	return 0;
}

int Rts2NewTarget::saveTarget ()
{
	std::string target_name;
	int ret;

	if (n_tar_id == -1)
	{
		do
		{
			n_tar_id = INT_MIN;
			askForInt ("Target ID (1 to 49999)", n_tar_id);
			if (n_tar_id >= 50000)
			{
				std::string reply;
				askForString ("You are requesting target ID above 50000. This will affect GRB autonomously addeed targets. Please confirm you decision by typing: I know that asking for ID above 50000 will do harm to GRBs!", reply);
				if (reply == std::string ("I know that asking for ID above 50000 will do harm to GRBs!"))
					break;
			}
		}
		while (n_tar_id >= 50000 || (n_tar_id <= 0 && n_tar_id != INT_MIN));
	}
	// create target if we don't create it..
	if (n_tar_name == NULL)
	{
		target_name = target->getTargetName ();
		askForString ("Target NAME", target_name);
	}
	else
	{
		target_name = std::string (n_tar_name);
	}
	target->setTargetName (target_name.c_str ());

	if (!isnan (radius))
	{
		rts2db::TargetSet tarset = target->getTargets (radius);
		tarset.load ();
		if (tarset.size () == 0)
		{
			std::cout << "No targets were found within " << LibnovaDegDist (radius)
				<< " from entered target." << std::endl;
		}
		else
		{
			std::cout << "Following targets were found within " <<
				LibnovaDegDist (radius) << " from entered target:" << std::
				endl << tarset << std::endl;
			if (askForBoolean ("Would you like to enter target anyway?", false)
				== false)
			{
				std::cout << "No target created, exiting." << std::endl;
				return -1;
			}
		}
	}

	if (n_tar_id > 0)
		ret = target->saveWithID (forcedRun, n_tar_id);
	else
		ret = target->save (forcedRun);

	if (ret)
	{
		if (askForBoolean ("Target with given ID already exists. Do you want to overwrite it?", false))
		{
			if (n_tar_id != INT_MIN)
				ret = target->saveWithID (true, n_tar_id);
			else
				ret = target->save (true);
		}
		else
		{
			std::cout << "No target created, exiting." << std::endl;
			return -1;
		}
	}

	struct ln_equ_posn pos;
	target->getPosition (&pos);

	struct ln_hrz_posn hrz;
	target->getAltAz (&hrz);

	if (n_pi)
		target->setPIName (n_pi);
	if (n_program)
	  	target->setProgramName (n_program);

	std::cout << "Created target #" << target->getTargetID ()
		<< " named " << target->getTargetName ()
		<< " on J2000.0 coordinates " << LibnovaRaDec (&pos)
		<< " horizontal " << LibnovaHrz (&hrz) << std::endl;

	if (ret)
	{
		std::cerr << "Error when saving target." << std::endl;
	}
	return ret;
}

int Rts2NewTarget::doProcessing ()
{
	double t_radius = 10.0 / 60.0;
	if (!isnan (radius))
		t_radius = radius;
	int ret;
	// ask for target name..
	if (n_tar_ra_dec == NULL)
	{
		if (n_tar_name == NULL)
		{
			std::cout << "Default values are written inside [].." << std::endl;
			ret = askForObject ("Target name, RA&DEC or anything else (MPEC one line, TLEs separated with |,..)");
		}
		else
		{
			ret = askForObject ("Target name, RA&DEC or anything else (MPEC one line, TLEs separated with |,..)", std::string (n_tar_name));
		}
	}
	else
	{
		ret = askForObject ("Target, RA&DEC or anything else (MPEC one line, TLEs separated with |,..)", std::string (n_tar_ra_dec));
	}
	if (ret)
		return ret;

	if (tryMatch)
	{
		rts2db::TargetSet ts;
		ts.loadByName (n_tar_name, true);
		if (ts.size () == 1)
		{
			std::cout << "Target #" << ts.begin ()->second->getTargetID () << " matched name " << n_tar_name << std::endl;
			return 0;
		}
		std::cerr << "cannot find target " << n_tar_name << ", inserting new target" << std::endl;
	}

	if (n_tar_id == INT_MIN || forcedRun)
		return saveTarget ();

	rts2core::AskChoice selection (this);
	selection.addChoice ('s', "Save");
	selection.addChoice ('q', "Quit");
	selection.addChoice ('o', "List observations around position");
	selection.addChoice ('t', "List targets around position");

	while (1)
	{
		char sel_ret;
		sel_ret = selection.query (std::cout);
		switch (sel_ret)
		{
			case 's':
				return saveTarget ();
			case 'q':
				return 0;
			case 'o':
				askForDegrees ("Radius", t_radius);
				target->printObservations (t_radius, std::cout);
				break;
			case 't':
				askForDegrees ("Radius", t_radius);
				target->printTargets (t_radius, std::cout);
				break;
			default:
				std::cerr << "Unknow key pressed: " << sel_ret << std::endl;
				return -1;
		}
	}
}

int main (int argc, char **argv)
{
	Rts2NewTarget app = Rts2NewTarget (argc, argv);
	return app.run ();
}
