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
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    std::string protocolo = "UDP"; // Protocolo TCP, UDP ou Mixed
    uint32_t nSta = 4; // Numero de clientes Wifi
    std::string dataRate = "100Mbps"; // Taxa de dados
    std::string delay = "2ms"; // Atraso do enlace
    bool cenarioMovel = false;
    std::string pcapPrefix = "";
    double simTime = 60.0; //tempo de simulação

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1500)); // Quadro TCP

    CommandLine cmd; 
    cmd.AddValue("nSta", "Number of WiFi clients", nSta );
    cmd.AddValue("protocolo", "Protocolo a ser usado (TCP, UDP, Mixed)", protocolo);
    cmd.AddValue("cenarioMovel", "Ativa o cenario de mobilidade (true/false)", cenarioMovel);
    cmd.AddValue("pcapPrefix", "Prefixo de caminho para arquivos pcap", pcapPrefix); 
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

    if (!pcapPrefix.empty())
    {
        csma.EnablePcap(pcapPrefix + "lan", csmaDevices.Get(1), true); 
    } else {
        csma.EnablePcap("lan", csmaDevices.Get(1), true); 
    }



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

    /* 
    
    SEÇÃO DE MOBILIDADE

    */ 

    // Mobilidade

    MobilityHelper mobility;

    if (cenarioMovel)
    {
        // Nós MÓVEIS
        std::cout << "Iniciando cenário de mobilidade" << std::endl;
        
        // Define os limites da área 
        double minBounds = 50.0; 
        double maxBounds = 90.0; 

        double minStart = minBounds + 0.1;
        double maxStart = maxBounds - 0.1;
        
        // Coloca os clientes aleatoriamente dentro da área
        Ptr<UniformRandomVariable> varX = CreateObject<UniformRandomVariable>();
        varX->SetAttribute("Min", DoubleValue(minStart));
        varX->SetAttribute("Max", DoubleValue(maxStart));
        Ptr<UniformRandomVariable> varY = CreateObject<UniformRandomVariable>();
        varY->SetAttribute("Min", DoubleValue(minStart));
        varY->SetAttribute("Max", DoubleValue(maxStart));
        
        Ptr<RandomRectanglePositionAllocator> posAllocator = CreateObject<RandomRectanglePositionAllocator>();
        posAllocator->SetX(varX);
        posAllocator->SetY(varY);
        mobility.SetPositionAllocator(posAllocator); 

        // O modelo de mobilidade NUNCA deve sair da área
        // RandomWalk2dMobilityModel -> andar aleatoriamente
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
            "Bounds", RectangleValue(Rectangle(minBounds, maxBounds, minBounds, maxBounds)), // Agora coincide!
            "Time", StringValue("2s"), // Mudar de direção a cada 2s
            "Speed", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=1.0]")); 
    }
    else
    {
        // Nós ESTÁTICOS
        std::cout << "Iniciando cenário de mobilidade estático" << std::endl;
        
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
            "MinX", DoubleValue(65.0), "MinY", DoubleValue(65.0),
            "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(5.0),
            "GridWidth", UintegerValue(2), "LayoutType", StringValue("RowFirst"));

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    }

    // Instala o modelo (estático OU móvel) nos clientes Wi-Fi
    mobility.Install(wifiStaNodes);

    // DEBUG
    // for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    // {
    //     Ptr<MobilityModel> mob = wifiStaNodes.Get(i)->GetObject<MobilityModel>();
    //     Vector pos = mob->GetPosition();
    //     std::cout << "Cliente " << i << " posição inicial: (" 
    //             << pos.x << ", " << pos.y << ")" << std::endl;
        
    //     // Calcular distância ao AP (70, 70)
    //     double dist = sqrt(pow(pos.x - 70.0, 2) + pow(pos.y - 70.0, 2));
    //     std::cout << "  Distância ao AP: " << dist << " m" << std::endl;
    // }

    // Mobilidade do AP (s0) -> permanecer parado nos dois casos
    MobilityHelper apMobility;
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Posiciona o AP no centro da área
    Ptr<ListPositionAllocator> apPosAllocator = CreateObject<ListPositionAllocator>();
    apPosAllocator->Add(Vector(70.0, 70.0, 0.0)); // Posição (x=70, y=70)
    apMobility.SetPositionAllocator(apPosAllocator);


    apMobility.Install(wifiApNode);

    // FIM DA SEÇÃO MOBILIDADE



    // Montar o Wi-Fi (AP + STAs)
    // Helpers de PHY e Canal (compartilham o mesmo meio rádio)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    YansWifiPhyHelper phySta; 
    phySta.SetErrorRateModel("ns3::NistErrorRateModel");
    phySta.SetChannel(wifiChannel); // Usa o canal criado

    YansWifiPhyHelper phyAp; 
    phyAp.SetErrorRateModel("ns3::NistErrorRateModel");
    phyAp.SetChannel(wifiChannel); // Usa o MESMO canal

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
            "ActiveProbing", BooleanValue(true));
    NetDeviceContainer staDevs = wifi.Install(phySta, mac, wifiStaNodes);

    // DEBUG: Verificar associação após 4 segundos (antes das apps iniciarem)
    // Simulator::Schedule(Seconds(4.0), [staDevs]() {
    //     std::cout << "\n=== Status de Associação (t=4s) ===" << std::endl;
    //     for (uint32_t i = 0; i < staDevs.GetN(); ++i)
    //     {
    //         Ptr<NetDevice> dev = staDevs.Get(i);
    //         Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
    //         if (wifiDev)
    //         {
    //             Ptr<StaWifiMac> staMac = DynamicCast<StaWifiMac>(wifiDev->GetMac());
    //             if (staMac && staMac->IsAssociated())
    //             {
    //                 std::cout << "Cliente " << i << ": ASSOCIADO ✓" << std::endl;
    //             }
    //             else
    //             {
    //                 std::cout << "Cliente " << i << ": NÃO ASSOCIADO ✗" << std::endl;
    //             }
    //         }
    //     }
    //     std::cout << "====================================\n" << std::endl;
    // });

    // Sniffer no Wi-Fi para debug 
    if (!pcapPrefix.empty())
    {
        phySta.EnablePcap(pcapPrefix + "wifi-clients", staDevs);
    } else {
        phySta.EnablePcap("wifi-clients", staDevs);
    }

    // AP (s0)
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevs  = wifi.Install(phyAp, mac, wifiApNode);

    // Endereçar o Wi-Fi (192.168.0.0/24)
    addr.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer staIfaces = addr.Assign(staDevs);
    Ipv4InterfaceContainer apIfaces  = addr.Assign(apDevs);

    // DEBUG: Verificar se os clientes têm endereços IP válidos
    // std::cout << "\n=== Endereços IP dos Clientes WiFi ===" << std::endl;
    // for (uint32_t i = 0; i < staIfaces.GetN(); ++i)
    // {
    //     std::cout << "Cliente " << i << ": " << staIfaces.GetAddress(i) << std::endl;
    // }
    // std::cout << "===================================\n" << std::endl;

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
            app.Start(Seconds(5.0 + i * 0.1)); // Início escalonado dos clientes
            app.Stop(Seconds(simTime - 1.0));
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
        echoClientHelper.SetAttribute("PacketSize", UintegerValue(512)); // 512 bytes
        echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(0.008))); // 512kbps
        echoClientHelper.SetAttribute("MaxPackets", UintegerValue(7500)); // (60s / 0.008s)

        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            ApplicationContainer app = echoClientHelper.Install(wifiStaNodes.Get(i));
            app.Start(Seconds(5.0 + i * 0.1)); // Início escalonado dos clientes
            app.Stop(Seconds(simTime - 1.0));
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
        echoClientHelper.SetAttribute("PacketSize", UintegerValue(512));
        echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(0.008)));
        echoClientHelper.SetAttribute("MaxPackets", UintegerValue(7500)); 
        
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
            app.Start(Seconds(5.0 + i * 0.1));
            app.Stop(Seconds(simTime - 1.0));
        }
    }

    // Define os horários de início e fim para as aplicações
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime + 1.0));

    // FIM DAS APLICAÇÕES


    /* 
    
    SEÇÃO DE MÉTRICAS (FLOWMONITOR)

    */ 

    // Instalação do FlowMonitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll(); // Instala em todos os nós

    // Janela da simulação
    Simulator::Stop(Seconds(63.0));
    Simulator::Run();



    // Resultados do FlowMonitor
  
    flowMonitor->CheckForLostPackets(); 

    // Estatísticas de todos os fluxos monitorados
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();


    double totalDelay = 0;       // Atraso total
    double totalRxBytes = 0;     // Total de bytes recebidos
    uint32_t totalTxPackets = 0; // Total de pacotes enviados
    uint32_t totalRxPackets = 0; // Total de pacotes recebidos
    double totalThroughput = 0.0; // Soma das vazões individuais
    uint32_t totalFlows = 0;      // Contador de fluxos válidos
    Ipv4Address serverIp = csmaIfaces.GetAddress(0);
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    // Itera por cada fluxo (ex: c0->s2, c1->s2, etc.)
    for (auto const& [flowId, flowStats] : stats)
    {

        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);

        // Isso ignora os fluxos de "echo" (UDP) e de "ACK" (TCP)
        if (t.destinationAddress != serverIp)
        {
            continue; // Pula este fluxo (é um ACK ou um Echo de volta)
        }

        // Apenas considera fluxos que realmente enviaram pacotes
        if (flowStats.txPackets == 0)
            continue;

        totalTxPackets += flowStats.txPackets;
        totalRxPackets += flowStats.rxPackets;
        totalRxBytes += flowStats.rxBytes;
        totalDelay += flowStats.delaySum.GetSeconds();

        // Calcula vazão individual do fluxo
        double duration = flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds();
        if (duration > 0)
        {
            // Adiciona a vazão deste fluxo para soma total
            totalThroughput += (flowStats.rxBytes * 8.0) / (duration * 1024 * 1024);
            totalFlows++; // Conta como um fluxo válido para a média
        }
    }


    // Média de Atraso (Agregada: Atraso total / Pacotes totais)
    double mediaAtraso = (totalRxPackets > 0) ? (totalDelay / totalRxPackets) : 0;

    // Vazão Média (Média das vazões de cada fluxo)
    double vazãoMedia = (totalFlows > 0) ? (totalThroughput / totalFlows) : 0;

    // Taxa de Perda de Pacotes (Agregada: Perdidos / Enviados)
    double perdaPacotes =
        (totalTxPackets > 0) ? ((double)(totalTxPackets - totalRxPackets) / totalTxPackets) : 0;

    // Impressão dos resultados
    Simulator::Destroy();

    std::cout << "\nResultados da Simulação" << std::endl;
    std::cout << "Protocolo: " << protocolo << " | Clientes WiFi: " << nSta
              << " | Cenário Móvel: " << (cenarioMovel ? "Sim" : "Não") << std::endl;
    std::cout << "Vazão Média (Throughput): \t" << vazãoMedia << " Mbps" << std::endl;
    std::cout << "Atraso Médio (Delay): \t\t" << mediaAtraso * 1000 << " ms" << std::endl; // *1000 para ms
    std::cout << "Taxa de Perda de Pacotes: \t" << perdaPacotes * 100 << " %" << std::endl;
    std::cout << "Total Pacotes Enviados (Tx): \t" << totalTxPackets << std::endl;
    std::cout << "Total Pacotes Recebidos (Rx): \t" << totalRxPackets << std::endl;

    // Linha de Resumo para Script
    // Esta linha será usada pelo script run_experiments.sh para criar o CSV
    std::cout << "ResumoCSV," 
              << protocolo << "," 
              << nSta << "," 
              << (cenarioMovel ? "Movel" : "Estatico") << "," 
              << vazãoMedia << "," 
              << mediaAtraso * 1000 << "," 
              << perdaPacotes * 100 
              << std::endl;


    return 0;
}