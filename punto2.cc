#include <iostream>
#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Crear nodos: 1 switch y 4 nodos para cada sistema, y 1 switch central
    NodeContainer system1Switch, system2Switch, centralSwitch;
    system1Switch.Create(1);  // Switch del sistema 1
    system2Switch.Create(1);  // Switch del sistema 2
    centralSwitch.Create(1);  // Switch central

    NodeContainer system1Nodes, system2Nodes;
    system1Nodes.Create(4);  // 1 Servidor y 3 Clientes del sistema 1
    system2Nodes.Create(4);  // 1 Servidor y 3 Clientes del sistema 2

    // Instalar la pila de Internet en todos los nodos
    InternetStackHelper internet;
    internet.Install(system1Nodes);
    internet.Install(system2Nodes);
    internet.Install(system1Switch);  // Aseguramos que el switch tenga la pila IPv4
    internet.Install(system2Switch);
    internet.Install(centralSwitch);

    // Crear enlaces punto a punto entre nodos y switches
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> system1Devices(3), system2Devices(3);

    // Enlace servidor-switch para sistema 1
    system1Devices[0] = pointToPoint.Install(system1Nodes.Get(0), system1Switch.Get(0));
    // Enlaces cliente-switch para sistema 1
    for (uint32_t i = 1; i <= 3; ++i) {
        system1Devices[i - 1] = pointToPoint.Install(system1Nodes.Get(i), system1Switch.Get(0));
    }

    // Repetir para el sistema 2
    system2Devices[0] = pointToPoint.Install(system2Nodes.Get(0), system2Switch.Get(0));
    for (uint32_t i = 1; i <= 3; ++i) {
        system2Devices[i - 1] = pointToPoint.Install(system2Nodes.Get(i), system2Switch.Get(0));
    }

    // Crear enlace CSMA entre los switches y el switch central
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("1Gbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(500)));

    NetDeviceContainer switchLinks;
    switchLinks.Add(csma.Install(NodeContainer(system1Switch.Get(0), centralSwitch.Get(0))));
    switchLinks.Add(csma.Install(NodeContainer(system2Switch.Get(0), centralSwitch.Get(0))));

    // Asignar direcciones IP
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> system1Interfaces(3), system2Interfaces(3);

    for (uint32_t i = 0; i < 3; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        system1Interfaces[i] = ipv4.Assign(system1Devices[i]);

        subnet.str("");  // Limpiar el stream para el sistema 2
        subnet << "10.2." << i + 1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        system2Interfaces[i] = ipv4.Assign(system2Devices[i]);
    }

    // Configurar servidores TCP en los nodos servidores
    uint16_t port = 50000;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps;

    serverApps.Add(sinkHelper.Install(system1Nodes.Get(0)));  // Servidor sistema 1
    serverApps.Add(sinkHelper.Install(system2Nodes.Get(0)));  // Servidor sistema 2
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Configurar clientes TCP en los nodos clientes
    for (uint32_t i = 1; i <= 3; ++i) {
        BulkSendHelper clientHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(system1Interfaces[0].GetAddress(0), port));
        ApplicationContainer clientApp = clientHelper.Install(system1Nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        clientHelper.SetAttribute("Remote", 
                                  AddressValue(InetSocketAddress(system2Interfaces[0].GetAddress(0), port)));
        clientApp = clientHelper.Install(system2Nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Habilitar trazas ASCII y PCAP
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("second.tr"));
    csma.EnablePcapAll("second");

    // Monitor de flujo
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Iniciar la simulación
    Simulator::Stop(Seconds(12.0));
    Simulator::Run();

    // Generar estadísticas en XML
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("second.xml", true, true);

    Simulator::Destroy();
    return 0;
}
