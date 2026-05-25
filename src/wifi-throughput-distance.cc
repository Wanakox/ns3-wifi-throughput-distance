#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace ns3;

/*
 * WiFi Throughput vs Distance Simulation
 *
 * Scenario:
 * - One WiFi Access Point (AP)
 * - One WiFi Station (STA)
 * - UDP traffic from STA to AP
 * - The distance between AP and STA is varied between runs
 *
 * KPIs:
 * - Throughput (Mbps)
 * - Average delay (ms)
 * - Packet loss (%)
 *
 * Example:
 * ./ns3 run "scratch/wifi-throughput-distance --distance=20 --csvFile=/home/wan/Escritorio/WN/ns3-wifi-throughput-distance/results/results.csv"
 *
 * Note: SINR is discussed theoretically in the report, but this implementation
 * focuses on application/network-level KPIs: throughput, delay, and packet loss.
 */

NS_LOG_COMPONENT_DEFINE("WifiThroughputDistance");

static bool
FileIsEmpty(const std::string& filename)
{
    std::ifstream file(filename);
    return !file.good() || file.peek() == std::ifstream::traits_type::eof();
}

int
main(int argc, char* argv[])
{
    // ------------------------------------------------------------
    // Simulation parameters
    // ------------------------------------------------------------
    double distance = 10.0;            // Distance between AP and STA in meters
    double simulationTime = 10.0;      // Total traffic generation time in seconds
    double appStartTime = 1.0;         // Time when UDP client starts
    uint32_t packetSize = 1024;        // UDP packet size in bytes
    double packetInterval = 0.0002;    // Packet interval in seconds
    std::string csvFile = "results.csv";

    CommandLine cmd(__FILE__);
    cmd.AddValue("distance", "Distance between AP and STA in meters", distance);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("packetSize", "UDP packet size in bytes", packetSize);
    cmd.AddValue("packetInterval", "Interval between UDP packets in seconds", packetInterval);
    cmd.AddValue("csvFile", "CSV output file", csvFile);
    cmd.Parse(argc, argv);

    std::cout << "Running WiFi simulation with distance = "
              << distance << " m" << std::endl;

    // ------------------------------------------------------------
    // Create nodes
    // ------------------------------------------------------------
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // ------------------------------------------------------------
    // Configure WiFi channel and PHY
    // ------------------------------------------------------------
    YansWifiChannelHelper channel;

    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                           "Exponent",
                           DoubleValue(2.0),
                           "ReferenceDistance",
                           DoubleValue(1.0),
                           "ReferenceLoss",
                           DoubleValue(46.6777));

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Fixed transmission power for reproducibility.
    phy.Set("TxPowerStart", DoubleValue(20.0));
    phy.Set("TxPowerEnd", DoubleValue(20.0));

    // ------------------------------------------------------------
    // Configure WiFi standard and rate adaptation
    // ------------------------------------------------------------
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    /*
     * MinstrelWifiManager is a realistic rate adaptation algorithm.
     * It allows the WiFi rate to change depending on channel conditions.
     */
    wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-throughput-distance");

    // Station MAC
    mac.SetType("ns3::StaWifiMac",
                "Ssid",
                SsidValue(ssid),
                "ActiveProbing",
                BooleanValue(false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // Access Point MAC
    mac.SetType("ns3::ApWifiMac",
                "Ssid",
                SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // ------------------------------------------------------------
    // Configure node positions
    // ------------------------------------------------------------
    MobilityHelper mobility;

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    // AP position: origin
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));

    // STA position: variable distance from AP
    positionAlloc->Add(Vector(distance, 0.0, 0.0));

    NodeContainer allWifiNodes;
    allWifiNodes.Add(wifiApNode);
    allWifiNodes.Add(wifiStaNode);

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allWifiNodes);

    // ------------------------------------------------------------
    // Install Internet stack
    // ------------------------------------------------------------
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    Ipv4InterfaceContainer staInterface;
    staInterface = address.Assign(staDevice);

    // ------------------------------------------------------------
    // Configure UDP traffic from STA to AP
    // ------------------------------------------------------------
    uint16_t port = 9;

    // UDP server on AP
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime + 1.0));

    // UDP client on STA
    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(100000000));
    client.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(appStartTime));
    clientApp.Stop(Seconds(simulationTime));

    // ------------------------------------------------------------
    // FlowMonitor for KPI measurement
    // ------------------------------------------------------------
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1.0));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double throughputMbps = 0.0;
    double avgDelayMs = 0.0;
    double packetLossPercent = 0.0;

    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    uint64_t totalRxBytes = 0;
    double totalDelaySeconds = 0.0;

    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(flow.first);

        /*
         * We only consider the UDP flow from the station to the access point.
         */
        if (tuple.destinationPort == port)
        {
            totalTxPackets += flow.second.txPackets;
            totalRxPackets += flow.second.rxPackets;
            totalRxBytes += flow.second.rxBytes;
            totalDelaySeconds += flow.second.delaySum.GetSeconds();
        }
    }

    double measurementTime = simulationTime - appStartTime;

    if (measurementTime > 0.0)
    {
        throughputMbps = (totalRxBytes * 8.0) / (measurementTime * 1000000.0);
    }

    if (totalRxPackets > 0)
    {
        avgDelayMs = (totalDelaySeconds / totalRxPackets) * 1000.0;
    }

    if (totalTxPackets > 0)
    {
        packetLossPercent =
            ((double)(totalTxPackets - totalRxPackets) / (double)totalTxPackets) * 100.0;
    }

    // ------------------------------------------------------------
    // Print results
    // ------------------------------------------------------------
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Distance: " << distance << " m" << std::endl;
    std::cout << "Throughput: " << throughputMbps << " Mbps" << std::endl;
    std::cout << "Average delay: " << avgDelayMs << " ms" << std::endl;
    std::cout << "Packet loss: " << packetLossPercent << " %" << std::endl;
    std::cout << "TX packets: " << totalTxPackets << std::endl;
    std::cout << "RX packets: " << totalRxPackets << std::endl;

    // ------------------------------------------------------------
    // Save results to CSV
    // ------------------------------------------------------------
    bool writeHeader = FileIsEmpty(csvFile);

    std::ofstream outFile;
    outFile.open(csvFile, std::ios_base::app);

    if (!outFile.is_open())
    {
        std::cerr << "Error: could not open CSV file: " << csvFile << std::endl;
        Simulator::Destroy();
        return 1;
    }

    if (writeHeader)
    {
        outFile << "distance_m,throughput_mbps,delay_ms,packet_loss_percent,tx_packets,rx_packets\n";
    }

    outFile << std::fixed << std::setprecision(4)
            << distance << ","
            << throughputMbps << ","
            << avgDelayMs << ","
            << packetLossPercent << ","
            << totalTxPackets << ","
            << totalRxPackets << "\n";

    outFile.close();

    Simulator::Destroy();
    return 0;
}
