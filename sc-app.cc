#include "sc-app.h"
#include <iostream>
#include <iomanip>
using namespace ns3;
std::string str_extradelay = "100ms";
NS_LOG_COMPONENT_DEFINE("SCApp");
#define INFHOPS  1000
//#define mydebug
static std::string GetPosition(Ptr<SCApp> app) {
	std::stringstream ss;
	ss << "(";
	ss << (int) app->GetNode()->GetObject<MobilityModel>()->GetPosition().x;
	ss << ",";
	ss << (int) app->GetNode()->GetObject<MobilityModel>()->GetPosition().y;
	ss << ") ";
	return ss.str();
}
static Time extra_ms = Time(str_extradelay);
#ifdef mydebug
#define debug_node1 10
#define debug_node2 0
#define debug_dataid 15
#define debug_nodeip1 "10.0.0.11"
#define debug_nodeip2 "10.0.0.10"
#endif
static void MinusTTL(Ptr<Packet> packet) {
	CacheHeader tmpHeader;
	packet->RemoveHeader(tmpHeader);
	tmpHeader.TTLMinus();
	packet->AddHeader(tmpHeader);
}
static bool CmpSF(Friend f1, Friend f2) {
	return f1.socialFactor < f2.socialFactor;

}
static std::string GetInterests(InterestsSet interests) {
	std::ostringstream oss;
	oss.str("");
	InterestsSet::iterator it;
	for (it = interests.begin(); it != interests.end(); it++) {
		oss << (int) (*it) << " ";
	}
	return oss.str();
}
static void DebugToFile(std::stringstream & msg) {
	std::ofstream ofs;
	ofs.open("scdebug.txt", std::ofstream::out | std::ofstream::app);
	ofs << msg.str() << "\n";
	ofs.close();
}
bool SCApp::CheckAvailble(Ipv4Address ip) {
	for (size_t i = 0; i < friends.size(); i++) {
		if (friends[i].ip == ip) {
			return false;
		}
	}

	return true;
}
void SCApp::SortCacheEntries() {
	uint32_t i, j, flag = 1;
	CacheEntry tmp;
	for (i = 1; i <= m_cacheStore.size() && flag; i++) {
		flag = 0;
		//aesc
		for (j = 0; j < m_cacheStore.size() - 1; j++) {
			if (GetMetric(m_cacheStore[j + 1]) < GetMetric(m_cacheStore[j])) {
				tmp = m_cacheStore[j];
				m_cacheStore[j] = m_cacheStore[j + 1];
				m_cacheStore[j + 1] = tmp;
				flag = 1;
			}
		}

	}

}
bool SCApp::CmpCacheEntry(CacheEntry& ce1, CacheEntry& ce2) {
	return GetMetric(ce1) > GetMetric(ce2);
}
float SCApp::GetMetric(CacheEntry &ce) {
	bool myinterests = (interests.find(ce.m_category) != interests.end());
	bool friinterests = (friends_interests.find(ce.m_category)
			!= friends_interests.end());
	return 1 * myinterests + 0.5 * friinterests;
}
void SCApp::SortFriendsByDis() {
	uint32_t i, j, flag = 1;
	Friend tmp;
	for (i = 1; i <= friends.size() && flag; i++) {
		flag = 0;
		for (j = 0; j < friends.size() - 1; j++) {
			if (GetDistance(friends[j + 1].ip) < GetDistance(friends[j].ip)) {
				tmp = friends[j];
				friends[j] = friends[j + 1];
				friends[j + 1] = tmp;
				flag = 1;
			}
		}

	}
}
TypeId SCApp::GetTypeId(void) {
	static TypeId tid = TypeId("SCApp").SetParent<Application>().AddConstructor<
			SCApp>().AddAttribute("Port", "Destination app port.",
			UintegerValue(1603), MakeUintegerAccessor(&SCApp::m_destPort),
			MakeUintegerChecker<uint32_t>()).AddAttribute("Capacity",
			"Cache Capacity in KB.", UintegerValue(1024 * 64),
			MakeUintegerAccessor(&SCApp::m_cacheCap),
			MakeUintegerChecker<uint32_t>()).AddAttribute("SocialForwardFactor",
			"SocialForwardFactor", DoubleValue(0.5),
			MakeDoubleAccessor(&SCApp::social_forward_factor),
			MakeDoubleChecker<double>(0, 1.0)).AddAttribute("InterestFacotr",
			"InterestFacotr", DoubleValue(0.8),
			MakeDoubleAccessor(&SCApp::interest_factor),
			MakeDoubleChecker<double>(0, 1.0)).AddAttribute("ST", "cc strategy",
			UintegerValue(1), MakeUintegerAccessor(&SCApp::ccstrategy),
			MakeUintegerChecker<uint32_t>(0, 10)).AddAttribute("RequestRate",
			"RequestRate", DoubleValue(10),
			MakeDoubleAccessor(&SCApp::request_rate),
			MakeDoubleChecker<double>(0, 100)).AddAttribute("WarmUp", "warm up",
			BooleanValue(true), MakeBooleanAccessor(&SCApp::dowarmup),
			MakeBooleanChecker());

	return tid;
}

SCApp::SCApp() {
	//NS_LOG_DEBUG_NOARGS ();
	m_uniformRan = CreateObject<UniformRandomVariable>();
	m_01Ran = CreateObject<UniformRandomVariable>();
	m_01Ran->SetAttribute("Min", DoubleValue(0.0));
	m_01Ran->SetAttribute("Max", DoubleValue(1.0));
	recv_socket = 0;
	m_socket = 0;
	isDataSource = false;
	statRequestsInited = 0;
	statRequestReplyed = 0;
	statRequestForwared = 0;
	statRbP = 0;
	statRbS = 0;
	statFailed = 0;
	statReplCnt = 0;
	statLocalHit = 0;
	statUnFinished = 0;
	currentCSsize = 0;
	statRxCnt = 0;
	statSxCnt = 0;

	statReqSxCnt = 0;
	statReqRxCnt = 0;

	statAviSxCnt = 0;
	statAviRxCnt = 0;

	statRetdSxCnt = 0;
	statRetdRxCnt = 0;

	statDelay = 0;
}

SCApp::SCApp(Ptr<SCAppHelper> datahelper) {
	m_uniformRan = CreateObject<UniformRandomVariable>();
	recv_socket = 0;
	m_socket = 0;
	m_dataHelper = datahelper;
}

SCApp::~SCApp() {
	//NS_LOG_DEBUG_NOARGS ();
}

void SCApp::DoDispose(void) {
	//EvaluationStop();
	statUnFinished = m_historyQueryInited.size();

	m_dataHelper->glbstatNode += 1;
	m_dataHelper->gblstatRx += statRxCnt;
	m_dataHelper->glbstatSx += statSxCnt;
	m_dataHelper->gblstatInit += statRequestsInited;
	m_dataHelper->glbstatForwarded += statRequestForwared;
	m_dataHelper->glbstatRbL += statLocalHit;
	m_dataHelper->glbstatRbP += statRbP;
	m_dataHelper->glbstatRbS += statRbS;
	m_dataHelper->glbstatUnfinished += statUnFinished;
	m_dataHelper->glbstatRplace += statReplCnt;
	m_dataHelper->glbstatDelay += statDelay;
	m_dataHelper->glbstatUnusual += statUnusualDelayCnt;

	std::ostringstream oss;
	oss.str("");
	oss << nodeIDstr << " I:" << statRequestsInited << " RbP:" << statRbP
			<< " RbS:" << statRbS << " RbL:" << statLocalHit << " F:"
			<< statRequestForwared << " U:" << statUnFinished << " CS:"
			<< this->m_cacheStore.size() << " " << currentCSsize / 1024 << "/"
			<< m_cacheCap / 1024 << "KB" << " Repl:" << statReplCnt
			<< " UnusCnt:" << statUnusualDelayCnt << " Sx:" << statSxCnt
			<< " Rx:" << statRxCnt << " D:"
			<< statDelay
					/ (statRequestsInited - statUnFinished - statUnusualDelayCnt)
			<< "ms";

	oss << "\tAviSx:" << statAviSxCnt << " AviRx:" << statAviRxCnt << " ReqSx:"
			<< statReqSxCnt << " ReqRx:" << statReqRxCnt << " RetDSx:"
			<< statRetdSxCnt << " RetDRx:" << statRetdRxCnt;

	//oss << PrintDebugInfo();

//	DataIDTimeMap::iterator his_it;
//	oss << "\tU:";
//	for (his_it = m_historyQueryInited.begin();
//			his_it != m_historyQueryInited.end(); his_it++) {
//		oss << (*his_it).first << "S:"
//				<< m_dataHelper->GetDataSize((*his_it).first) << " @ "
//				<< (*his_it).second.GetSeconds() << "\t";
//	}

	oss << "\n";
	m_dataHelper->node_stat.append(oss.str());

	m_socket = 0;
	recv_socket = 0;
	Application::DoDispose();
}
void SCApp::SendEcho(uint32_t nodeid) {
	//std::vector<double> delays;
	std::stringstream ss;

	CacheHeader header;
	header.SetMessageType(CACHE_HEADER_TYPE(1));
	header.SetFrom(m_localAddr);

	Ptr<Packet> echoPacket = Create<Packet>(0);
	echoPacket->AddHeader(header);
	ss.str("");
	ss << "10.0.0.";
	ss << nodeid;
	m_socket->SendTo(echoPacket, 0,
			InetSocketAddress(Ipv4Address(ss.str().c_str()), m_destPort));
	/*std::cout << nodeIDstr << "Send Echo ->" << ss.str() << "@"
	 << Simulator::Now().GetMilliSeconds() << "\n";*/

}
void SCApp::RecvEcho(Ptr<Socket> socket) {
	Ptr<Packet> packet;
	Address from;
	while ((packet = socket->RecvFrom(from))) {
		CacheHeader recvHeader;
		packet->PeekHeader(recvHeader);
		if (recvHeader.GetMessageType() == CACHE_HEADER_TYPE(1)) {
			//std::cout << nodeIDstr << "Recv Echo from" << recvHeader.GetFrom()
			//	<< "\n";
			Ptr<Packet> replyPacket = Create<Packet>(0);
			CacheHeader sendHeader;
			sendHeader.SetFrom(m_localAddr);
			sendHeader.SetMessageType(CACHE_HEADER_TYPE(2));
			replyPacket->AddHeader(sendHeader);
			m_socket->SendTo(replyPacket, 0,
					InetSocketAddress(recvHeader.GetFrom(), m_destPort));
		}

		else if (recvHeader.GetMessageType() == CACHE_HEADER_TYPE(2)) {
			//std::cout << nodeIDstr << "Recv Reply from" << recvHeader.GetFrom()
			//	<<"@"<<Simulator::Now().ToDouble(Time::MS);
			std::stringstream ss;
			ss << recvHeader.GetFrom();
			std::string ipstr = ss.str();

			int idx = atoi(ipstr.substr(ipstr.find_last_of('.') + 1).c_str());

			Time diff = Simulator::Now()
					- Seconds((double) GetNodeID() / 100 + 50 + idx);
			statEcho[idx] = diff.ToDouble(Time::MS);
		}
	}
}
void SCApp::EvaluationStop() {
	statEcho.erase(statEcho.begin());
	std::ofstream ofs;
	ofs.open("echo.csv", std::ios_base::app);
	//ofs << nodeIDstr << "\t";
	for (size_t i = 0; i < statEcho.size(); i++) {
		ofs << statEcho[i] << "\t";
	}
	ofs << "\n";

	ofs.close();

}
void SCApp::EvaluationStartUp() {
	if (recv_socket == 0) {
		Ptr<SocketFactory> socketFactory = GetNode()->GetObject<SocketFactory>(
				UdpSocketFactory::GetTypeId());
		recv_socket = socketFactory->CreateSocket();
		InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(),
				m_destPort);
		recv_socket->Bind(local);
	}
	recv_socket->SetRecvCallback(MakeCallback(&SCApp::RecvEcho, this));
	if (m_socket == 0) {
		Ptr<SocketFactory> socketFactory = GetNode()->GetObject<SocketFactory>(
				UdpSocketFactory::GetTypeId());

		m_socket = socketFactory->CreateSocket();
		m_socket->SetAllowBroadcast(true);
		m_socket->Bind();
	}

	statEcho.resize(m_dataHelper->totalNodes + 1);
	//Simulator::Cancel(m_sendEvent);
	for (uint32_t i = 1; i <= m_dataHelper->totalNodes; i++) {
		Simulator::Schedule(Seconds((double) GetNodeID() / 100 + i),
				&SCApp::SendEcho, this, i);
	}
}
void SCApp::RealStartUp() {
	for (size_t i = 0;
			i < friends.size()
					&& friends_interests.size()
							<= m_dataHelper->tolalIntestsInNetwork; i++) {
		friends_interests.insert(friends[i].interests.begin(),
				friends[i].interests.end());
	}

	if (recv_socket == 0) {
		Ptr<SocketFactory> socketFactory = GetNode()->GetObject<SocketFactory>(
				UdpSocketFactory::GetTypeId());
		recv_socket = socketFactory->CreateSocket();
		InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(),
				m_destPort);
		recv_socket->Bind(local);
	}
	recv_socket->SetRecvCallback(MakeCallback(&SCApp::Receive, this));
	if (m_socket == 0) {
		Ptr<SocketFactory> socketFactory = GetNode()->GetObject<SocketFactory>(
				UdpSocketFactory::GetTypeId());

		m_socket = socketFactory->CreateSocket();
		m_socket->SetAllowBroadcast(true);
		m_socket->Bind();
	}

	Simulator::Cancel(m_sendEvent);
	Simulator::Cancel(m_checkEvent);

	///Simulator::ScheduleNow(&SCApp::PrintSocialInfo, this);
	//Simulator::Schedule(Seconds(10.0), &SCApp::PrintSocialInfo, this);
	//Simulator::Schedule(Seconds(100.0), &SCApp::PrintSocialInfo, this);

	//TODO: for now block request from nodes which have no friends;
	if (!this->isDataSource /*&&GetNodeID()==2*/) {
		m_sendEvent = Simulator::Schedule(
				Seconds(GetNodeID() + (double) GetNodeID() / 100),
				&SCApp::Request, this);
		m_cleanupEvent = Simulator::Schedule(
				Seconds(GetNodeID() + m_dataHelper->intervalRefreshRequestList),
				&SCApp::CleanUpTables, this);
		NS_ASSERT(interests.size()>0);
		NS_ASSERT(friends.size()>0);
		for (size_t i = 0; i < friends.size(); i++) {
			NS_ASSERT(friends[i].centrality>0);
			NS_ASSERT(friends[i].interests.size()>0);
		}
		//WarmUp();
	}
	NS_ASSERT(CheckAvailble(m_localAddr));
}
void SCApp::StartApplication() {
	Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
	Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
	m_localAddr = iaddr.GetLocal();
	std::stringstream ss;
	ss.str();
	ss << "N#" << std::setw(3) << std::left << GetNodeID() + 1;
	this->nodeIDstr = ss.str();
	RealStartUp();
	//EvaluationStartUp();

}

void SCApp::StopApplication() {
//NS_LOG_DEBUG(this);
	Simulator::Cancel(m_sendEvent);
	Simulator::Cancel(m_checkEvent);
	if (recv_socket != 0) {
		recv_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
	}

}

void SCApp::AddRandomFriends(uint32_t numTotalNodes) {
//NS_LOG_DEBUG(this);
	if (!this->isDataSource) {
		Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();

		//elimit the data source
		uint16_t freindcount = x->GetInteger(1, numTotalNodes - 2);
		while (friends.size() < freindcount) {
			std::ostringstream oss;
			oss.flush();
			uint32_t frid = x->GetInteger(1, numTotalNodes - 1);
			oss << "10.0.0." << frid;
			Ipv4Address friendip(oss.str().data());
			if (CheckAvailble(friendip) && GetNodeID() != frid - 1) {
				Friend afriend(friendip);
				friends.push_back(afriend);
			}
		}NS_LOG_DEBUG("Added "<<friends.size() <<" friends on Node#"<<GetNode()->GetId());
	}

}

//TODO just randomly inject some interests
void SCApp::AddRandomInterests(uint32_t numTotalNodes, uint16_t numInterests) {
	if (!this->isDataSource) {
		Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
		uint16_t myIntCnt = x->GetInteger(1, 3);
		for (int i = 0; i < myIntCnt; i++) {
			interests.insert(x->GetInteger(1, numInterests));
		}
	}
}
uint32_t SCApp::GetDistance(Ipv4Address friendip) {
	uint32_t ret = INFHOPS;
	Ptr<RoutingProtocol> routing = GetNode()->GetObject<RoutingProtocol>();
	std::vector<RoutingTableEntry> table = routing->GetRoutingTableEntries();
	std::vector<RoutingTableEntry>::iterator rtit;
	for (rtit = table.begin(); rtit != table.end(); rtit++) {
		if ((*rtit).destAddr == friendip) {
			ret = (*rtit).distance;
			break;
		}
	}
	return ret;
}
void SCApp::SendToHighestCentralityFriend(Ptr<Packet> packet) {
	NS_LOG_DEBUG(nodeIDstr<<" SendToHighestCentralityFriend");
	uint32_t maxCentrality = 0;
	uint32_t maxCentralityIndex = 0;
	for (size_t i = 0; i < friends.size(); i++) {
		if (maxCentrality < friends[i].centrality) {
			maxCentrality = friends[i].centrality;
			maxCentralityIndex = i;
		}
	}
	statSxCnt++;
	CacheHeader ch;
	packet->PeekHeader(ch);
	Ptr<Packet> queryPacket = Create<Packet>(0);
	queryPacket->AddHeader(ch);
	m_socket->SendTo(queryPacket, 0,
			InetSocketAddress(friends[maxCentralityIndex].ip, m_destPort));
	NS_LOG_DEBUG(DebugPacket(queryPacket,1,friends[maxCentralityIndex].ip,"Highest"));
}
void SCApp::ForwardToNeighbors(Ptr<Packet> packet) {
	CacheHeader cacheHeader;
	packet->PeekHeader(cacheHeader);
	Ptr<Packet> forwardPacket = Create<Packet>(0);
	forwardPacket->AddHeader(cacheHeader);

	Ptr<RoutingProtocol> routing = GetNode()->GetObject<RoutingProtocol>();
	std::vector<RoutingTableEntry> table = routing->GetRoutingTableEntries();

	for (uint32_t i = 0; i < table.size() && table[i].distance == 1
//	&& i <= m_dataHelper->maxRequests
			&& table[i].destAddr != cacheHeader.GetFrom(); i++) {
		statSxCnt++;
		m_socket->SendTo(forwardPacket, 0,
				InetSocketAddress(table[i].destAddr, m_destPort));

	}
}
void SCApp::ForwardToRandomNodes(Ptr<Packet> packet) {
	CacheHeader cacheHeader;
	packet->PeekHeader(cacheHeader);
	Ptr<Packet> forwardPacket = Create<Packet>(0);
	forwardPacket->AddHeader(cacheHeader);

	std::ostringstream oss;
	Ipv4Address rnddest;

	for (uint32_t i = 0; i < 3; i++) {
		oss.str("");
		oss << "10.0.0."
				<< m_uniformRan->GetInteger(1, m_dataHelper->totalNodes - 1);
		rnddest = Ipv4Address(oss.str().data());
		while (rnddest == cacheHeader.GetFrom()) {
			oss << "10.0.0."
					<< m_uniformRan->GetInteger(1,
							m_dataHelper->totalNodes - 1);
			rnddest = Ipv4Address(oss.str().data());
		}
		statSxCnt++;
		m_socket->SendTo(forwardPacket, 0,
				InetSocketAddress(rnddest, m_destPort));
		NS_LOG_DEBUG(DebugPacket(forwardPacket,1,rnddest,"RND_FWD"));
	}

}
void SCApp::ForwardToFriends(Ptr<Packet> packet, uint32_t maxDis = 3) {
	CacheHeader cacheHeader;
	packet->PeekHeader(cacheHeader);
	NS_LOG_DEBUG(nodeIDstr<<" ForwardToFriends for "<<cacheHeader.GetFrom()<<" D#"<<cacheHeader.GetDataId());
	Ptr<Packet> forwardPacket = Create<Packet>(0);
	forwardPacket->AddHeader(cacheHeader);
	uint8_t cat = cacheHeader.GetDataCategory();
	std::vector<Friend> listFriendsCommonInterests;
	for (size_t i = 0; i < friends.size(); i++) {
		if (friends[i].interests.find(cat) != friends[i].interests.end()
				&& cacheHeader.GetFrom() != friends[i].ip
				&& GetDistance(friends[i].ip) <= maxDis) {
			listFriendsCommonInterests.push_back(friends[i]);
		}
	}
	std::sort(listFriendsCommonInterests.begin(),
			listFriendsCommonInterests.end(), CmpSF);
	for (uint8_t i = 0;
			i < m_dataHelper->maxRequests
					&& i < listFriendsCommonInterests.size(); i++) {
		statSxCnt++;
		m_socket->SendTo(forwardPacket, 0,
				InetSocketAddress(listFriendsCommonInterests[i].ip,
						m_destPort));
		NS_LOG_DEBUG(DebugPacket(forwardPacket,1,listFriendsCommonInterests[i].ip,"Fri"));
	}

}
void SCApp::SendToCommonFriends(Ptr<Packet> packet) {
	NS_LOG_DEBUG(nodeIDstr<<" SendToCommonFriends");
	ForwardToFriends(packet, INFHOPS);

}

void SCApp::SendRequest(DataItem dataItem) {
	NS_LOG_DEBUG(nodeIDstr<< " SendRequest");
	CacheHeader cacheHeader;
	cacheHeader.SetMessageType(CACHE_HEADER_QUERY);
	cacheHeader.SetDataId(dataItem.m_id);
	cacheHeader.SetDataCategor(dataItem.m_category);
	cacheHeader.SetSize(0);
	cacheHeader.SetTimetag(Simulator::Now());
	cacheHeader.SetFrom(m_localAddr);
	cacheHeader.SetTTL(128);

	BroadCastQuery(cacheHeader);
	Ptr<Packet> packet = Create<Packet>(0);
	packet->AddHeader(cacheHeader);

	if (ccstrategy == 1) {
		SendToHighestCentralityFriend(packet);
		ForwardToFriends(packet);
	}
	/*if (ccstrategy == 2) {
	 SendToHighestCentralityFriend(packet);
	 SendToCommonFriends(packet);
	 }*/
	if (ccstrategy == 4) {
		ForwardToRandomNodes(packet);
	}

}

void SCApp::Request() {
	NS_LOG_DEBUG(nodeIDstr<< " Request");
	DataItem randomDataItem;
	if (m_01Ran->GetValue() < interest_factor) {
		//request interested data
		randomDataItem = m_dataHelper->GetRandomDataItem();
		while (interests.find(randomDataItem.m_category) == interests.end()) {
			randomDataItem = m_dataHelper->GetRandomDataItem();
		}
	} else {
		//request random data
		randomDataItem = m_dataHelper->GetRandomDataItem();
	}
	m_logRequested.push_back(randomDataItem.m_id);
	statRequestsInited++;
	if (CheckRequestBeenResponed(randomDataItem.m_id)) {
		NS_LOG_DEBUG("|-----Local Hit");
		statLocalHit++;
	} else {
		m_historyQueryInited.insert(
				std::pair<uint32_t, Time>(randomDataItem.m_id,
						Simulator::Now()));
		/*
		 #ifdef mydebug

		 if (GetNodeID() == debug_node1) {
		 NS_LOG_UNCOND("D#"<<randomDataItem.m_id<<" @ "<<Simulator::Now().GetMilliSeconds());
		 }
		 #endif
		 */

		NS_LOG_DEBUG(nodeIDstr<< "Request D#"<<randomDataItem.m_id
				<<"I#"<<randomDataItem.m_category<<" @ "<<Simulator::Now().GetMilliSeconds());

		/*std::stringstream ss;
		 ss << nodeIDstr << " Request D#" << randomDataItem.m_id << " C#"
		 << randomDataItem.m_category << " @ "
		 << Simulator::Now().GetMilliSeconds()<<"ms";
		 DebugToFile(ss);*/
		SendRequest(randomDataItem);
		m_checkEvent = Simulator::Schedule(
				Seconds(this->m_dataHelper->secondRequestTimeout),
				&SCApp::DoCheckandRequest, this, randomDataItem.m_id);
	}

	m_sendEvent = Simulator::Schedule(Seconds(request_rate), &SCApp::Request,
			this);

}
//0 is recv 1 is send
std::string SCApp::DebugPacket(Ptr<Packet> packet, int method = 0,
		Ipv4Address to = "", std::string msg = "") {
	std::ostringstream oss;
	CacheHeader recvHeader;
	packet->PeekHeader(recvHeader);
	oss.str("");
	oss << nodeIDstr;
	switch (method) {
	case 0:
		oss << " ==>Recv ";
		break;
	case 1:
		oss << " Send==> ";
		break;
	default:
		break;
	}
	switch (recvHeader.GetMessageType()) {
	case CACHE_HEADER_QUERY:
		oss << "QUERY";
		if (recvHeader.IsBroadCast())
			oss << "_B";
		break;
	case CACHE_HEADER_AVI:
		oss << "AVI";
		break;
	case CACHE_HEADER_REQ:
		oss << "REQ";
		break;
	case CACHE_HEADER_RETD:
		oss << "RETD";
		break;
	default:

		break;
	}
	switch (method) {
	case 0:
		oss << " from " << recvHeader.GetFrom();
		break;
	case 1:
		oss << " to " << to << "[" << msg << "]";
		break;
	default:
		break;
	}
	oss << "\t TTL:" << recvHeader.GetTTL() << " D#" << recvHeader.GetDataId()
			<< " C#" << (int) recvHeader.GetDataCategory() << " @Time "
			<< Simulator::Now().GetSeconds() << "s";
	return oss.str();

}
void SCApp::WarmUp() {
//TODO careful
	int failcnt = 0;
	while (currentCSsize < m_cacheCap - m_dataHelper->maxDataItemSize
			&& failcnt < 60000) {
		DataItem randomDataItem;
		randomDataItem = m_dataHelper->GetRandomDataItem();
		while ((interests.find(randomDataItem.m_category) == interests.end()
				|| CheckRequestBeenResponed(randomDataItem.m_id))
				&& failcnt < 60000) {
			randomDataItem = m_dataHelper->GetRandomDataItem();
			failcnt++;

		}

		CacheEntry ce(randomDataItem.m_id, randomDataItem.m_category,
				randomDataItem.m_size, Simulator::Now(), 0, m_localAddr);
		currentCSsize += randomDataItem.m_size;
		m_cacheStore.push_back(ce);

	}NS_LOG_DEBUG(nodeIDstr<<" warm up "<<m_cacheStore.size()<<" items "<<currentCSsize/1024<<"/"<<m_cacheCap/1024<<"KB");

}
void SCApp::BroadCastQuery(CacheHeader cacheHeader) {
	NS_LOG_DEBUG(nodeIDstr<<" BroadCastQuery");
	cacheHeader.SetBroadCast(true);
	Ptr<Packet> packet = Create<Packet>(0);
	packet->AddHeader(cacheHeader);
//either 10.0.0.255
	m_socket->SendTo(packet, 0,
			InetSocketAddress("255.255.255.255", m_destPort));
	NS_LOG_DEBUG(DebugPacket(packet,1,"255.255.255.255","B"));
}
void SCApp::HandleQuery(Ptr<Packet> packet) {
	CacheHeader recvHeader;
	packet->PeekHeader(recvHeader);
	NS_LOG_DEBUG(nodeIDstr<<" HandleQuery D#"<<recvHeader.GetDataId());

	if (isDataSource) {
		Ipv4Address dest = recvHeader.GetFrom();

		/*if (dest == Ipv4Address(debug_nodeip)) {
		 NS_LOG_DEBUG("N#S rec req D#"<<recvHeader.GetDataId()<<"from"<<recvHeader.GetFrom());
		 }*/
		//FIXME ignoring TTL
		this->statRequestReplyed++;
		uint32_t dataSize = this->m_dataHelper->GetDataSize(
				recvHeader.GetDataId());

		CacheHeader repHeader;
		repHeader.SetMessageType(CACHE_HEADER_AVI);
		repHeader.SetFrom(m_localAddr);
		repHeader.SetTimetag(Simulator::Now());
		repHeader.SetSize(dataSize);
		repHeader.SetDataId(recvHeader.GetDataId());
		repHeader.SetDataCategor(
				m_dataHelper->GetDataCat(recvHeader.GetDataId()));

		Ptr<Packet> avipacket = Create<Packet>(0);
		avipacket->AddHeader(repHeader);

		statSxCnt++;
		statAviSxCnt++;
		m_socket->SendTo(avipacket, 0, InetSocketAddress(dest, m_destPort));
		NS_LOG_DEBUG(DebugPacket(avipacket,1,dest,"AS DS"));

	} else {
		bool handleBefore = HandledBefore(packet);

		if (recvHeader.GetTTL() > 128 - m_dataHelper->maxHops
				&& !handleBefore) {

			uint32_t cachesize = CanServer(packet);
			if (cachesize > 0) {
				Ipv4Address dest = recvHeader.GetFrom();
				//response request
				recvHeader.SetMessageType(CACHE_HEADER_AVI);
				recvHeader.SetFrom(m_localAddr);
				recvHeader.SetTimetag(Simulator::Now());
				Ptr<Packet> avipacket = Create<Packet>(0);
				avipacket->AddHeader(recvHeader);

				statSxCnt++;
				statAviSxCnt++;
				m_socket->SendTo(avipacket, 0,
						InetSocketAddress(dest, m_destPort));

				NS_LOG_DEBUG(DebugPacket(avipacket,1,dest,"AS Peer"));
			} else {
//				DataItem dataItem(recvHeader.GetDataId(),
//						recvHeader.GetDataCategory(), recvHeader.GetSize());
				CacheEntry ce(recvHeader.GetDataId(),
						recvHeader.GetDataCategory(), recvHeader.GetSize(),
						recvHeader.GetTimetag(), 0.0, recvHeader.GetFrom());

				//log forwarded request
				m_historyPassingRequests.push_back(ce);

				if (ccstrategy == 0) {
					statRequestForwared++;
					MinusTTL(packet);
					CacheHeader tmpHeader;
					packet->PeekHeader(tmpHeader);
					BroadCastQuery(tmpHeader);
				} else if (ccstrategy == 2) {
					CacheHeader header;
					packet->PeekHeader(header);
					if (CheckAvailble(header.GetFrom())) {
						statRequestForwared++;
						MinusTTL(packet);
						CacheHeader tmpHeader;
						packet->PeekHeader(tmpHeader);
						BroadCastQuery(tmpHeader);

					}

				} else if (ccstrategy == 1) {
					if (m_01Ran->GetValue() < social_forward_factor
							&& !recvHeader.IsBroadCast()) {
						statRequestForwared++;
						MinusTTL(packet);
						if (m_01Ran->GetValue() < social_forward_factor) {
							ForwardToFriends(packet);
						}
					}
				} else if (ccstrategy == 3 && !recvHeader.IsBroadCast()) {
					statRequestForwared++;
					MinusTTL(packet);
					ForwardToNeighbors(packet);
				} else if (ccstrategy == 4 && !recvHeader.IsBroadCast()) {
					statRequestForwared++;
					MinusTTL(packet);
					ForwardToRandomNodes(packet);
				}

			} //else do nothing
		}
	}
}
void SCApp::HandleAVI(Ptr<Packet> packet) {
	NS_LOG_DEBUG(nodeIDstr<< " HandleAVI");
	CacheHeader recvHeader;
	packet->PeekHeader(recvHeader);
	if (!CheckRequestBeenResponed(recvHeader.GetDataId())
			&& m_historyReqSent.find(recvHeader.GetDataId())
					== m_historyReqSent.end()) {
		CacheHeader reqHeader;
		reqHeader.SetMessageType(CACHE_HEADER_REQ);
		reqHeader.SetDataId(recvHeader.GetDataId());
		reqHeader.SetDataCategor(recvHeader.GetDataCategory());
		reqHeader.SetFrom(m_localAddr);
		reqHeader.SetTimetag(Simulator::Now());
		reqHeader.SetSize(recvHeader.GetSize());
		m_historyReqSent.insert(
				std::make_pair(recvHeader.GetDataId(), Simulator::Now()));
		Ptr<Packet> reqPacket = Create<Packet>(0);
		reqPacket->AddHeader(reqHeader);

		statSxCnt++;
		statReqSxCnt++;
		m_socket->SendTo(reqPacket, 0,
				InetSocketAddress(recvHeader.GetFrom(), m_destPort));

		NS_LOG_DEBUG(DebugPacket(reqPacket,1,recvHeader.GetFrom(),""));

	}
}

void SCApp::HandleREQ(Ptr<Packet> packet) {
	NS_LOG_DEBUG(nodeIDstr<<" HandleREQ");
	CacheHeader recvHeader;
	packet->PeekHeader(recvHeader);
	uint32_t dataSize;
	if (isDataSource) {
		//FIXME ignoring TTL
		this->statRequestReplyed++;
		dataSize = this->m_dataHelper->GetDataSize(recvHeader.GetDataId());
	} else {
		dataSize = CanServer(packet);
		if (dataSize == 0) {
			return;
		}
	}

	CacheHeader repHeader;
	repHeader.SetMessageType(CACHE_HEADER_RETD);
	repHeader.SetFrom(m_localAddr);
	repHeader.SetTimetag(Simulator::Now());
	repHeader.SetSize(dataSize);
	repHeader.SetDataId(recvHeader.GetDataId());
	repHeader.SetDataCategor(m_dataHelper->GetDataCat(recvHeader.GetDataId()));

	Ptr<Packet> retd = Create<Packet>(dataSize);
	retd->AddHeader(repHeader);

	statRetdSxCnt++;
	statSxCnt++;
	m_socket->SendTo(retd, 0,
			InetSocketAddress(recvHeader.GetFrom(), m_destPort));
	NS_LOG_DEBUG(DebugPacket(retd,1,recvHeader.GetFrom(),""));
}
void SCApp::HandleRETD(Ptr<Packet> packet) {
	NS_LOG_DEBUG(nodeIDstr<<" HandleRETD");
	CacheHeader recvHeader;
	packet->PeekHeader(recvHeader);
	if (!CheckRequestBeenResponed(recvHeader.GetDataId())) {

		Time diff;
		DataIDTimeMap::iterator it2;

		//cleanup query log and do stat
		it2 = m_historyQueryInited.find(recvHeader.GetDataId());
		if (it2 != m_historyQueryInited.end()) {
			diff = Simulator::Now() - (*it2).second;
//			NS_LOG_INFO("Now"<<Simulator::Now().GetMilliSeconds()<<"ms"<<
//					" inited:"<<(*it2).second.GetMilliSeconds()<<"ms"<<
//					" diff"<<diff.GetMilliSeconds()<<" ms.");

			//FIXME there will be huge delay suddenly !!!!
			if (diff.ToDouble(Time::MS) > 1000) {
				statUnusualDelayCnt++;
			} else {

				if (recvHeader.GetFrom() == this->m_dataHelper->dataSource) {
					diff += extra_ms;
				}
				/*std::stringstream ss;
				 ss << nodeIDstr << "REVRETD #D " << recvHeader.GetDataId()
				 << " from" << recvHeader.GetFrom() << " @"
				 << Simulator::Now().GetMilliSeconds() << "ms."
				 << " diff=" << diff.GetMilliSeconds() << " ms."
				 << " inited=" << (*it2).second.GetMilliSeconds()
				 << "ms";
				 DebugToFile(ss);*/
				statDelay += diff.ToDouble(Time::MS);
				this->statRequestTime->Update(diff);
			}

			//}
			m_historyQueryInited.erase(it2);
		}

		//cleanup req log
		it2 = m_historyReqSent.find(recvHeader.GetDataId());
		if (it2 != m_historyReqSent.end()) {
			m_historyReqSent.erase(it2);
		}

		//clean up m_historyRequestsInited

		CacheEntry ce(recvHeader.GetDataId(), recvHeader.GetDataCategory(),
				recvHeader.GetSize(), Simulator::Now(), 0.0,
				recvHeader.GetFrom());

		//check if full
		if (CacheStoreWillFull(recvHeader.GetSize())) {
			DoReplacementLRU(recvHeader.GetSize());
		}

		//TODO: so easy here
		if (ccstrategy == 1 || ccstrategy == 2) {
			std::vector<Friend>::iterator it1;
			for (it1 = friends.begin(); it1 < friends.end(); it1++) {
				if ((*it1).ip == recvHeader.GetFrom()) {
					(*it1).socialFactor++;
					break;
				}
			}
		}

		//check if has old one , delete it
		CacheEntryVector::iterator it;
		for (it = m_cacheStore.begin(); it < m_cacheStore.end(); it++) {
			if ((*it).m_id == recvHeader.GetDataId()) {
				/*if (recvHeader.GetTimetag() > (*it).m_timestamp)*/{
					m_cacheStore.erase(it);
					break;
				}
			}

		}

		statRequestReplyed++;
		if (recvHeader.GetFrom() == this->m_dataHelper->GetDataSourceIP()) {
			statRbS++;
		} else {
			statRbP++;
		}

		currentCSsize += ce.m_size;
		m_cacheStore.push_back(ce);
	}NS_LOG_DEBUG("append cache table on "<<nodeIDstr<<" size="<<m_cacheStore.size() );
}

void SCApp::Receive(Ptr<Socket> socket) {
	NS_LOG_DEBUG(nodeIDstr<<" Receive");
	Ptr<Packet> packet;
	Address from;
	while ((packet = socket->RecvFrom(from))) {
//		if (InetSocketAddress::IsMatchingType(from)) {
//			NS_LOG_UNCOND ("N#"<<GetNode()->GetId()<<"Received " << packet->GetSize () << " bytes from " <<
//					InetSocketAddress::ConvertFrom (from).GetIpv4 () <<" @ "<<Simulator::Now().GetMilliSeconds()<<"ms");
//		}
		statRxCnt++;
		CacheHeader recvHeader;
		packet->PeekHeader(recvHeader);
		NS_LOG_DEBUG(DebugPacket(packet));
		//FIXME
		//NS_ASSERT(recvHeader.GetFrom()!=m_localAddr);
		if (recvHeader.GetMessageType() == CACHE_HEADER_QUERY) {
			if (recvHeader.GetFrom() == m_localAddr) {
				return;
			}
			HandleQuery(packet);

		} else if (recvHeader.GetMessageType() == CACHE_HEADER_AVI) {
			statAviRxCnt++;
			HandleAVI(packet);
		} else if (recvHeader.GetMessageType() == CACHE_HEADER_REQ) {
			statReqRxCnt++;
			HandleREQ(packet);
		} else if (recvHeader.GetMessageType() == CACHE_HEADER_RETD) {
			statRetdRxCnt++;
			HandleRETD(packet);
		}
	} //end receive

}

//if have same node id && data id
bool SCApp::HandledBefore(Ptr<Packet> packet) {
	NS_LOG_DEBUG(nodeIDstr<<" HandledBefore");

	CacheHeader recvHeader;
	packet->PeekHeader(recvHeader);
	CacheEntryVector::reverse_iterator it;
	for (it = m_historyPassingRequests.rbegin();
			it != m_historyPassingRequests.rend(); it++) {
		if (recvHeader.GetDataId() == (*it).m_id)
			return true;
	}
	return false;
}
void SCApp::DoCheckandRequest(uint32_t dataID) {
	NS_LOG_DEBUG(nodeIDstr<<" DoCheckandRequest D#"<<dataID);
	if (CheckRequestBeenResponed(dataID)) {
		return;
	} else {

		//request from data source
		CacheHeader cacheHeader;
		Ptr<Packet> packet = Create<Packet>(0);
		cacheHeader.SetDataId(dataID);
		cacheHeader.SetFrom(m_localAddr);
		cacheHeader.SetMessageType(CACHE_HEADER_QUERY);
		cacheHeader.SetSize(0);
		cacheHeader.SetTimetag(Simulator::Now());
		packet->AddHeader(cacheHeader);

		NS_LOG_INFO (nodeIDstr<< " Req D#"<<cacheHeader.GetDataId()
				<<" from source at "<< Simulator::Now ().GetMilliSeconds() << "ms." );
		statSxCnt++;
		m_socket->SendTo(packet, 0,
				InetSocketAddress(this->m_dataHelper->GetDataSourceIP(),
						m_destPort));
		NS_LOG_DEBUG(DebugPacket(packet,1,this->m_dataHelper->GetDataSourceIP(),"After TO"));
	}
}
//check request log and cache table
bool SCApp::CheckRequestBeenResponed(uint32_t dataID) {

	/*if (m_historyRequestsInited.find(dataID) != m_historyRequestsInited.end())
	 return false;*/
	bool res = false;
	CacheEntryVector::reverse_iterator rit;
	if (m_cacheStore.size() > 0) {
		for (rit = m_cacheStore.rbegin(); rit != m_cacheStore.rend(); rit++) {
			if (dataID == (*rit).m_id) {
				res = true;
				break;
			}
		}
	}
	/*if (GetNodeID() == debug_node1) {
	 NS_LOG_DEBUG (nodeIDstr<<"D#"<<dataID<<"CheckRequestBeenResponed="<<res);
	 }*/
//NS_LOG_DEBUG(nodeIDstr<<" CheckRequestBeenResponed "<<dataID<<"="<<res);
	return res;

}
void SCApp::CleanUpTables() {
	DataIDTimeMap::iterator it;
	/*
	 if (!m_historyQueryInited.empty()) {
	 statFailed += m_historyQueryInited.size();
	 for (it = m_historyQueryInited.begin();
	 it != m_historyQueryInited.end(); it++) {
	 DoCheckandRequest((*it).first);
	 #ifdef mydebug
	 if (GetNodeID() == debug_node1) {
	 NS_LOG_UNCOND((*it).first<<"been rechecked");

	 }
	 #endif
	 }
	 }
	 */
	m_historyPassingRequests.clear();

	m_checkEvent = Simulator::Schedule(
			Seconds(m_dataHelper->intervalRefreshRequestList),
			&SCApp::CleanUpTables, this);
}
std::string SCApp::PrintSocialInfo() {

//std::sort(friends.begin(), friends.end(), (SCApp::CmpFriendbyDis));
	SortFriendsByDis();
	std::ostringstream oss;
	oss << "===============" << nodeIDstr << " " << GetPosition(this)
			<< m_localAddr << " @ Time " << Simulator::Now().GetSeconds()
			<< "s:============\n";
	InterestsSet::iterator it;
	Ptr<RoutingProtocol> routing = GetNode()->GetObject<RoutingProtocol>();
	std::vector<RoutingTableEntry> table = routing->GetRoutingTableEntries();
	std::vector<RoutingTableEntry>::iterator rtit;

	oss << "Interests(" << interests.size() << ")==";
	for (it = interests.begin(); it != interests.end(); it++) {
		oss << (int) (*it) << " ";
	}
	oss << "\nFriends Interests(" << friends_interests.size() << ")=="
			<< GetInterests(friends_interests);

	oss << "\nFriends: " << friends.size() << "\n";
	for (size_t i = 0; i < friends.size(); i++) {
		oss << friends[i].ip << "\tDis=";
		for (rtit = table.begin(); rtit != table.end(); rtit++) {
			if ((*rtit).destAddr == friends[i].ip) {
				oss << (*rtit).distance;
				break;
			}
		}
		oss << " C=" << (int) friends[i].centrality << " I("
				<< friends[i].interests.size() << ")="
				<< GetInterests(friends[i].interests) << "\n";
	}
	oss << "Routing Table: " << table.size()
			<< " entries\nIP              dis\tFri\n"
			<< "=====================================================\n";
	for (rtit = table.begin(); rtit != table.end(); rtit++) {

		if ((*rtit).distance < 4) {
			oss << (*rtit).destAddr << "\t" << (*rtit).distance;
			if (!CheckAvailble((*rtit).destAddr)) {
				oss << "\tT";
			}
			oss << "\n";
		}

	}
	std::cout << oss.str();
	std::cout.flush();
	return oss.str();

}
std::string SCApp::PrintDebugInfo() {
	std::stringstream ss;
	DataIDTimeMap::iterator it1;
	CacheEntryVector::reverse_iterator rit;
	CacheEntryVector::iterator it2;
	InterestsSet::iterator it;

	ss << "\n======" << this->nodeIDstr << "Debug info @"
			<< Simulator::Now().GetMilliSeconds() << "ms.======";

	ss << "\n=====Interests(" << interests.size() << ")=====\n";
	for (it = interests.begin(); it != interests.end(); it++) {
		ss << (int) (*it) << " ";
	}

	if (!m_historyQueryInited.empty()) {
		ss << "\n====Unfinished====\n";
		for (it1 = m_historyQueryInited.begin();
				it1 != m_historyQueryInited.end(); it1++) {
			ss << (*it1).first << ",";
		}
	}

	if (!m_logRequested.empty()) {
		ss << "\n====Request Inited(" << m_logRequested.size() << ")====";
		int cnt = 0;
		std::stringstream tmpss;
		for (uint32_t i = 0; i < m_logRequested.size(); i++) {
			if (interests.find(m_dataHelper->GetDataCat(m_logRequested[i]))
					!= interests.end()) {
				cnt++;
				tmpss << m_logRequested[i] << ",";
			}
		}
		ss << "\n  ==Interested(" << cnt << ")==\n";
		ss << tmpss.str();

		cnt = 0;
		tmpss.str("");
		for (uint32_t i = 0; i < m_logRequested.size(); i++) {
			if (interests.find(m_dataHelper->GetDataCat(m_logRequested[i]))
					== interests.end()) {
				cnt++;
				tmpss << m_logRequested[i] << ",";
			}
		}
		ss << "\n  ==Randoms(" << cnt << ")==\n";
		ss << tmpss.str();
	}

	if (!m_historyPassingRequests.empty()) {
		ss << "\n====Passing request====\nid    from (rev)\n";
		for (rit = m_historyPassingRequests.rbegin();
				rit != m_historyPassingRequests.rend(); rit++) {
			ss << "D#" << std::setw(4) << std::left << (*rit).m_id << " "
					<< (*rit).from << "\n";
		}
	}
	if (!m_cacheStore.empty()) {
		uint32_t cnt1 = 0;
		uint32_t cnt2 = 0;
		uint32_t cnt3 = 0;
		std::stringstream tmpss1;
		std::stringstream tmpss2;
		std::stringstream tmpss3;

		ss << "\n====Cache Store " << (double) currentCSsize / 1024 << "/"
				<< m_cacheCap / 1024
				<< "KB ====\nid cat size    from      time";
		for (it2 = m_cacheStore.begin(); it2 != m_cacheStore.end(); it2++) {

			if (interests.find((*it2).m_category) != interests.end()) {
				cnt1++;
				tmpss1 << std::setw(6) << (*it2).m_id << std::setw(3)
						<< (*it2).m_category << std::setw(6) << (*it2).m_size
						<< std::setw(12) << (*it2).from << " "
						<< (*it2).m_timestamp.GetMilliSeconds() << "\n";
			} else if (friends_interests.find((*it2).m_category)
					!= interests.end()) {
				cnt2++;
				tmpss2 << std::setw(6) << (*it2).m_id << std::setw(3)
						<< (*it2).m_category << std::setw(6) << (*it2).m_size
						<< std::setw(12) << (*it2).from << " "
						<< (*it2).m_timestamp.GetMilliSeconds() << "\n";

			} else {
				cnt3++;
				tmpss3 << std::setw(6) << (*it2).m_id << std::setw(3)
						<< (*it2).m_category << std::setw(6) << (*it2).m_size
						<< std::setw(12) << (*it2).from << " "
						<< (*it2).m_timestamp.GetMilliSeconds() << "\n";

			}
		}
		NS_ASSERT(cnt1+cnt2+cnt3 == m_cacheStore.size());
		ss << "\n  ==Personal Interest(" << cnt1 << ")==\n" << tmpss1.str();
		ss << "\n  ==Fri Interest(" << cnt2 << ")==\n" << tmpss2.str();
		ss << "\n  ==Random(" << cnt3 << ")==\n" << tmpss3.str();

	}
	ss << "\n==================================";
	return ss.str();
}
bool SCApp::CacheStoreWillFull(uint32_t dataSize) {
//will full if add dataSize bytes content
	return currentCSsize + dataSize > this->m_cacheCap;
}
void SCApp::DoReplacementLRU(uint32_t dataSize) {
// clean at least dataSize bytes store
	statReplCnt++;

//InterestsSet::iterator setit;
	NS_ASSERT(m_cacheStore.size()>0);
	if (ccstrategy == 1 || ccstrategy == 2)
		SortCacheEntries();
	while (currentCSsize + dataSize > this->m_cacheCap) {
		currentCSsize -= m_cacheStore[0].m_size;
		m_cacheStore.erase(m_cacheStore.begin());
	}
}

//TODO: validation . for now if time diff less than 5 mins than it's validate
uint32_t SCApp::CanServer(Ptr<Packet> pkt) {
	uint32_t retv = 0;
	CacheHeader recvHeader;
	pkt->PeekHeader(recvHeader);
	std::vector<CacheEntry>::iterator it;
	for (it = m_cacheStore.begin(); it < m_cacheStore.end(); it++) {
		if ((*it).m_id == recvHeader.GetDataId() && (*it).m_size > 0
				&& (Simulator::Now() - (*it).m_timestamp) < Seconds(5 * 60)) {
			retv = (*it).m_size;
			break;

		}
	}NS_LOG_DEBUG(nodeIDstr<<" CanServer"<<" D#"<<recvHeader.GetDataId()<<"="<<retv);
	return retv;
}

ApplicationContainer SCAppHelper::Install(NodeContainer c) const {
	ApplicationContainer apps;
	for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i) {
		apps.Add(InstallPriv(*i));
	}

	return apps;
}
ApplicationContainer SCAppHelper::Install(NodeContainer c,
		Ptr<SCAppHelper> datahelper) const {
	ApplicationContainer apps;
	for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i) {
		apps.Add(InstallPriv(*i, datahelper));
	}
	return apps;
}
std::string SCAppHelper::PrintGblStatSimp() {
	std::ostringstream oss;
	oss.str();
	oss << glbstatDelay / (gblstatInit - glbstatUnfinished - glbstatUnusual)
			<< "\t" << ((double) (glbstatRbP + glbstatRbL) / gblstatInit)
			<< "\t"
			<< ((double) (glbstatRbP + glbstatRbS + glbstatRbL) / gblstatInit);
	return oss.str();
}
std::string SCAppHelper::PrintGblStat() {
	std::ostringstream oss;
	oss.str();
	oss << node_stat;
	oss << "======Global_Statistics======="
			// << "\nNum_Nodes:\t" << glbstatNode
			<< "\nTotal_Inited:\t" << gblstatInit << "\nRep_by_peer:\t"
			<< glbstatRbP << "\nRep_by_datasrc:\t" << glbstatRbS
			<< "\nRep_by_local:\t" << glbstatRbL << "\nForwarded:\t"
			<< glbstatForwarded << "\nReplc_Cnt:\t" << glbstatRplace
			<< "\nUnfinished:\t" << glbstatUnfinished << "\nMsg_Sent:\t"
			<< glbstatSx << "\nMsg_Received:\t" << gblstatRx
			<< "\nAverage_Delay:\t"
			<< glbstatDelay / (gblstatInit - glbstatUnfinished - glbstatUnusual)
			<< " ms" << "\nHit_ratio:\t"
			<< ((double) 100 * (glbstatRbP + glbstatRbL) / gblstatInit) << "%"
			<< "\nSuccess_ratio:\t"
			<< ((double) 100 * (glbstatRbP + glbstatRbS + glbstatRbL)
					/ gblstatInit) << "%";
	return oss.str();
}
SCAppHelper::SCAppHelper(uint16_t numInterests, uint32_t numNodes) {
	m_factory.SetTypeId(SCApp::GetTypeId());
	this->tolalIntestsInNetwork = numInterests;
	this->totalNodes = numNodes;
	random = CreateObject<UniformRandomVariable>();
}

void SCAppHelper::SetAttribute(std::string name, const AttributeValue &value) {
	m_factory.Set(name, value);
}

Ptr<Application> SCAppHelper::InstallPriv(Ptr<Node> node) const {
	Ptr<Application> app = m_factory.Create<SCApp>();
	node->AddApplication(app);

	return app;
}
Ptr<Application> SCAppHelper::InstallPriv(Ptr<Node> node,
		Ptr<SCAppHelper> datahelper) const {
	Ptr<Application> app = m_factory.Create<SCApp>();
	Ptr<SCApp> scapp = app->GetObject<SCApp>();
	scapp->SetDataHelper(datahelper);
	node->AddApplication(app);
	return app;
}
void SCAppHelper::InitDataList() {
//TODO for now insert 1k data items
//	Ptr<NormalRandomVariable> dataran = CreateObject<NormalRandomVariable>();
//	dataran->SetAttribute("Mean", DoubleValue(maxDataItemSize/2));
//	dataran->SetAttribute("Variance", DoubleValue(2));
	for (uint32_t i = 0; i < this->totalDataItems; i++) {
		uint16_t dataCat = random->GetInteger(1, this->tolalIntestsInNetwork);
		uint32_t dataSize = random->GetInteger(40, maxDataItemSize);
		dataList.insert(
				DataListMap::value_type(i, DataItem(i, dataCat, dataSize)));
	}NS_LOG_UNCOND("Inited "<<totalDataItems<<" data items");
	std::ofstream ofs;
	ofs.open("datalist.csv");
	DataListMap::iterator it;
	ofs << "id\tcat\tsize" << std::endl;
	for (it = dataList.begin(); it != dataList.end(); it++) {
		ofs << (*it).first << "\t" << (*it).second.m_category << "\t"
				<< (*it).second.m_size << std::endl;
	}
	ofs.close();
}
void SCAppHelper::InitSocialNetwork_Simple(ApplicationContainer apps) {
	ApplicationContainer::Iterator i;
	for (i = apps.Begin(); i != apps.End(); ++i) {
		Ptr<SCApp> scapp = DynamicCast<SCApp>(*i);
		scapp->AddRandomFriends(apps.GetN());
		scapp->AddRandomInterests(apps.GetN(), this->tolalIntestsInNetwork);
	}

//update friend table on every node
	for (i = apps.Begin(); i != apps.End(); ++i) {

		Ptr<SCApp> scapp = DynamicCast<SCApp>(*i);

		for (size_t j = 0; j < scapp->friends.size(); j++) {

			//scapp, jth friend
			for (size_t m = 0; m < apps.GetN(); m++) {
				Ptr<SCApp> search = DynamicCast<SCApp>(apps.Get(m));
				Ptr<Ipv4> ipv4 = search->GetNode()->GetObject<Ipv4>();
				Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
				if (iaddr.GetLocal() == scapp->friends[j].ip) {
					//write interests and centrality
					scapp->friends[j].interests = search->interests;
					scapp->friends[j].centrality = search->friends.size();

				}
			}
		}
	}

}
//node # contains ZERO  node # not continuous
//edge no direction
void SCAppHelper::InitSocialNetwork_1(ApplicationContainer apps,
		std::string filePath) {

	std::fstream ifs;
	std::ostringstream oss;
	ifs.open(filePath.data());
	uint32_t a, b; //may == 0
	int cnt = 0;
	while (ifs.peek() != EOF) {

		ifs >> a >> b;
		NS_ASSERT( a>=0 && a<this->totalNodes && b>=0 && b<this->totalNodes);
		cnt++;

		Ptr<SCApp> appa = apps.Get(a)->GetObject<SCApp>();
		oss.str("");
		oss << "10.0." << b / 254 << "." << b % 254 + 1; // 1...254
		Ipv4Address friendipb(oss.str().data());
		Friend friendb(friendipb);
		appa->friends.push_back(friendb);

		Ptr<SCApp> appb = apps.Get(b)->GetObject<SCApp>();
		oss.str("");
		oss << "10.0." << a / 254 << "." << a % 254 + 1; // 1...254
		Ipv4Address friendipa(oss.str().data());
		Friend frienda(friendipa);
		appb->friends.push_back(frienda);

	}NS_LOG_INFO("added "<<cnt<<" edges");
	ifs.close();

	ApplicationContainer::Iterator i;
	for (i = apps.Begin(); i != apps.End(); ++i) {
		Ptr<SCApp> scapp = DynamicCast<SCApp>(*i);
		scapp->AddRandomInterests(apps.GetN(), this->tolalIntestsInNetwork);
	}
	for (i = apps.Begin(); i != apps.End() - 1; ++i) {

		Ptr<SCApp> scapp = DynamicCast<SCApp>(*i);

		for (size_t j = 0; j < scapp->friends.size(); j++) {

			//scapp, jth friend
			for (size_t m = 0; m < apps.GetN(); m++) {
				Ptr<SCApp> search = DynamicCast<SCApp>(apps.Get(m));
				Ptr<Ipv4> ipv4 = search->GetNode()->GetObject<Ipv4>();
				Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
				if (iaddr.GetLocal() == scapp->friends[j].ip) {
					//write interests and centrality
					scapp->friends[j].interests = search->interests;
					scapp->friends[j].centrality = search->friends.size();

				}
			}
		}
	}

}
//read edge file and circle file
void SCAppHelper::InitSocialNetwork_2(ApplicationContainer apps,
		std::string filePath) {

	int max_nodes = 789; // limit to 100
	std::fstream ifs;
	std::ostringstream oss;
	ApplicationContainer::Iterator iappit;
	std::string egPath = filePath;
	egPath.append(".edges");
	std::string cirPath = filePath;
	cirPath.append(".circles");
	ifs.open(egPath.data());

	NS_ASSERT(ifs.is_open());
	std::string a, b; //may == 0
	uint32_t node_cnt = 0;
	uint32_t edge_cnt = 0;
	uint32_t i;
	std::map<std::string, uint32_t> name_id_map;
	while (ifs.peek() != EOF) {

		ifs >> a >> b;
		if (atoi(a.c_str()) > max_nodes || atoi(b.c_str()) > max_nodes)
			continue;
		edge_cnt++;
		if (name_id_map.find(a) == name_id_map.end()) {
			name_id_map.insert(std::pair<std::string, uint32_t>(a, node_cnt++));
		}
		if (name_id_map.find(b) == name_id_map.end()) {
			name_id_map.insert(std::pair<std::string, uint32_t>(b, node_cnt++));
		}
		//NS_ASSERT(a>=0 && a<this->totalNodes && b>=0 && b<this->totalNodes);

		Ptr<SCApp> appa = apps.Get(name_id_map[a])->GetObject<SCApp>();
		oss.str("");
		oss << "10.0." << name_id_map[b] / 254 << "."
				<< name_id_map[b] % 254 + 1; // 1...254
		Ipv4Address friendipb(oss.str().data());
		Friend friendb(friendipb);
		if (appa->CheckAvailble(friendipb)) {
			appa->friends.push_back(friendb);
		}

		Ptr<SCApp> appb = apps.Get(name_id_map[b])->GetObject<SCApp>();
		oss.str("");
		oss << "10.0." << name_id_map[a] / 254 << "."
				<< name_id_map[a] % 254 + 1; // 1...254
		Ipv4Address friendipa(oss.str().data());
		Friend frienda(friendipa);
		if (appb->CheckAvailble(friendipa)) {

			appb->friends.push_back(frienda);
		}

	}

	NS_LOG_UNCOND("Read "<<node_cnt<<"=="<<name_id_map.size()<<" nodes with "<<edge_cnt <<" edges");
	ifs.close();

//write nodeid associated with ip
	std::ofstream ofs;
	ofs.open("./scratch/sc/686ip.out");
	std::map<std::string, uint32_t>::iterator name_id_it;
	for (name_id_it = name_id_map.begin(); name_id_it != name_id_map.end();
			name_id_it++) {
		oss.str("");
		oss << "10.0." << (*name_id_it).second / 254 << "."
				<< (*name_id_it).second % 254 + 1;
		ofs << (*name_id_it).first << "\t" << oss.str() << "\n";
	}
	ofs.close();

//init interests
	ifs.open(cirPath.data());
	NS_ASSERT(ifs.is_open());
	std::string line;
	std::string name;
	int linenum = -1;
	while (ifs.peek() != EOF) {
		std::getline(ifs, line);
		linenum++;
		//read a circle
		std::set<uint16_t> circle_common_interests;
		//divide interests into circles
		/*		std::cout << "\ncircle" << linenum << "interests:" << std::endl;
		 for (int in = this->tolalIntestsInNetwork / 14 * linenum;
		 in < this->tolalIntestsInNetwork / 14 * (linenum + 1); in++) {
		 circle_common_interests.insert((uint8_t) in);
		 std::cout << in << " ";
		 }*/
		circle_common_interests.insert((uint8_t) (linenum + 1));

		/*
		 uint32_t circle_common_interests_cnt = random->GetInteger(4, 6);

		 for (i = 0; i < circle_common_interests_cnt; i++) {
		 circle_common_interests.insert(
		 random->GetInteger(1, tolalIntestsInNetwork));

		 }*/
		std::istringstream iss(line);
		while (iss >> name) {
			if (name[0] == 'c')
				continue;
			if (atoi(name.c_str()) > max_nodes) {
				continue;
			}
			//a large id may cause a lower id node escaped
			else if (name_id_map.find(name) != name_id_map.end()) {
				//This assert will fail if limit node number
				//NS_ASSERT(name_id_map.find(name)!=name_id_map.end());

				uint32_t n_id = name_id_map[name];
				Ptr<SCApp> scapp = apps.Get(n_id)->GetObject<SCApp>();
				//add common interests
				scapp->interests.insert(circle_common_interests.begin(),
						circle_common_interests.end());
				//add personal interests
				//scapp->AddRandomInterests(apps.GetN(),
				//	this->tolalIntestsInNetwork);
			}
		}
	}
	ifs.close();

	for (iappit = apps.Begin(); iappit != apps.End() - 1; ++iappit) {
		Ptr<SCApp> scapp = DynamicCast<SCApp>(*iappit);
		for (size_t j = 0; j < scapp->friends.size(); j++) {
			//scapp, jth friend
			for (size_t m = 0; m < apps.GetN(); m++) {
				Ptr<SCApp> search = DynamicCast<SCApp>(apps.Get(m));
				Ptr<Ipv4> ipv4 = search->GetNode()->GetObject<Ipv4>();
				Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
				if (iaddr.GetLocal() == scapp->friends[j].ip) {
					//write interests and centrality
					scapp->friends[j].interests = search->interests;
					scapp->friends[j].centrality = search->friends.size();

				}
			}
		}
	}
	for (i = 0; i < apps.GetN(); i++) {

		//apps.Get(i)->GetObject<SCApp>()->PrintSocialInfo();
	}

}

