#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"

using namespace ns3;

void ConfigurarAplicaciones(NodeContainer& clientes, NodeContainer& servidores, Ipv4InterfaceContainer& interfaces, uint16_t puerto) {
    for (uint32_t i = 0; i < clientes.GetN(); ++i) {
        // Configurar servidor en cada nodo del contenedor "servidores"
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), puerto + i));
        ApplicationContainer servidorApp = sink.Install(servidores.Get(i));
        servidorApp.Start(Seconds(0.5));
        servidorApp.Stop(Seconds(10.0));

        // Configurar cliente en cada nodo del contenedor "clientes"
        BulkSendHelper cliente("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(i), puerto + i));
        cliente.SetAttribute("MaxBytes", UintegerValue(1024 * 1024));  // 1MB de datos
        ApplicationContainer clienteApp = cliente.Install(clientes.Get(i));
        clienteApp.Start(Seconds(1.0));
        clienteApp.Stop(Seconds(9.5));
    }
}

int main(int argc, char* argv[]) {
    Time::SetResolution(Time::NS);  // Establecer la resolución del tiempo en nanosegundos

    // Crear los nodos finales (6 dispositivos)
    NodeContainer nodos;
    nodos.Create(6);

    // Crear 3 switches
    NodeContainer switches;
    switches.Create(3);

    // Crear canal CSMA para conectar nodos y switches
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("1Gbps")));  // Velocidad de 1 Gbps
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(5000)));  // Retardo de 5000 ns

    // Conectar los nodos a sus respectivos switches (2 nodos por switch)
    NetDeviceContainer dispositivosSwitch1, dispositivosSwitch2, dispositivosSwitch3;

    for (uint32_t i = 0; i < 2; ++i) {
        // Nodos 0 y 1 -> Switch 1
        NodeContainer grupo1(nodos.Get(i), switches.Get(0));
        dispositivosSwitch1.Add(csma.Install(grupo1));

        // Nodos 2 y 3 -> Switch 2
        NodeContainer grupo2(nodos.Get(i + 2), switches.Get(1));
        dispositivosSwitch2.Add(csma.Install(grupo2));

        // Nodos 4 y 5 -> Switch 3
        NodeContainer grupo3(nodos.Get(i + 4), switches.Get(2));
        dispositivosSwitch3.Add(csma.Install(grupo3));
    }

    // Interconectar los switches entre sí
    NetDeviceContainer interSwitch;
    interSwitch.Add(csma.Install(NodeContainer(switches.Get(0), switches.Get(1))));
    interSwitch.Add(csma.Install(NodeContainer(switches.Get(1), switches.Get(2))));
    interSwitch.Add(csma.Install(NodeContainer(switches.Get(0), switches.Get(2))));

    // Instalar la pila de protocolos de Internet en los nodos y switches
    InternetStackHelper internet;
    internet.Install(nodos);
    internet.Install(switches);

    // Asignar direcciones IP a los dispositivos conectados
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesSwitch1 = ipv4.Assign(dispositivosSwitch1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesSwitch2 = ipv4.Assign(dispositivosSwitch2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesSwitch3 = ipv4.Assign(dispositivosSwitch3);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    ipv4.Assign(interSwitch);  // Direcciones IP para enlaces entre switches

    // Llenar las tablas de enrutamiento globalmente
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configurar aplicaciones para generar tráfico entre nodos
    uint16_t puerto = 50000;  // Puerto base para las conexiones
    NodeContainer clientes(nodos.Get(0), nodos.Get(2), nodos.Get(4));
    NodeContainer servidores(nodos.Get(1), nodos.Get(3), nodos.Get(5));
    ConfigurarAplicaciones(clientes, servidores, interfacesSwitch1, puerto);

    // Habilitar capturas PCAP para analizar el tráfico de red
    csma.EnablePcap("nodos", nodos, true);
    csma.EnablePcap("switches", switches, true);

    // Establecer el tiempo de parada de la simulación
    Simulator::Stop(Seconds(10.0));

    // Iniciar la simulación
    Simulator::Run();

    // Limpiar recursos una vez terminada la simulación
    Simulator::Destroy();

    return 0;
}
