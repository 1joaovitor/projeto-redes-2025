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

    /*
     MOBILIDADE
    - AP em (70,70)
    - Nós móveis começam  perto do AP (distância aleatória pequena)
    - Velocidade aleatória entre 1.0 e 2.0 m/s (3.6 a 7.2 km/h)
    - Direção: aponta para longe do AP (AP -> nó)
    */

    MobilityHelper mobility;

    Vector apPosition = Vector(70.0, 70.0, 0.0); // mesma posição do AP que você já coloco
    

    if (cenarioMovel)
    {
        std::cout << "Iniciando cenário de mobilidade (ConstantVelocity, começando perto do AP e indo para longe)." << std::endl;

        // Parâmetros do "próximo ao AP"
        double minRadius = 2.0;   // distância mínima do AP (m)
        double maxRadius = 8.0;   // distância máxima do AP (m) -> "perto"
        
        // Velocidade em m/s (3.6 -- 7.2 km/h => 1.0 -- 2.0 m/s)
        double minSpeed = 1.0;
        double maxSpeed = 2.0;

        // Position allocator: posições iniciais calculadas manualmente perto do AP
        Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();

        // Geradores aleatórios
        Ptr<UniformRandomVariable> varAngle  = CreateObject<UniformRandomVariable>();
        varAngle->SetAttribute("Min", DoubleValue(0.0));
        varAngle->SetAttribute("Max", DoubleValue(2.0 * M_PI));
        Ptr<UniformRandomVariable> varRadius = CreateObject<UniformRandomVariable>();
        varRadius->SetAttribute("Min", DoubleValue(minRadius));
        varRadius->SetAttribute("Max", DoubleValue(maxRadius));

        // Gerador de velocidade
        Ptr<UniformRandomVariable> varSpeed = CreateObject<UniformRandomVariable>();
        varSpeed->SetAttribute("Min", DoubleValue(minSpeed));
        varSpeed->SetAttribute("Max", DoubleValue(maxSpeed));

        // Preenche a lista de posições iniciais (perto do AP)
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            double angle = varAngle->GetValue();
            double radius = varRadius->GetValue();
            double x = apPosition.x + radius * std::cos(angle);
            double y = apPosition.y + radius * std::sin(angle);
            posAlloc->Add(Vector(x, y, 0.0));
        }
        mobility.SetPositionAllocator(posAlloc);

        // Usa ConstantVelocity
        mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
        mobility.Install(wifiStaNodes);

        // Agora define velocidade para cada nó: VETOR DIREÇÃO = (pos - ap), normalizado -> multiplica speed aleatório
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            Ptr<ConstantVelocityMobilityModel> mob = wifiStaNodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
            Vector pos = mob->GetPosition();

            // vetor do AP para o nó
            Vector dir = Vector(pos.x - apPosition.x, pos.y - apPosition.y, 0.0);

            // Se por alguma razão dir for zero (muito improvável), escolhe direção aleatória
            double len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
            if (len < 1e-9)
            {
                double a = varAngle->GetValue();
                dir = Vector(std::cos(a), std::sin(a), 0.0);
                len = 1.0;
            }

            // normaliza
            dir.x /= len;
            dir.y /= len;

            // velocidade aleatória no intervalo [minSpeed, maxSpeed]
            double speed = varSpeed->GetValue();

            Vector velocity = Vector(dir.x * speed, dir.y * speed, 0.0);
            mob->SetVelocity(velocity);

            // Debug
            std::cout << "STA " << i << " pos=(" << pos.x << "," << pos.y << ") velocity=("
                    << velocity.x << "," << velocity.y << ") speed=" << speed << " m/s" << std::endl;
        }
    }
    else
    {
        std::cout << "Iniciando cenário de mobilidade estático (ConstantPositionMobilityModel)." << std::endl;

        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
            "MinX", DoubleValue(65.0), "MinY", DoubleValue(65.0),
            "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(5.0),
            "GridWidth", UintegerValue(2), "LayoutType", StringValue("RowFirst"));

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiStaNodes);
    }

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

    // Definir potência de transmissão em dBm (16 dBm)
    phySta.Set("TxPowerStart", DoubleValue(16.0));
    phySta.Set("TxPowerEnd",   DoubleValue(16.0));
    phyAp.Set("TxPowerStart",  DoubleValue(16.0));
    phyAp.Set("TxPowerEnd",    DoubleValue(16.0));


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
    uint16_t portUdp = 10; // OnOffHelper (UDP)

    // Containers para guardar as aplicações (para poder iniciá-las/pará-las)
    ApplicationContainer serverApps;

    if (protocolo == "TCP")
    {
        std::cout << "Protocolo: TCP (OnOff com controle de taxa)" << std::endl;
        
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", 
            InetSocketAddress(Ipv4Address::GetAny(), portTcp));
        serverApps.Add(sinkHelper.Install(lan.Get(0)));

        // MUDANÇA: Usar OnOffHelper para controlar taxa (rajadas)
        OnOffHelper onoffTcp("ns3::TcpSocketFactory", 
            InetSocketAddress(s2Addr, portTcp));
        
        // Taxa controlada: 256 kbps por cliente (rajadas)
        onoffTcp.SetAttribute("DataRate", StringValue("512kbps"));
        onoffTcp.SetAttribute("PacketSize", UintegerValue(1024));
        
        // Padrão de rajada: 500ms ON, 500ms OFF
        onoffTcp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
        onoffTcp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));

        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            ApplicationContainer app = onoffTcp.Install(wifiStaNodes.Get(i));
            app.Start(Seconds(5.0 + i * 0.1));
            app.Stop(Seconds(simTime - 1.0));
        }
    }
   
    else if (protocolo == "UDP")
    {
        std::cout << "Iniciando cenário UDP (CBR)" << std::endl;

        // Servidor UDP (PacketSink)
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", 
                                    InetSocketAddress(Ipv4Address::GetAny(), portUdp));
        serverApps.Add(sinkHelper.Install(lan.Get(0)));

        // Cliente UDP com OnOff (CBR - Constant Bit Rate)
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(s2Addr, portUdp));
        
        // Taxa ajustada para cada cliente - 2 Mbps é mais razoável
        onoff.SetAttribute("DataRate", StringValue("512kbps"));   // 512 kbps
        onoff.SetAttribute("PacketSize", UintegerValue(512));    // 512 bytes

        // CORREÇÃO: Definir OnTime e OffTime para CBR constante
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        // REMOVER: onoff.SetConstantRate(DataRate("5Mbps")); // Esta linha não existe!
        
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            ApplicationContainer app = onoff.Install(wifiStaNodes.Get(i));
            app.Start(Seconds(5.0 + i * 0.1));
            app.Stop(Seconds(simTime - 1.0));
        }
    }

    else if (protocolo == "Mixed")
    {
        std::cout << "Protocolo: Misto (50% TCP, 50% UDP)" << std::endl;

        PacketSinkHelper sinkTcp("ns3::TcpSocketFactory", 
            InetSocketAddress(Ipv4Address::GetAny(), portTcp));
        serverApps.Add(sinkTcp.Install(lan.Get(0)));

        PacketSinkHelper sinkUdp("ns3::UdpSocketFactory", 
            InetSocketAddress(Ipv4Address::GetAny(), portUdp));
        serverApps.Add(sinkUdp.Install(lan.Get(0)));

        // TCP com OnOff (rajadas)
        OnOffHelper onoffTcp("ns3::TcpSocketFactory", 
            InetSocketAddress(s2Addr, portTcp));
        onoffTcp.SetAttribute("DataRate", StringValue("512kbps"));
        onoffTcp.SetAttribute("PacketSize", UintegerValue(1024));
        onoffTcp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
        onoffTcp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));

        // UDP (mantém igual)
        OnOffHelper onoff("ns3::UdpSocketFactory", 
            InetSocketAddress(s2Addr, portUdp));
        onoff.SetAttribute("DataRate", StringValue("512kbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(512));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        {
            ApplicationContainer app;
            if (i < wifiStaNodes.GetN() / 2) // TCP
                app = onoffTcp.Install(wifiStaNodes.Get(i)); // <-- MUDOU AQUI
            else // UDP
                app = onoff.Install(wifiStaNodes.Get(i));
            
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