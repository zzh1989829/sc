//testu1
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/aodv-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include "sc-app.h"
//#include "ns3/config-store-module.h"
//#include "ns3/gtk-config-store.h"
//#define gtk_config

NS_LOG_COMPONENT_DEFINE("SC");
uint32_t ccstrategy = ST_PHY;

double SocialFwdFactor = 0.5; //
double InterestFactor = 1; //probability of request data in personal interest set
uint32_t layout = 1; // 1=grid 2=random
uint32_t TotalNodes = 99 + 1; //168
int MaxDataSize = 3 * 1024;
int TotalDataItems = 1000;
int CacheCapacity; //bytes
double CacheScale = 5;
int NumInterests = 16;
int MaxHops = 2;
int NumRunID;
int MaxRequests = 5;
int RequestRate = 10;
//int MinDataSize = 300;
int trialRun = 0;
bool WarmUp = 1;
double transRange = 150; // 500 m  for grid layout
double StartupDelay = 50; // for building routing table in secs
double RunTime = 1000; //1000 Simulation Time in seconds
double SecondsTimeout = 0.040; //node request timeout 40ms
//==================================================================
time_t start_time, end_time;
using namespace ns3;
int GetRunID() {

	std::ifstream ifs;
	ifs.open("runid");
	int runid;
	ifs >> runid;
	ifs.close();
	runid++;
	std::ofstream ofs;
	ofs.open("runid");
	ofs << runid;
	ofs.close();
	return runid - 1;
}
void PrintProcess() {
	double TimePiece = RunTime / 100;
	NS_LOG_UNCOND("@@@@@@@@@@@@@@@@@@@@@@@@@@   "<<Simulator::Now().GetSeconds()<<"/"<<RunTime<<"s, "
			<<100*Simulator::Now().GetSeconds()/RunTime<<"%");
	Simulator::Schedule(Seconds(TimePiece), &PrintProcess);
}
void LogFileAndConsole(std::string log, std::string suffix = "") {
	NS_LOG_UNCOND(log);
	std::ofstream ofs;
	std::stringstream ssfile;
	ssfile << "sclog" << suffix << ".txt";
	ofs.open(ssfile.str().c_str(), std::ofstream::out | std::ofstream::app);
	ofs << log << "\n";
	ofs.close();
}
std::string GetSimpConfigString() {
	std::ostringstream oss;
	oss.str();
	oss << "=size=" << MaxDataSize / 1024 << " cap="
			<< (double) CacheCapacity / 1024 << " intfct=" << InterestFactor
			<< " rr=" << RequestRate << " st=" << ccstrategy << " RUNID=" << NumRunID;
	return oss.str();

}
std::string GetConfigString() {
	int runSeconds = difftime(end_time, start_time);
	std::ostringstream oss;
	oss.str();
	oss << "===========Setup============" << "\nCC Strategy:\t" << ccstrategy
			<< "\nDataSize(KB):\t" << MaxDataSize / 1024 << "\nCacheCap(KB):\t"
			<< (double) CacheCapacity / 1024 << "\nWarmup:\t" << WarmUp
			<< "\nFWD Factor:\t" << SocialFwdFactor << "\nIntrst Factor:\t"
			<< InterestFactor << "\nMax Hops:\t" << MaxHops
			<< "\nMax Requests:\t" << MaxRequests << "\nRequest Rate:\t"
			<< RequestRate << "\nRunTime(sec):\t" << RunTime
			<< "\nDataSourceX:\t" << str_extradelay << "\nData Items:\t"
			<< TotalDataItems << "\nNumber Nodes:\t" << TotalNodes
			<< "\nLayout:   \t" << (layout == 1 ? "grid" : "random")
			<< "\nInterests:\t" << NumInterests
			<< "\n=============================";
	oss << "\n@Run time@  " << runSeconds / 60 << "m" << runSeconds % 60
			<< "s\tRun ID:" << NumRunID << "\n";
	oss << ctime(&start_time);
	oss << ctime(&end_time);
	oss << "============================================================\n\n";
	return oss.str();

}

int main(int argc, char *argv[]) {
#ifdef gtk_config
	GtkConfigStore config;
	config.ConfigureAttributes();
	config.ConfigureDefaults();
#endif
	SeedManager::SetSeed(41);
	time(&start_time);
	time(&end_time);
	//Packet::EnablePrinting();

	LogComponentEnable("SC", LOG_LEVEL_ALL);
	//LogComponentEnable("SCApp", LOG_LEVEL_ALL);

	std::string phyMode("DsssRate11Mbps"); //
	bool verbose = false;
	bool tracing = false;
	NumRunID = GetRunID();
	std::string configFilePath("./scratch/sc/686");
	std::stringstream ss;

	CommandLine cmd;
	cmd.AddValue("st", "CC strategy", ccstrategy);
	cmd.AddValue("time", "run time", RunTime);
	cmd.AddValue("size", "max data size(byte)", MaxDataSize);
	cmd.AddValue("rr", "request rate", RequestRate);
	cmd.AddValue("if", "interest factor", InterestFactor);
	cmd.AddValue("test", "trial run", trialRun);
	cmd.AddValue("cs", "cache scale", CacheScale);
	//cmd.AddValue("d", "data items", TotalDataItems);
	//cmd.AddValue("i", "Interests", NumInterests);
	//cmd.AddValue("n", "number of nodes", TotalNodes);

	/*cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
	 cmd.AddValue("verbose", "turn on all WifiNetDevice log components",
	 verbose);
	 cmd.AddValue("tracing", "turn on ascii and pcap trac", tracing);*/

	cmd.Parse(argc, argv);

	CacheCapacity = MaxDataSize * TotalDataItems * 0.05 / 4 / 5 * CacheScale; //bytes

	if (trialRun) {
		RunTime = 150;
		TotalDataItems = 100;
		NumInterests = 16;
		TotalNodes = 9 + 1;
		StartupDelay = 50;
		MaxDataSize = 3 * 1024;
		CacheCapacity = 30 * 1024;
	}
	ss << "sc#" << TotalNodes;
	std::string experiment(ss.str());
	ss.str("");
	ss << "#I" << NumInterests << "#D" << TotalDataItems << "T" << StartupDelay
			<< "/" << RunTime << "M" << MaxHops;
	std::string strategy(ss.str());
	ss.str("");
	ss << NumRunID;
	std::string runID = ss.str();
	std::cout << GetConfigString();
	//else {
//		cacheCapacity = MaxDataSize * TotalDataItems * 0.05 / 4;
//		TotalDataItems = 2000;
//		NumInterests = 64;
//		numNodes = 168 + 1;
//		startUpDelay = 50;
//	}
//=================================================================

	// Convert to time object
	std::ostringstream oss;
	NodeContainer nodes;
	nodes.Create(TotalNodes);

	/*// disable fragmentation for frames below 2200 bytes
	 Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold",
	 StringValue("2200"));
	 // turn off RTS/CTS for frames below 2200 bytes
	 Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold",
	 StringValue("2200"));*/
	// Fix non-unicast data rate to be the same as that of unicast
	Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
			StringValue(phyMode));
	// The below set of helpers will help us to put together the wifi NICs we want
	WifiHelper wifi;
	wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

	/*wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
	 StringValue(phyMode), "ControlMode", StringValue(phyMode));*/
	if (verbose) {
		wifi.EnableLogComponents(); // Turn on all Wifi logging
	}

	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
	// set it to zero; otherwise, gain will be added
	//wifiPhy.Set("RxGain", DoubleValue(-10));
	// ns-3 supports RadioTap and Prism tracing extensions for 802.11b
	//wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

	/*wifiPhy.Set("TxPowerStart", DoubleValue(64.0206));
	 wifiPhy.Set("TxPowerEnd", DoubleValue(64.0206));
	 wifiPhy.Set("TxPowerLevels", UintegerValue(10));
	 wifiPhy.Set("TxGain", DoubleValue(1));
	 wifiPhy.Set("RxGain", DoubleValue(1));
	 wifiPhy.Set("CcaMode1Threshold", DoubleValue(-90));
	 wifiPhy.Set("RxNoiseFigure", DoubleValue(0));
	 wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-100.0));*/

	YansWifiChannelHelper wifiChannel;
	wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

	wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange",
			DoubleValue(transRange));
	wifiPhy.SetChannel(wifiChannel.Create());

	// Add a non-QoS upper mac, and disable rate control
	NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
	wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
			StringValue(phyMode), "ControlMode", StringValue(phyMode));

	// Set it to adhoc mode
	wifiMac.SetType("ns3::AdhocWifiMac");
	NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

	MobilityHelper mobility;
	ObjectFactory pos;
	double maxX = 1500; //= floor(sqrt(numNodes * 6400 * 3 / 6) * 3);
	double maxY = 1500; //= floor(sqrt(numNodes * 6400 * 3 / 6) * 2);
	//NS_LOG_INFO("Area");

	switch (layout) {
	case 1:
		//grid
		mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX",
				DoubleValue(0.0), "MinY", DoubleValue(0.0), "DeltaX",
				DoubleValue(120), "DeltaY", DoubleValue(120), "GridWidth",
				UintegerValue(10), "LayoutType", StringValue("RowFirst"));
		break;
	case 2:
		//random distributed in a rectangle
		//RngSeedManager::SetSeed(3);  //or it will always be the same

		pos.SetTypeId("ns3::RandomRectanglePositionAllocator");
		oss.clear();
		oss << "ns3::UniformRandomVariable[Min=0.0|Max=" << maxX << "]";
		pos.Set("X", StringValue(oss.str()));
		oss.clear();
		oss << "ns3::UniformRandomVariable[Min=0.0|Max=" << maxY << "]";
		pos.Set("Y", StringValue(oss.str()));
		NS_LOG_INFO("Area = [" << maxX <<" * "<<maxY << "]");
		mobility.SetPositionAllocator(
				pos.Create()->GetObject<PositionAllocator>());

		break;
	default:
		break;
	}

	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mobility.Install(nodes);

	//set data source location
	if (layout == 2) {
		Ptr<MobilityModel> dsMobilityModel;
		dsMobilityModel = nodes.Get(TotalNodes - 1)->GetObject<MobilityModel>();
		dsMobilityModel->SetPosition(Vector(100.0, 100.0, 0.0));
		NS_LOG_UNCOND("Data Source Location: ("<<dsMobilityModel->GetPosition().x<<","<<dsMobilityModel->GetPosition().y<<")");
	}
	OlsrHelper olsr;
	AodvHelper aodv;
	Ipv4StaticRoutingHelper staticRouting;

	Ipv4ListRoutingHelper list;
	list.Add(staticRouting, 0);
	// Enable OLSR
	list.Add(olsr, 10);
	//list.Add(aodv,10);

	InternetStackHelper internet;
	internet.SetRoutingHelper(list); // has effect on the next Install ()
	internet.Install(nodes);

	Ipv4AddressHelper ipv4;
	NS_LOG_INFO ("Assign IP Addresses.");
	ipv4.SetBase("10.0.0.0", "255.255.255.0");
	Ipv4InterfaceContainer i = ipv4.Assign(devices);

	TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

	/*
	 Time interPacketInterval = Seconds(interval);
	 Ptr<Socket> recvSink = Socket::CreateSocket(c.Get(sinkNode), tid);
	 InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
	 recvSink->Bind(local);
	 recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));

	 Ptr<Socket> source = Socket::CreateSocket(c.Get(sourceNode), tid);
	 InetSocketAddress remote = InetSocketAddress(i.GetAddress(sinkNode, 0), 80);
	 source->Connect(remote);
	 */
	if (tracing == true) {
		AsciiTraceHelper ascii;
		wifiPhy.EnableAsciiAll(
				ascii.CreateFileStream("wifi-simple-adhoc-grid.tr"));
		wifiPhy.EnablePcap("sc", devices);
		// Trace routing tables
		Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(
				"wifi-simple-adhoc-grid.routes", std::ios::out);
		olsr.PrintRoutingTableAllEvery(Seconds(2), routingStream);

		// To do-- enable an IP-level trace that shows forwarding events only
	}

	Ptr<SCAppHelper> apphelper = CreateObject<SCAppHelper>(NumInterests,
			TotalNodes);
	apphelper->SetIntervalRefreshRequestList(60 * 1);
	apphelper->SetMaxHops(MaxHops);
	apphelper->SetMaxRequests(MaxRequests);
	apphelper->SetMaxDataSize(MaxDataSize);
	apphelper->SetTotalDataItems(TotalDataItems);
	apphelper->SetSecondRequestTimeout(SecondsTimeout);

	//set app attr
	apphelper->SetAttribute("WarmUp", BooleanValue(WarmUp));
	apphelper->SetAttribute("ST", UintegerValue(ccstrategy));
	apphelper->SetAttribute("Capacity", UintegerValue(CacheCapacity));
	apphelper->SetAttribute("SocialForwardFactor",
			DoubleValue(SocialFwdFactor));
	apphelper->SetAttribute("InterestFacotr", DoubleValue(InterestFactor));
	apphelper->SetAttribute("RequestRate", DoubleValue(RequestRate));
	ApplicationContainer apps = apphelper->Install(nodes, apphelper);
	//TODO for now the last one is source
	apphelper->SetDataSource(nodes.Get(TotalNodes - 1));

	if (TotalNodes < 80) {
		apphelper->InitSocialNetwork_Simple(apps);
	} else {
		apphelper->InitSocialNetwork_2(apps, configFilePath);
	}

	apphelper->InitDataList();

	apps.Start(Seconds(StartupDelay));

	DataCollector data;
	data.DescribeRun(experiment, strategy, configFilePath, runID);
	Ptr<TimeMinMaxAvgTotalCalculator> delayStat = CreateObject<
			TimeMinMaxAvgTotalCalculator>();
	delayStat->SetKey("delay");
	delayStat->SetContext(".");
	for (uint32_t i = 0; i < apps.GetN(); i++) {
		apps.Get(i)->GetObject<SCApp>()->SetDelayTracker(delayStat);

	}
	//data.AddDataCalculator(delayStat);

	Ptr<PacketCounterCalculator> totalTx =
			CreateObject<PacketCounterCalculator>();
	totalTx->SetKey("wifi-tx-frames");
	totalTx->SetContext("node");
	Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
			MakeCallback(&PacketCounterCalculator::PacketUpdate, totalTx));
	data.AddDataCalculator(totalTx);

	Ptr<PacketCounterCalculator> totalRx =
			CreateObject<PacketCounterCalculator>();
	totalRx->SetKey("wifi-rx-frames");
	totalRx->SetContext("node");
	Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
			MakeCallback(&PacketCounterCalculator::PacketUpdate, totalRx));
	data.AddDataCalculator(totalRx);

	Simulator::Stop(Seconds(RunTime));

	/* Ptr<PacketCounterCalculator> appTx =
	 CreateObject<PacketCounterCalculator>();
	 appTx->SetKey("tx-packets");
	 appTx->SetContext("node[0]");*/

	//Config::Connect("/NodeList/0/ApplicationList/*/$SCApp/Tx",
	//		MakeCallback(&PacketCounterCalculator::PacketUpdate, appTx));
	//data.AddDataCalculator(appTx);
	//GetConfigString();
	PrintProcess();
	Simulator::Run();
	Ptr<DataOutputInterface> output = 0;
#ifdef STATS_HAS_SQLITE3
	NS_LOG_INFO ("Creating sqlite formatted data output.");
	output = CreateObject<SqliteDataOutput>();
#endif

	// Finally, have that writer interrogate the DataCollector and save
	// the results.
	if (output != 0)
		output->Output(data);

	Simulator::Destroy();
	time(&end_time);

	LogFileAndConsole(apphelper->PrintGblStat());
	LogFileAndConsole(GetConfigString());
	LogFileAndConsole(GetSimpConfigString(), "_r");
	LogFileAndConsole(apphelper->PrintGblStatSimp(), "_r");
	return 0;
}

