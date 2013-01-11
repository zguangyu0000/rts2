/* 
 * BB API access for RTS2.
 * Copyright (C) 2012 Petr Kubanek, Institute of Physics <kubanek@fzu.cz>
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

#include "xmlrpcd.h"
#include "rts2db/constraints.h"
#include "rts2json/jsonvalue.h"

#ifdef RTS2_JSONSOUP
#include <glib-object.h>
#include <json-glib/json-glib.h>
#endif // RTS2_JSONSOUP

using namespace rts2xmlrpc;

BBAPI::BBAPI (const char* prefix, rts2json::HTTPServer *_http_server, XmlRpc::XmlRpcServer* s):rts2json::JSONRequest (prefix, _http_server, s)
{
}

void BBAPI::executeJSON (std::string path, XmlRpc::HttpParams *params, const char* &response_type, char* &response, size_t &response_length)
{
	std::vector <std::string> vals = SplitStr (path, std::string ("/"));
  	std::ostringstream os;

	os.precision (8);
	os << std::fixed;

	// calls returning binary data
	if (vals.size () == 1)
	{
		// return time the observatory might be able to schedule the request
		if (vals[0] == "schedule" || vals[0] == "confirm")
		{
			rts2db::Target *tar = getTarget (params);
			double from = params->getDouble ("from", getNow ());
			double to = params->getDouble ("to", from + 86400);
			if (to < from)
			{
				delete tar;
				throw JSONException ("to time is before from time");
			}

			const char *schedule_id;
			if (vals[0] == "confirm")
			{
				schedule_id = params->getString ("schedule_id", "");
				if (schedule_id[0] == '\0')
				{
					delete tar;
					throw JSONException ("missing schedule ID");
				}
			}

			// find free spots
			XmlRpcd *master = (XmlRpcd*) getMasterApp ();
			connections_t::iterator iter = master->getConnections ()->begin ();

			master->getOpenConnectionType (DEVICE_TYPE_SELECTOR, iter);
			
			rts2core::TimeArray *free_start = NULL;
			rts2core::TimeArray *free_end = NULL;

			std::vector <double>::iterator iter_fstart;
			std::vector <double>::iterator iter_fend;

			if (iter != master->getConnections ()->end ())
			{
				rts2core::Value *f_start = (*iter)->getValue ("free_start");
				rts2core::Value *f_end = (*iter)->getValue ("free_end");
				if (f_start == NULL || f_end == NULL)
				{
					logStream (MESSAGE_WARNING) << "cannot find free_start or free_end variables in " << (*iter)->getName () << sendLog;
				}
				else if (f_start->getValueType () == (RTS2_VALUE_TIME | RTS2_VALUE_ARRAY) && f_end->getValueType () == (RTS2_VALUE_TIME | RTS2_VALUE_ARRAY))
				{
					free_start = (rts2core::TimeArray*) f_start;
					free_end = (rts2core::TimeArray*) f_end;

					iter_fstart = free_start->valueBegin ();
					iter_fend = free_end->valueBegin ();
				}
				else
				{
					logStream (MESSAGE_WARNING) << "invalid free_start or free_end types: " << std::hex << f_start->getValueType () << " " << std::hex << f_end->getValueType () << sendLog;
				}
			}

			// get target observability
			rts2db::ConstraintsList violated;
			time_t f = from;
			double JD = ln_get_julian_from_timet (&f);
			time_t tto = to;
			double JD_end = ln_get_julian_from_timet (&tto);
			double dur = rts2script::getMaximalScriptDuration (tar, ((XmlRpcd *) getMasterApp ())->cameras);
			if (dur < 60)
				dur = 60;
			dur /= 86400.0;
			// go through nights
			double t;
			
			double fstart_JD = NAN;
			double fend_JD = NAN;

			if (free_start && iter_fstart != free_start->valueEnd () && iter_fend != free_end->valueEnd ())
			{
				f = *iter_fstart;
				fstart_JD = ln_get_julian_from_timet (&f);

				f = *iter_fend;
				fend_JD = ln_get_julian_from_timet (&f);

				if (JD < fstart_JD)
					JD = fstart_JD;
			}

			for (t = JD; t < JD_end; t += dur)
			{
				if (t > fend_JD)
				{
					iter_fstart++;
					iter_fend++;
					if (iter_fstart == free_start->valueEnd () || iter_fend == free_end->valueEnd ())
					{
						t = JD_end;
						break;
					}
					else
					{
						f = *iter_fstart;
						fstart_JD = ln_get_julian_from_timet (&f);

						f = *iter_fend;
						fend_JD = ln_get_julian_from_timet (&f);
					}
				}
				if (tar->getViolatedConstraints (t).size () == 0)
				{
					double t2;
					// check full duration interval
					for (t2 = t; t2 < t + dur; t2 += 60 / 86400.0)
					{
						if (tar->getViolatedConstraints (t2).size () > 0)
						{
							t = t2;
							continue;
						}
					}
					if (t2 >= t + dur)
					{
						ln_get_timet_from_julian (t, &f);
						os << f;
						if (vals[0] == "confirm")
							confirmSchedule (tar, f, schedule_id);
						break;
					}
				}
			}
			if (t >= JD_end)
				os << "0";
			delete tar;
		}
		else if (vals[0] == "cancel")
		{
			const char *schedule_id = params->getString ("schedule_id", "");

			BBSchedules::iterator iter = schedules.find (schedule_id);
			if (iter == schedules.end ())
				throw JSONException ("invalid schedule id");
		}
		else
		{
			throw JSONException ("invalid request" + path);
		}

	}
	returnJSON (os.str ().c_str (), response_type, response, response_length);
}

void BBAPI::confirmSchedule (rts2db::Target *tar, double f, const char *schedule_id)
{
	BBSchedules::iterator iter = schedules.find (schedule_id);
	if (iter != schedules.end ())
		delete iter->second;

	BBSchedule *sched = new BBSchedule (std::string (schedule_id), tar->getTargetID (), f);
	
	schedules[std::string (schedule_id)] = sched;

	((XmlRpcd*) getMasterApp ())->confirmSchedule (sched);
}
