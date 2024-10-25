/*
    Topologia estrella
          n2
           |
           |
     n1---n0---n3
           |
           |
           n4
*/

#include <iostream>
#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Configuración para usar TCP Reno como algoritmo de control de congestión.
    //Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpReno"));

    // Crear nodos: 1 servidor y 4 clientes.
    NodeContainer serverNode;
    NodeContainer clientNodes;
    serverNode.Create(1);  // Nodo servidor
    clientNodes.Create(4); // Cuatro nodos clientes

    // Instalar la pila de Internet en todos los nodos
    InternetStackHelper link;
    link.Install(serverNode);
    link.Install(clientNodes);

    // Crear enlaces punto a punto entre el servidor y cada cliente.
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers(4);
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i) {
        deviceContainers[i] = pointToPoint.Install(serverNode.Get(0), clientNodes.Get(i));
    }

    // Asignar direcciones IP
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> interfaces(4);
    for (uint32_t i = 0; i < deviceContainers.size(); ++i) {
        std::ostringstream sub;
        sub << "10.1." << i + 1 << ".0";
        ipv4.SetBase(sub.str().c_str(), "255.255.255.0");
        interfaces[i] = ipv4.Assign(deviceContainers[i]);
    }

    // Configurar el servidor TCP en el nodo servidor.
    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = sinkHelper.Install(serverNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Configurar clientes TCP en cada nodo cliente.
    for (uint32_t i = 0; i < clientNodes.GetN(); ++i) {
        BulkSendHelper clientHelper("ns3::TcpSocketFactory", 
                                    InetSocketAddress(interfaces[i].GetAddress(0), port));
        clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Enviar datos ilimitados.
        ApplicationContainer clientApp = clientHelper.Install(clientNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Habilitar trazas en formato ASCII y PCAP.
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("star-topology.tr"));
    pointToPoint.EnablePcapAll("star-topology");

    // Configuración del monitor de flujo para generar un archivo XML con estadísticas.
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Iniciar la simulación.
    Simulator::Stop(Seconds(12.0));
    Simulator::Run();

    // Verificar paquetes perdidos y serializar los resultados en un archivo XML.
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("traffic-results.xml", true, true);

    Simulator::Destroy();
    return 0;
}
