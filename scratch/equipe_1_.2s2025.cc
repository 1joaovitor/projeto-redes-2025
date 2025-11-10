/*
Node: “computador” da simulação, onde você instala pilha de rede e aplicações.
Application: gera/consome tráfego nos nós (p.ex., clientes e servidores UDP).
Channel: meio de comunicação (fio, rádio).
NetDevice: interface de rede de um nó (p2p, CSMA, Wi-Fi).
Helpers: construtores de alto nível que criam e conectam tudo (PointToPointHelper, 
CsmaHelper, WifiHelper, InternetStackHelper, Ipv4AddressHelper).

STA (de station) é o cliente Wi-Fi os nós c0, c1, …, cn que se conectam ao AP são 
justamente as STAs.

LAN Ethernet (CSMA) com s2, s1 e s0: uma mídia compartilhada (multi-point-to-point) todos os 
nós do barramento ficam na mesma sub-rede (estilo LAN). É uma forma natural de pôr s2, s1, s0 
juntos num único domínio 10.1.1.0. O nó s0 também atua como AP do Wi-Fi.
*/


// Bibliotecas para criacao de nos e manipulacao de containers
#include "ns3/core-module.h"
#include "ns3/network-module.h"
// Biblioteca para criacao de redes CSMA
#include "ns3/csma-module.h"
// Biblioteca para pilha de protocolos de internet
#include "ns3/internet-module.h"
// Biblioteca para aplicacoes de internet (ping, etc)
#include "ns3/internet-apps-module.h"
// Bibliotecas para criacao de redes Wi-Fi
#include "ns3/wifi-module.h"
// Biblioteca para mobilidade dos nos
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

// Métricas
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    std::string protocolo = "UDP"; // Protocolo TCP, UDP ou Mixed
    uint32_t nSta = 4; // Numero de clientes Wifi
    std::string dataRate = "100Mbps"; // Taxa de dados
    std::string delay = "2ms"; // Atraso do enlace

    CommandLine cmd; 
    cmd.AddValue("nSta", "Number of WiFi clients", nSta );
    cmd.AddValue("protocolo", "Protocolo a ser usado (TCP, UDP, Mixed)", protocolo);
    cmd.Parse(argc, argv);

    NodeContainer lan; lan.Create(3);       // 0:s2, 1:s1, 2:s0

    Ptr<Node> s2 = lan.Get(0); // servidor (LAN 10.1.1.0)
    Ptr<Node> s1 = lan.Get(1);
    Ptr<Node> s0 = lan.Get(2); // também é o AP

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue(dataRate)); // Configuracao da taxa de dados
    csma.SetChannelAttribute("Delay", StringValue(delay)); // Configuracao do atraso do enlace

    // reaproveitar s0 (da LAN) como AP
    NodeContainer wifiApNode; // AP Wifi
    wifiApNode.Add(s0);

    // Instalar CSMA na LAN s2–s1–s0
    NetDeviceContainer csmaDevices = csma.Install(lan);  // conecta s2,s1,s0 na mesma LAN

    csma.EnablePcap("lan", csmaDevices.Get(1), true);  // sniffer na LAN para debug

    NodeContainer wifiStaNodes; // Clients Wifi que se conectan a AP
    wifiStaNodes.Create(nSta);

    // Instalar pilha IP (TCP/UDP/IPv4) nos nós relevantes
    InternetStackHelper stack;
    stack.Install(lan);           // s2, s1, s0
    stack.Install(wifiStaNodes);  // clientes Wi-Fi

    // Endereçar a LAN (10.1.1.0/24)
    Ipv4AddressHelper addr;
    addr.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaIfaces = addr.Assign(csmaDevices);

    // MobilityHelper mobility;
    MobilityHelper mobility;

    // Posiciona as STAs em grade e o AP parado
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(5.0),
        "GridWidth", UintegerValue(2), "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes); // STAs
    mobility.Install(wifiApNode);   // AP (s0)

    // Montar o Wi-Fi (AP + STAs)
    // Helpers de PHY e Canal (compartilham o mesmo meio rádio)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper     phy;
    phy.SetErrorRateModel("ns3::NistErrorRateModel");
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("Equipe1");
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
        "DataMode", StringValue("OfdmRate54Mbps"),
        "ControlMode", StringValue("OfdmRate6Mbps"));

    // STAs (clientes)
    mac.SetType("ns3::StaWifiMac",
            "Ssid", SsidValue(ssid),
            "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevs = wifi.Install(phy, mac, wifiStaNodes);

    // Zniffer no Wi-Fi para debug 
    phy.EnablePcap("wifi-clients", staDevs);

    // AP (s0)
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevs  = wifi.Install(phy, mac, wifiApNode);

    // Endereçar o Wi-Fi (192.168.0.0/24)
    addr.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer staIfaces = addr.Assign(staDevs);
    Ipv4InterfaceContainer apIfaces  = addr.Assign(apDevs);

    // Rotas entre as sub-redes (s0 roteando LAN↔Wi-Fi)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4Address s2Addr = csmaIfaces.GetAddress(0); // Endereco IP do servidor s2
    std::cout << "Server s2 IP Address: " << s2Addr << std::endl;
    
    /*
     
    SEÇÃO DE APLICAÇÃO (TCP/UDP/Mixed)
    
    */ 

    // Definição das portas de cada protocolo
    uint16_t portTcp = 9;   // padrão para o "PacketSink" (TCP)
    uint16_t portUdp = 10; // Echo (UDP)

    // Containers para guardar as aplicações (para poder iniciá-las/pará-las)
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    if (protocolo == "TCP")
    {
        std::cout << "Iniciando cenário TCP" << std::endl;
        
        // Servidor (nó s2)
        // Instala o "PacketSink" (receptor TCP) no nó s2 (lan.Get(0))
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), portTcp));
        serverApps.Add(sinkHelper.Install(lan.Get(0)));

        // Clientes (nós c0...cn)
        // Instala o "BulkSend" (emissor TCP) em todos os clientes
        BulkSendHelper bulkHelper("ns3::TcpSocketFactory", InetSocketAddress(s2Addr, portTcp));
        
        // Enviar o máximo de dados possível (0 = ilimitado)
        bulkHelper.SetAttribute("MaxBytes", UintegerValue(0));

        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            ApplicationContainer app = bulkHelper.Install(wifiStaNodes.Get(i));
            app.Start(Seconds(2.0 + i * 0.1)); // Início escalonado dos clientes
            app.Stop(Seconds(10.0));
        }
    }
    else if (protocolo == "UDP")
    {
        std::cout << "Iniciando cenário UDP" << std::endl;

        // Servidor (no nó s2)
        // Instala o "UdpEchoServer" no s2
        UdpEchoServerHelper echoServerHelper(portUdp);
        serverApps.Add(echoServerHelper.Install(lan.Get(0)));

        // Clientes (nos nós c0...cn) ---
        // Instala o "UdpEchoClient" 
        UdpEchoClientHelper echoClientHelper(s2Addr, portUdp);
        
        // Configura o cliente UDP: 1 pacote a cada 0.01s (simulando tráfego)
        echoClientHelper.SetAttribute("MaxPackets", UintegerValue(1000)); // N° de pacotes
        echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(0.01))); // Intervalo
        echoClientHelper.SetAttribute("PacketSize", UintegerValue(1024)); // Tamanho

        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            ApplicationContainer app = echoClientHelper.Install(wifiStaNodes.Get(i));
            app.Start(Seconds(2.0 + i * 0.1)); // Início escalonado dos clientes
            app.Stop(Seconds(10.0));
        }
    }
    else if (protocolo == "Mixed")
    {
        std::cout << "Iniciando cenário Misto (50% TCP, 50% UDP)" << std::endl;

        // Servidor (s2)
        // O servidor precisa rodar os DOIS serviços ao mesmo tempo
        
        // Servidor TCP (PacketSink)
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), portTcp));
        serverApps.Add(sinkHelper.Install(lan.Get(0)));
        
        // Servidor UDP (UdpEcho)
        UdpEchoServerHelper echoServerHelper(portUdp);
        serverApps.Add(echoServerHelper.Install(lan.Get(0)));

        // Clientes (c0...cn)
        // Metade vai usar TCP, metade vai usar UDP
        
        // Clientes TCP (BulkSend)
        BulkSendHelper bulkHelper("ns3::TcpSocketFactory", InetSocketAddress(s2Addr, portTcp));
        bulkHelper.SetAttribute("MaxBytes", UintegerValue(0));

        // Clientes UDP (UdpEcho), mesmos valores do teste exclusivo UDP
        UdpEchoClientHelper echoClientHelper(s2Addr, portUdp);
        echoClientHelper.SetAttribute("MaxPackets", UintegerValue(1000));
        echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        echoClientHelper.SetAttribute("PacketSize", UintegerValue(1024));
        
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            ApplicationContainer app;
            if (i < wifiStaNodes.GetN() / 2) // Primeira metade
            {
                app = bulkHelper.Install(wifiStaNodes.Get(i));
            }
            else // Segunda metade
            {
                app = echoClientHelper.Install(wifiStaNodes.Get(i));
            }
            app.Start(Seconds(2.0 + i * 0.1)); 
            app.Stop(Seconds(10.0));
        }
    }

    // Define os horários de início e fim para as aplicações
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // FIM DAS APLICAÇÕES




    // Parte comentada para implementação do tcp, udp e mixer

    // Ping ICMPv4 a partir de uma STA
    // uint32_t staIndex = 0;
    // PingHelper ping(s2Addr);
    // ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    // auto pingApp = ping.Install(wifiStaNodes.Get(staIndex));
    // pingApp.Start(Seconds(2.0));
    // pingApp.Stop (Seconds(10.0));


    // Instalação do FlowMonitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll(); // Instala em todos os nós

    // Janela da simulação
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();



    // Resultados do FlowMonitor
  
    flowMonitor->CheckForLostPackets(); 

    // Estatísticas de todos os fluxos monitorados
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    
    double totalDelay = 0;       // Atraso total
    double totalRxBytes = 0;     // Total de bytes recebidos
    uint32_t totalTxPackets = 0; // Total de pacotes enviados
    uint32_t totalRxPackets = 0; // Total de pacotes recebidos
    double totalDuration = 0;    // Duração total 

    // Itera por cada fluxo (ex: c0->s2, c1->s2, etc.)
    for (auto const& [flowId, flowStats] : stats)
    {
        // Apenas considera fluxos que realmente enviaram pacotes
        if (flowStats.txPackets == 0) continue;

        // Soma as estatísticas de cada fluxo
        totalTxPackets += flowStats.txPackets;
        totalRxPackets += flowStats.rxPackets;
        totalRxBytes += flowStats.rxBytes;
        totalDelay += flowStats.delaySum.GetSeconds();

        // Duração do fluxo (do primeiro envio até a última recepção)
        double duration = flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds();
        
        // Se a duração for > 0, soma para a média
        if (duration > 0)
        {
            totalDuration += duration;
        }
    }

    // Médias

    // Média de Atraso 
    // Atraso total dividido pelo número de pacotes recebidos
    double mediaAtraso = totalDelay / totalRxPackets;

    // Vazão Média 
    // Bytes totais * 8 (para bits) / duração total em segundos
    // Dividimos por 1024*1024 para obter em Megabits por segundo (Mbps)
    double vazãoMedia = (totalRxBytes * 8.0) / (totalDuration * 1024 * 1024);

    // Taxa de Perda de Pacotes
    // (Pacotes enviados - Pacotes recebidos) / Pacotes enviados
    double perdaPacotes = (double)(totalTxPackets - totalRxPackets) / totalTxPackets;

    
    // Impressão dos Resultados
    
    Simulator::Destroy();

    std::cout << "\nResultados da Simulação \n" << std::endl;
    std::cout << "Protocolo: " << protocolo << " | Clientes WiFi: " << nSta << std::endl;
    std::cout << "Vazão Média (Throughput): " << vazãoMedia << " Mbps" << std::endl;
    std::cout << "Atraso Médio (Delay): " << mediaAtraso * 1000 << " ms" << std::endl; // *1000 para ms
    std::cout << "Taxa de Perda de Pacotes: " << perdaPacotes * 100 << " %" << std::endl;
    std::cout << "Total Pacotes Enviados (Tx): " << totalTxPackets << std::endl;
    std::cout << "Total Pacotes Recebidos (Rx): " << totalRxPackets << std::endl;

    return 0;
}