//
// Copyright (C) 2008 Irene Ruengeler
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//

package inet.applications.sctpapp;

simple SCTPServer like SCTPApp
{
	parameters:
	    string address = default(""); // may be left empty ("")
		int port = default(0); // port number to listen on
		int echoFactor = default(0);
		volatile double echoDelay @unit(s) = default(0s);
		double delayFirstRead @unit(s) = default(0s);
		volatile double readingInterval @unit(s) = default(0s);	
		int messagesToPush = default(0);	
		int numPacketsToSendPerClient = default(0);  // number of requests sent per session
		int numPacketsToReceivePerClient = default(1);
		int requestLength = default(1452); // length of a request (bytes)
		volatile double thinkTime @unit(s) = default(0s); // time gap between requests
		double waitToClose @unit(s) = default(0s); //time to wait between last message sent and abort
		bool ordered = default(true);		
		int outboundStreams = default(1);
		int queueSize = default(0);
	gates:
		input sctpIn;
		output sctpOut;
}

