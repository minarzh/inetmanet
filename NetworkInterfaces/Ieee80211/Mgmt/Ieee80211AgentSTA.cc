//
// Copyright (C) 2006 Andras Varga
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
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//


#include "Ieee80211AgentSTA.h"
#include "Ieee80211Primitives_m.h"
#include "NotifierConsts.h"



Define_Module(Ieee80211AgentSTA);


void Ieee80211AgentSTA::initialize(int stage)
{
    if (stage==0)
    {
        // read parameters
        probeDelay = par("probeDelay");
        minChannelTime = par("minChannelTime");
        maxChannelTime = par("maxChannelTime");
        authenticationTimeout = par("authenticationTimeout");
        associationTimeout = par("associationTimeout");

        NotificationBoard *nb = NotificationBoardAccess().get();
        nb->subscribe(this, NF_L2_BEACON_LOST);
        nb->subscribe(this, NF_L2_ASSOCIATED);

        // start up: send scan request; XXX refine! e.g. use scheduleAt
        sendScanRequest();
    }
}

void Ieee80211AgentSTA::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
        handleTimer(msg);
    else
        handleResponse(msg);
}

void Ieee80211AgentSTA::handleTimer(cMessage *msg)
{
    error("internal error: unrecognized timer '%s'", msg->name());
}

void Ieee80211AgentSTA::handleResponse(cMessage *msg)
{
    cPolymorphic *ctrl = msg->removeControlInfo();
    delete msg;

    EV << "Processing confirmation from mgmt: " << ctrl->className() << "\n";

    if (dynamic_cast<Ieee80211Prim_ScanConfirm *>(ctrl))
        processScanConfirm((Ieee80211Prim_ScanConfirm *)ctrl);
    else if (dynamic_cast<Ieee80211Prim_AuthenticateConfirm *>(ctrl))
        processAuthenticateConfirm((Ieee80211Prim_AuthenticateConfirm *)ctrl);
    else if (dynamic_cast<Ieee80211Prim_AssociateConfirm *>(ctrl))
        processAssociateConfirm((Ieee80211Prim_AssociateConfirm *)ctrl);
    else if (ctrl)
        error("handleResponse(): unrecognized control info class `%s'", ctrl->className());
    else
        error("handleResponse(): control info is NULL");
    delete ctrl;
}

void Ieee80211AgentSTA::receiveChangeNotification(int category, cPolymorphic *details)
{
    Enter_Method_Silent();

    if (category == NF_L2_BEACON_LOST)
    {
        //XXX should check details if it's about this NIC
        EV << className() << ": beacon lost, starting scanning again\n"; //XXX should we get disassociated or anything before that?
        parentModule()->parentModule()->bubble("Beacon lost!");
        sendScanRequest();
    }
    else if (category == NF_L2_ASSOCIATED)
    {
        //XXX should check details if it's about this NIC
        // we don't really need to react on this (we get the confirm anyway)
        EV << className() << ": associated with AP\n";
        parentModule()->parentModule()->bubble("Associated with AP");
    }
}

void Ieee80211AgentSTA::sendRequest(Ieee80211Prim *req)
{
    cMessage *msg = new cMessage(req->className());
    msg->setControlInfo(req);
    send(msg, "mgmtOut");
}


void Ieee80211AgentSTA::sendScanRequest()
{
    EV << "Sending ScanRequest primitive to mgmt\n";
    Ieee80211Prim_ScanRequest *req = new Ieee80211Prim_ScanRequest();
    req->setBSSType(BSSTYPE_INFRASTRUCTURE);
    req->setActiveScan(true);
    req->setProbeDelay(probeDelay);
    req->setMinChannelTime(minChannelTime);
    req->setMaxChannelTime(maxChannelTime);
    // BSSID, SSID and channelList[] are left at default ("any")

    sendRequest(req);
}

void Ieee80211AgentSTA::sendAuthenticateRequest(const MACAddress& address, int authType)
{
    EV << "Sending AuthenticateRequest primitive to mgmt\n";
    Ieee80211Prim_AuthenticateRequest *req = new Ieee80211Prim_AuthenticateRequest();
    req->setAddress(address);
    req->setAuthType(authType);
    req->setTimeout(authenticationTimeout);
    sendRequest(req);
}

void Ieee80211AgentSTA::sendAssociateRequest(const MACAddress& address)
{
    EV << "Sending AssociateRequest primitive to mgmt\n";
    Ieee80211Prim_AssociateRequest *req = new Ieee80211Prim_AssociateRequest();
    req->setAddress(address);
    req->setTimeout(associationTimeout);
    //XXX    Ieee80211CapabilityInformation capabilityInfo;
    //XXX    int listenInterval; // unsupported by MAC
    sendRequest(req);
}

void Ieee80211AgentSTA::processScanConfirm(Ieee80211Prim_ScanConfirm *resp)
{
    // choose best AP
    int bssIndex = chooseBSS(resp);
    if (bssIndex==-1)
    {
        EV << "No (suitable) AP found, continue scanning\n";
        sendScanRequest();
        return;
    }

    dumpAPList(resp);

    Ieee80211Prim_BSSDescription& bssDesc = resp->getBssList(bssIndex);
    EV << "Chosen AP address=" << bssDesc.getBSSID() << " from list, starting authentication\n";
    sendAuthenticateRequest(bssDesc.getBSSID(), AUTHTYPE_SHAREDKEY); //XXX or AUTHTYPE_OPENSYSTEM -- should be parameter?
}

void Ieee80211AgentSTA::dumpAPList(Ieee80211Prim_ScanConfirm *resp)
{
    EV << "Received AP list:\n";
    for (int i=0; i<resp->getBssListArraySize(); i++)
    {
        Ieee80211Prim_BSSDescription& bssDesc = resp->getBssList(i);
        EV << "    " << i << ". "
           << " address=" << bssDesc.getBSSID()
           << " channel=" << bssDesc.getChannel()
           << " SSID=" << bssDesc.getSSID()
           << " beaconIntvl=" << bssDesc.getBeaconInterval()
           << " rxPower=" << bssDesc.getRxPower()
           << endl;
        // later: supportedRates, capabilityInfo
    }
}

int Ieee80211AgentSTA::chooseBSS(Ieee80211Prim_ScanConfirm *resp)
{
    if (resp->getBssListArraySize()==0)
        return -1;

    // here, just choose the one with the greatest receive power
    // TODO and which supports a good data rate we support
    int bestIndex = 0;
    for (int i=0; i<resp->getBssListArraySize(); i++)
        if (resp->getBssList(i).getRxPower() > resp->getBssList(bestIndex).getRxPower())
            bestIndex = i;
    return bestIndex;
}

void Ieee80211AgentSTA::processAuthenticateConfirm(Ieee80211Prim_AuthenticateConfirm *resp)
{
    if (resp->getResultCode()!=PRC_SUCCESS)
    {
        EV << "Authentication error\n";

        // try scanning again, maybe we'll have better luck next time, possibly with a different AP
        EV << "Going back to scanning\n";
        sendScanRequest();
    }
    else
    {
        EV << "Authentication successful, let's try to associate\n";
        sendAssociateRequest(resp->getAddress());
    }
}

void Ieee80211AgentSTA::processAssociateConfirm(Ieee80211Prim_AssociateConfirm *resp)
{
    if (resp->getResultCode()!=PRC_SUCCESS)
    {
        EV << "Association error\n";

        // try scanning again, maybe we'll have better luck next time, possibly with a different AP
        EV << "Going back to scanning\n";
        sendScanRequest();
    }
    else
    {
        EV << "Association successful\n";
        // we are happy!
    }
}



