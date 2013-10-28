/*
 * SCApp.h
 *
 *  Created on: May 14, 2013
 *      Author: roy
 */

#ifndef SCAPP_H_
#define SCAPP_H_
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/application.h"
#include "ns3/mobility-module.h"
#include "ns3/stats-module.h"
#include "ns3/olsr-routing-protocol.h"
//#include "ns3/aodv-module.h"
#include "ns3/udp-socket-factory.h"
#include <fstream>

using namespace ns3;
extern std::string str_extradelay;

enum CACHE_HEADER_TYPE {
	CACHE_HEADER_QUERY = 0,
	CACHE_HEADER_AVI = 1,
	CACHE_HEADER_REQ = 2,
	CACHE_HEADER_RETD = 3

};
enum CC_STRATEGY {
	ST_PHY = 0,  ST_PHYSO = 1, ST_PYHSOFO = 2 , ST_PHYFWD=3,

};

class Friend {
public:
	Friend() {

	}
	Friend(const Friend& af) {
		ip = af.ip;
		centrality = af.centrality;
		socialFactor = af.socialFactor;
		interests = af.interests;
	}
	Friend(Ipv4Address _ip) :
			ip(_ip), centrality(0), socialFactor(0) {

	}
	Ipv4Address ip;
	uint16_t centrality; //num friends of this friend;
	double socialFactor;
	std::set<uint16_t> interests;
};
class RequestLogEntry {
public:
	uint32_t m_dataID;
	Time m_time;
};
class CacheEntry {
public:
	CacheEntry(uint32_t id, uint16_t category, uint32_t size, Time timestamp,
			double popularity, Ipv4Address ip) :
			m_id(id), m_category(category), m_size(size), m_timestamp(
					timestamp), m_popularity(popularity), from(ip) {

	}
	;
	CacheEntry() {

	}
	;

	uint32_t m_id;
	uint16_t m_category;
	uint32_t m_size; //in bytes
	Time m_timestamp;
	double m_popularity;
	Ipv4Address from;
};
class DataItem;
class SCAppHelper;
class CacheHeader;
typedef std::vector<CacheEntry> CacheEntryVector;
typedef std::map<uint32_t, Time> DataIDTimeMap;
typedef std::map<uint32_t, DataItem> DataListMap;
typedef std::vector<Friend> FriendsVector;
typedef std::set<uint16_t> InterestsSet;

class SCApp: public Application {
public:

protected:
	virtual void DoDispose(void);

private:
	virtual void StartApplication(void);
	virtual void StopApplication(void);

public:
	static TypeId GetTypeId(void);
	SCApp();
	SCApp(Ptr<SCAppHelper> datahelper);
	virtual ~SCApp();

	void Request();
	void Receive(Ptr<Socket> socket);

	bool CheckAvailble(Ipv4Address ip);
	bool HandledBefore(Ptr<Packet> packet); //true if have not handled before
	bool CacheStoreWillFull(uint32_t dataSize); //will full if add dataSize bytes content
	bool CheckRequestBeenResponed(uint32_t dataID);
	bool CmpCacheEntry(CacheEntry& ce1, CacheEntry& ce2);
	float GetMetric(CacheEntry &ce); //for replacement

	void WarmUp();
	void HandleQuery(Ptr<Packet> packet);
	void HandleAVI(Ptr<Packet> packet);
	void HandleREQ(Ptr<Packet> packet);
	void HandleRETD(Ptr<Packet> packet);
	void SendToHighestCentralityFriend(Ptr<Packet> packet);
	void SendToCommonFriends(Ptr<Packet> packet);
	void ForwardToFriends(Ptr<Packet> packet, uint32_t maxHop);
	void ForwardToNeighbors(Ptr<Packet> packet );
	void ForwardToRandomNodes(Ptr<Packet> packet );
	void SendRequest(DataItem dataItem);
	void DoCheckandRequest(uint32_t dataID);
	void BroadCastQuery(CacheHeader);
	void SortCacheEntries();
	void SortFriendsByDis();
	std::string PrintDebugInfo();
	std::string PrintSocialInfo();
	void AddRandomInterests(uint32_t numTotalNodes, uint16_t numInterests);
	void AddRandomFriends(uint32_t numTotalNodes);
	void CleanUpTables();
	void DoReplacementLRU(uint32_t dataSize); // clean at least dataSize bytes store
	uint32_t CanServer(Ptr<Packet> pkt); //return size of data item
	uint32_t GetDistance(Ipv4Address friendip);
	std::string DebugPacket(Ptr<Packet> packet, int method, Ipv4Address,std::string);
	void SetDataHelper(Ptr<SCAppHelper> datahelper) {
		m_dataHelper = datahelper;
	}
	void SetAsDataSource(bool val) {
		isDataSource = val;
	}
	uint32_t GetNodeID() {
		return GetNode()->GetId();
	}
	void RealStartUp();
	void EvaluationStartUp();
	void RecvEcho(Ptr<Socket> socket);
	void SendEcho(uint32_t nodeid);
	void EvaluationStop();
	void SetDelayTracker(Ptr<TimeMinMaxAvgTotalCalculator> statRequestTime) {
		this->statRequestTime = statRequestTime;
	}

	uint32_t m_cacheCap;
	uint32_t m_destPort;
	Ptr<UniformRandomVariable> m_uniformRan;
	Ptr<UniformRandomVariable> m_01Ran;
	Ptr<Socket> m_socket;
	Ptr<Socket> recv_socket;
	EventId m_sendEvent;
	EventId m_checkEvent;
	EventId m_cleanupEvent;

	//TracedCallback<Ptr<const Packet> > m_txTrace;
	//uint32_t m_count;
	//Ptr<CounterCalculator<> > m_calc;
	Ptr<SCAppHelper> m_dataHelper;
	Ipv4Address m_localAddr;
	uint32_t ccstrategy;
	double social_forward_factor;
	double request_rate;
	double interest_factor;
	bool isDataSource;
	bool dowarmup;
	int statRequestsInited;
	int statLocalHit;
	int statFailed;
	int statRequestReplyed; // requests that have been answered
	int statRbS;
	int statRbP;
	int statRequestForwared;
	int statUnFinished;
	int statRxCnt;
	int statSxCnt;
	int statReplCnt;
	double statDelay; //in ms

	int statReqSxCnt;
	int statReqRxCnt;
	int statUnusualDelayCnt;
	int statAviSxCnt;
	int statAviRxCnt;

	int statRetdSxCnt;
	int statRetdRxCnt;

	std::vector<double> statEcho;

	uint32_t currentCSsize;
	std::string nodeIDstr;
	Ptr<TimeMinMaxAvgTotalCalculator> statRequestTime;
	CacheEntryVector m_cacheStore; //for local cache storage
	CacheEntryVector m_historyPassingRequests; //for ignoring bypass same request. app level, not network level
	DataIDTimeMap m_historyQueryInited; //data id, time request sent
	DataIDTimeMap m_historyReqSent; //unfinished request
	std::vector<uint32_t> m_logRequested; //all requested (id)
	FriendsVector friends;
	InterestsSet interests;
	InterestsSet friends_interests;
};

class CacheHeader: public Header {
public:

	CacheHeader() {
		m_type = 0;
		m_ttl = 0;
		m_isbroadcast = 0;
		m_dataID = 0;
		m_size = 0;
		m_dataCat = 0;
	}
	CacheHeader(const CacheHeader & header) {
		m_type = header.m_type;
		m_ttl = header.m_ttl;
		m_isbroadcast = header.m_isbroadcast;
		m_dataID = header.m_dataID;
		m_size = header.m_size;
		m_dataCat = header.m_dataCat;
		m_from = header.m_from;
		m_timetag = header.m_timetag;
	}
	virtual ~CacheHeader() {

	}

	static TypeId GetTypeId(void) {
		static TypeId tid =
				TypeId("ns3::CacheHeader").SetParent<Header>().AddConstructor<
						CacheHeader>();
		return tid;
	}
	virtual TypeId GetInstanceTypeId(void) const {
		return GetTypeId();
	}
	virtual void Print(std::ostream &os) const {
		os << "[CacheHeader]";
		os << " Type:" << GetMessageType() << " TTL:" << GetTTL();
		os << " From=" << m_from << " ID=" << m_dataID << " Cat="
				<< (int) m_dataCat << " Size=" << m_size << " Time="
				<< m_timetag.GetMilliSeconds() << "ms" << std::endl;
	}
	virtual void Serialize(Buffer::Iterator start) const {
		Buffer::Iterator i = start;
		i.WriteU8(m_type);
		i.WriteU8(m_ttl);
		i.WriteU8(m_isbroadcast);
		i.WriteU8(m_dataCat);
		i.WriteHtonU32(m_from.Get());
		i.WriteU32(m_dataID);
		i.WriteU32(m_size);
		int64_t t = m_timetag.GetNanoSeconds();
		i.Write((const uint8_t *) &t, 8);
	}
	virtual uint32_t Deserialize(Buffer::Iterator start) {
		Buffer::Iterator i = start;
		m_type = i.ReadU8();
		m_ttl = i.ReadU8();
		m_isbroadcast = i.ReadU8();
		m_dataCat = i.ReadU8();
		m_from.Set(i.ReadNtohU32());
		m_dataID = i.ReadU32();
		m_size = i.ReadU32();
		int64_t t;
		i.Read((uint8_t *) &t, 8);
		m_timetag = NanoSeconds(t);
		// we return the number of bytes effectively read.
		return GetSerializedSize();
	}
	virtual uint32_t GetSerializedSize(void) const {
		return 192;
	}

	uint32_t GetDataId() const {
		return m_dataID;
	}

	void SetDataId(uint32_t dataId) {
		m_dataID = dataId;
	}

	Ipv4Address GetFrom() const {
		return m_from;
	}

	void SetFrom(Ipv4Address from) {
		m_from = from;
	}

	CACHE_HEADER_TYPE GetMessageType() const {
		return CACHE_HEADER_TYPE(m_type);
	}
	void SetBroadCast(bool b) {
		m_isbroadcast = b ? 1 : 0;
	}
	bool IsBroadCast() {
		return m_isbroadcast == 1 ? true : false;
	}
	void SetMessageType(CACHE_HEADER_TYPE messageType) {
		m_type = messageType;
	}
	uint32_t GetTTL() const {
		return m_ttl;
	}
	void SetTTL(uint8_t ttl) {

		m_ttl = ttl;
	}
	void TTLPlus(uint8_t a = 1) {
		m_ttl += a;
	}

	void TTLMinus(uint8_t a = 1) {
		m_ttl -= a;
	}

	uint32_t GetSize() const {
		return m_size;
	}

	void SetSize(uint32_t size) {
		m_size = size;
	}

	Time GetTimetag() const {
		return m_timetag;
	}
	uint8_t GetDataCategory() {
		return m_dataCat;
	}
	void SetDataCategor(uint8_t category) {
		m_dataCat = category;
	}

	void SetTimetag(Time timetag) {
		m_timetag = timetag;
	}
private:
	uint8_t m_dataCat;
	uint8_t m_ttl;
	uint8_t m_type;
	uint8_t m_isbroadcast;
	Ipv4Address m_from;
	uint32_t m_dataID;
	uint32_t m_size;
	Time m_timetag;
};
class DataItem {
public:
	DataItem() {

	}
	DataItem(uint32_t id, uint16_t category, uint32_t size) :
			m_id(id), m_category(category), m_size(size) {

	}
	uint32_t m_id;
	uint16_t m_category;
	uint32_t m_size;

};
class SCAppHelper: public Object {
public:

	SCAppHelper(uint16_t numInterests, uint32_t numNodes);
	void InitSocialNetwork_Simple(ApplicationContainer apps);
	void InitSocialNetwork_1(ApplicationContainer apps, std::string filePath);
	void InitSocialNetwork_2(ApplicationContainer apps, std::string filePath);
	void InitDataList();
	void SetAttribute(std::string name, const AttributeValue &value);

	Ptr<Application> InstallPriv(Ptr<Node> node) const;
	Ptr<Application> InstallPriv(Ptr<Node> node,
			Ptr<SCAppHelper> datahelper) const;

	ApplicationContainer Install(NodeContainer c) const;
	ApplicationContainer Install(NodeContainer c,
			Ptr<SCAppHelper> datahelper) const;

	void SetIntervalRefreshRequestList(int intervalRefreshRequestList) {
		this->intervalRefreshRequestList = intervalRefreshRequestList;
	}

	void SetMaxHops(int maxHops) {
		this->maxHops = maxHops;
	}
	void SetDataSource(Ptr<Node> node) {
		node->GetApplication(0)->GetObject<SCApp>()->SetAsDataSource(true);
		dataSource = node->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
		NS_LOG_UNCOND("Set Data Source:"<<node->GetId());

	}

	void SetMaxRequests(int maxRequests) {
		this->maxRequests = maxRequests;
	}

	DataItem GetRandomDataItem() {

		return dataList[random->GetValue(0, this->totalDataItems)];
	}
	std::string PrintGblStat();
	std::string PrintGblStatSimp();
	//for data source
	uint32_t GetDataSize(uint32_t dataID) {
		return dataList[dataID].m_size;
	}
	uint16_t GetDataCat(uint32_t dataID) {
		return dataList[dataID].m_category;
	}

	double GetSecondRequestTimeout() const {
		return secondRequestTimeout;
	}

	void SetSecondRequestTimeout(double secondRequestTimeout) {
		this->secondRequestTimeout = secondRequestTimeout;
	}

	//todo stat work
	std::set<uint32_t> GetDataIDsByCat(uint16_t cat){
		std::set<uint32_t> rval;
		DataListMap::iterator it;
		for(it=dataList.begin();it!=dataList.end();it++){
			if((*it).second.m_category==cat){
				rval.insert((*it).first);
			}
		}
		return rval;
	}
	uint32_t GetTotalDataItems() const {
		return totalDataItems;
	}

	void SetTotalDataItems(uint32_t totalDataItems) {
		this->totalDataItems = totalDataItems;
	}

	uint32_t GetMaxDataSize() const {
		return maxDataItemSize;
	}

	void SetMaxDataSize(uint32_t maxDataSize) {
		this->maxDataItemSize = maxDataSize;
	}

	Ipv4Address GetDataSourceIP() {
		return dataSource;
	}

	//configs:
	uint32_t intervalRefreshRequestList; //in seconds
	uint32_t maxRequests;
	uint32_t maxHops;
	uint32_t totalDataItems;
	uint32_t maxDataItemSize;
	uint32_t totalNodes;
	uint16_t tolalIntestsInNetwork;
	double secondRequestTimeout;
	//stats
	int gblstatRx;
	int glbstatSx;
	int glbstatNode;
	int gblstatInit;
	int glbstatRbP;
	int glbstatRbS;
	int glbstatRbL;
	int glbstatForwarded;
	int glbstatUnfinished;
	int glbstatUnusual;
	int glbstatRplace;
	double glbstatDelay;
	std::string node_stat;
	DataListMap dataList;
	Ipv4Address dataSource;
	Ptr<UniformRandomVariable> random;
	ObjectFactory m_factory;
	//	Ptr<MinMaxAvgTotalCalculator<int> > statRequestCountOnNodes;

};
//------------------------------------------------------

#endif /* SCAPP_H_ */
