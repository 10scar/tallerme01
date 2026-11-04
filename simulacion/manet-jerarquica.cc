/*
 * =============================================================================
 *  Taller 01 — Simulación de MANETs jerárquicas en ns-3
 *  Modelos estocásticos y simulación en computación y comunicaciones (2026-2S)
 *  Universidad Nacional de Colombia
 * =============================================================================
 *
 *  Archivo:  manet-jerarquica.cc   (copiar a <ns-3.43>/scratch/)
 *  Versión:  ns-3.43
 *
 *  Partes del código y responsables (ver README.md del repositorio):
 *    Parte 1 — David Fernando Benjumea Mora .......... parámetros, nodos,
 *                                                      clústeres, WiFi, AODV,
 *                                                      direccionamiento y
 *                                                      prueba de conectividad.
 *    Parte 2 — José Esteban Alvarado Monroy ......... ConfigurarMovilidad().
 *    Parte 3 — Stephan Alain Roland García Martiquet  ConfigurarTrafico(),
 *                                                      ConfigurarMetricas(),
 *                                                      NetAnim.
 *
 *  -------------------------------------------------------------------------
 *  TOPOLOGÍA JERÁRQUICA
 *  -------------------------------------------------------------------------
 *
 *    Nivel 2 (superclústeres):        X = {A, B}            Y = {C, D}
 *
 *    Nivel 1 (clústeres):          A        B             C        D
 *                                 CH0      CH5          CH10     CH15
 *                                1..4     6..9         11..14   16..19
 *
 *    - 20 nodos: 4 clústeres de 5 nodos. El primer nodo de cada clúster es su
 *      cluster head (CH); los otros 4 son miembros.
 *    - Cada clúster tiene su propio canal WiFi ad hoc (YansWifiChannel
 *      independiente), con alcance efectivo "rangoCluster".
 *    - Los 4 CH tienen además una segunda interfaz en un canal backbone
 *      compartido, con alcance efectivo "rangoBackbone". Toda comunicación
 *      entre clústeres pasa por este backbone.
 *    - Enrutamiento AODV en todos los nodos (sobre todas sus interfaces).
 *
 *    Direccionamiento:
 *      clúster A: 10.1.1.0/24   clúster B: 10.1.2.0/24
 *      clúster C: 10.1.3.0/24   clúster D: 10.1.4.0/24
 *      backbone : 10.2.0.0/24   (solo los 4 CH)
 *
 *  -------------------------------------------------------------------------
 *  USO
 *  -------------------------------------------------------------------------
 *    ./ns3 run "scratch/manet-jerarquica --verificar=true"
 *    ./ns3 run "scratch/manet-jerarquica --scenario=E3 --clusterSpeed=5 \
 *               --nodeSpeed=5 --simTime=200 --RngRun=1 --outDir=resultados/raw"
 *    ./ns3 run "scratch/manet-jerarquica --PrintHelp"
 * =============================================================================
 */

#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetJerarquica");

// =============================================================================
//  Parámetros de la simulación (Parte 1)
//  Los valores por defecto son los del diseño experimental (README, sección 2).
// =============================================================================
struct Parametros
{
    // --- Escenario y réplica ---
    std::string scenario = "E3"; // Etiqueta del escenario (E0..E3)
    double clusterSpeed = 5.0;   // Velocidad de movilidad de clúster (m/s)
    double nodeSpeed = 5.0;      // Velocidad de movilidad de nodo (m/s)
    double superSpeed = 0.0;     // Velocidad de los superclústeres (m/s)
    double simTime = 200.0;      // Duración de la simulación (s)
    std::string outDir = "resultados/raw";
    bool netanim = false;   // Generar traza de NetAnim
    bool verificar = false; // Prueba de conectividad (Parte 1)
    // RngRun se lee directamente como atributo global de ns-3 (--RngRun=N).

    // --- Jerarquía ---
    uint32_t numClusters = 4;       // Clústeres en el nivel 1
    uint32_t nodosPorCluster = 5;   // Nodos por clúster (1 CH + miembros)
    uint32_t clustersPorSuper = 2;  // Clústeres por superclúster (nivel 2)

    // --- Radio ---
    double rangoCluster = 60.0;   // Alcance efectivo dentro del clúster (m)
    double rangoBackbone = 250.0; // Alcance efectivo entre CH (m)
    std::string tasaWifi = "DsssRate1Mbps";

    // --- Geometría inicial ---
    double separacionClusters = 150.0; // Distancia entre centros de clúster (m)
    double radioInicialNodos = 25.0;   // Radio del anillo inicial de miembros (m)
    double origenX = 275.0;            // Centro del primer clúster (m)
    double origenY = 300.0;
};

// =============================================================================
//  Estructura de la red jerárquica (Parte 1)
//  Es la "interfaz" que reciben las Partes 2 y 3: todos los contenedores e
//  interfaces quedan agrupados aquí.
// =============================================================================
struct RedJerarquica
{
    NodeContainer todos;                      // Los 20 nodos
    std::vector<NodeContainer> clusters;      // clusters[k]: nodos del clúster k (CH primero)
    std::vector<NodeContainer> superclusters; // superclusters[s]: nodos del superclúster s
    NodeContainer clusterHeads;               // Los 4 CH (en orden de clúster)
    NodeContainer miembros;                   // Los 16 miembros

    std::vector<Ipv4InterfaceContainer> ifCluster; // ifCluster[k].GetAddress(i): IP del nodo i del clúster k
    Ipv4InterfaceContainer ifBackbone;             // IP de backbone de cada CH

    std::vector<std::string> nombreCluster = {"A", "B", "C", "D"};
    std::vector<std::string> nombreSuper = {"X", "Y"};

    // Índices auxiliares
    uint32_t ClusterDe(uint32_t nodeId, uint32_t nodosPorCluster) const
    {
        return nodeId / nodosPorCluster;
    }

    uint32_t SuperDe(uint32_t cluster, uint32_t clustersPorSuper) const
    {
        return cluster / clustersPorSuper;
    }
};

// =============================================================================
//  Parte 1 — Creación de nodos, clústeres y superclústeres
// =============================================================================
static void
CrearNodos(const Parametros& p, RedJerarquica& red)
{
    red.todos.Create(p.numClusters * p.nodosPorCluster);

    red.clusters.resize(p.numClusters);
    for (uint32_t k = 0; k < p.numClusters; ++k)
    {
        for (uint32_t i = 0; i < p.nodosPorCluster; ++i)
        {
            Ptr<Node> n = red.todos.Get(k * p.nodosPorCluster + i);
            red.clusters[k].Add(n);
            if (i == 0)
            {
                red.clusterHeads.Add(n);
            }
            else
            {
                red.miembros.Add(n);
            }
        }
    }

    uint32_t numSuper = p.numClusters / p.clustersPorSuper;
    red.superclusters.resize(numSuper);
    for (uint32_t k = 0; k < p.numClusters; ++k)
    {
        red.superclusters[red.SuperDe(k, p.clustersPorSuper)].Add(red.clusters[k]);
    }
}

// =============================================================================
//  Parte 1 — Capa física y MAC WiFi
//  Un canal independiente por clúster + un canal backbone para los CH.
//
//  Propagación: modelo de DISCO (RangePropagationLossModel) con retardo de
//  velocidad constante. Todo receptor a una distancia <= MaxRange recibe la
//  señal con la potencia transmitida; más allá no recibe nada. Así el alcance
//  de cada canal es exactamente el configurado (rangoCluster, rangoBackbone).
//
//  Por qué no LogDistance (el modelo por defecto de YansWifiChannelHelper):
//  con la potencia por defecto (16 dBm) y el umbral de detección de preámbulo
//  de ns-3.43 (-82 dBm), un enlace 802.11b de 1 Mbps solo alcanza ~50 m
//  (medido: 50 m -> 10/10 paquetes, 55 m -> 0/10). Eso no permite un backbone
//  entre clústeres separados 150 m.
// =============================================================================
static NetDeviceContainer
InstalarCanal(NodeContainer nodos, double rango, const std::string& tasa)
{
    YansWifiChannelHelper canal;
    canal.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    canal.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(rango));

    YansWifiPhyHelper phy;
    phy.SetChannel(canal.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(tasa),
                                 "ControlMode",
                                 StringValue(tasa));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    return wifi.Install(phy, mac, nodos);
}

// =============================================================================
//  Parte 1 — Pila de internet, AODV y direccionamiento
// =============================================================================
static void
ConfigurarRed(const Parametros& p, RedJerarquica& red)
{
    // 1) Primero se crean TODAS las interfaces WiFi (un canal por clúster y el
    //    backbone). Las interfaces deben existir antes de instalar la pila IP:
    //    AODV abre sus sockets sobre las interfaces presentes al instalarse.
    std::vector<NetDeviceContainer> devsCluster(p.numClusters);
    for (uint32_t k = 0; k < p.numClusters; ++k)
    {
        devsCluster[k] = InstalarCanal(red.clusters[k], p.rangoCluster, p.tasaWifi);
    }
    NetDeviceContainer devsBackbone =
        InstalarCanal(red.clusterHeads, p.rangoBackbone, p.tasaWifi);

    // 2) Pila IP con AODV en todos los nodos.
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(red.todos);

    // 3) Direccionamiento: una subred por clúster (10.1.(k+1).0/24) y una para
    //    el backbone (10.2.0.0/24).
    Ipv4AddressHelper ipv4;
    red.ifCluster.resize(p.numClusters);
    for (uint32_t k = 0; k < p.numClusters; ++k)
    {
        std::string subred = "10.1." + std::to_string(k + 1) + ".0";
        ipv4.SetBase(subred.c_str(), "255.255.255.0");
        red.ifCluster[k] = ipv4.Assign(devsCluster[k]);
    }
    ipv4.SetBase("10.2.0.0", "255.255.255.0");
    red.ifBackbone = ipv4.Assign(devsBackbone);
}

// =============================================================================
//  Parte 2 — Movilidad jerárquica   >>> RESPONSABLE: Parte 2 <<<
//
//  Lo que debe hacer (README, sección 2.3):
//    Superclúster (X, Y)  → punto de referencia (fijo si superSpeed = 0)
//      └─ Clúster (A..D)  → RandomWaypoint relativo a su superclúster,
//                           región ±150 m, velocidad clusterSpeed, pausa 2 s
//            └─ Nodo      → RandomWalk2d relativo a su clúster,
//                           región ±40 m, velocidad nodeSpeed
//    Usar HierarchicalMobilityModel anidado. clusterSpeed = 0 y nodeSpeed = 0
//    deben dejar quieta la capa correspondiente.
//
//  Mientras tanto (Parte 1) se usan posiciones FIJAS con la geometría inicial
//  del diseño: el CH en el centro de su clúster y los miembros en un anillo de
//  radio radioInicialNodos alrededor. Es exactamente el escenario E0.
// =============================================================================
static void
ConfigurarMovilidad(const Parametros& p, RedJerarquica& red)
{
    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
    uint32_t miembrosPorCluster = p.nodosPorCluster - 1;

    for (uint32_t k = 0; k < p.numClusters; ++k)
    {
        double cx = p.origenX + k * p.separacionClusters;
        double cy = p.origenY;

        pos->Add(Vector(cx, cy, 0.0)); // CH en el centro
        for (uint32_t m = 0; m < miembrosPorCluster; ++m)
        {
            double ang = 2.0 * M_PI * m / miembrosPorCluster;
            pos->Add(Vector(cx + p.radioInicialNodos * std::cos(ang),
                            cy + p.radioInicialNodos * std::sin(ang),
                            0.0));
        }
    }

    MobilityHelper movilidad;
    movilidad.SetPositionAllocator(pos);
    movilidad.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    movilidad.Install(red.todos);
}

// =============================================================================
//  Parte 3 — Tráfico   >>> RESPONSABLE: Parte 3 <<<
//
//  Lo que debe hacer (README, sección 2.7):
//    8 flujos UDP OnOff CBR, 512 B, 4 paquetes/s, entre miembros:
//      A→B, B→A, C→D, D→C  (dentro de cada superclúster)
//      A→C, B→D, C→A, D→B  (entre superclústeres)
//    Inicio uniforme en [10, 11] s, fin a los 195 s.
// =============================================================================
static void
ConfigurarTrafico(const Parametros& /*p*/, RedJerarquica& /*red*/)
{
    // TODO (Parte 3)
}

// =============================================================================
//  Parte 3 — Métricas   >>> RESPONSABLE: Parte 3 <<<
//
//  Instalar FlowMonitor antes de Simulator::Run() y, al final, guardar el XML
//  en <outDir>/<scenario>_cs<clusterSpeed>_ns<nodeSpeed>_run<RngRun>.xml
// =============================================================================
static void
ConfigurarMetricas(const Parametros& /*p*/, RedJerarquica& /*red*/)
{
    // TODO (Parte 3)
}

static void
GuardarMetricas(const Parametros& /*p*/)
{
    // TODO (Parte 3)
}

// =============================================================================
//  Parte 1 — Prueba de conectividad (--verificar=true)
//
//  Envía 10 paquetes UDP Echo desde el primer miembro de cada clúster hacia el
//  primer miembro de cada uno de los otros clústeres (12 pares) y cuenta las
//  respuestas recibidas. Sirve para comprobar que la red jerárquica funciona:
//  miembro → CH → backbone → CH → miembro.
// =============================================================================
static std::map<std::pair<uint32_t, uint32_t>, uint32_t> g_respuestas;

static void
RespuestaEcho(std::pair<uint32_t, uint32_t> par, Ptr<const Packet> /*paquete*/)
{
    g_respuestas[par]++;
}

static const uint32_t PAQUETES_PRUEBA = 10;

static void
InstalarPruebaConectividad(const Parametros& p, RedJerarquica& red)
{
    const uint16_t puerto = 9;

    // Servidor de eco en el primer miembro de cada clúster.
    for (uint32_t k = 0; k < p.numClusters; ++k)
    {
        UdpEchoServerHelper servidor(puerto);
        ApplicationContainer app = servidor.Install(red.clusters[k].Get(1));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(p.simTime));
    }

    // Clientes: miembro 1 de cada clúster hacia el miembro 1 de los demás.
    double inicio = 5.0;
    for (uint32_t origen = 0; origen < p.numClusters; ++origen)
    {
        for (uint32_t destino = 0; destino < p.numClusters; ++destino)
        {
            if (origen == destino)
            {
                continue;
            }
            Ipv4Address ipDestino = red.ifCluster[destino].GetAddress(1);
            UdpEchoClientHelper cliente(ipDestino, puerto);
            cliente.SetAttribute("MaxPackets", UintegerValue(PAQUETES_PRUEBA));
            cliente.SetAttribute("Interval", TimeValue(Seconds(1.0)));
            cliente.SetAttribute("PacketSize", UintegerValue(64));

            ApplicationContainer app = cliente.Install(red.clusters[origen].Get(1));
            app.Start(Seconds(inicio));
            app.Stop(Seconds(p.simTime));
            inicio += 0.1; // escalonar para no saturar el arranque de AODV

            auto par = std::make_pair(origen, destino);
            g_respuestas[par] = 0;
            app.Get(0)->TraceConnectWithoutContext("Rx", MakeBoundCallback(&RespuestaEcho, par));
        }
    }
}

static bool
ReportarPruebaConectividad(const Parametros& p, const RedJerarquica& red)
{
    std::cout << "\n=== Prueba de conectividad (UDP Echo, " << PAQUETES_PRUEBA
              << " paquetes por par) ===\n";
    std::cout << " Origen -> Destino   Respuestas\n";
    uint32_t total = 0;
    uint32_t esperado = 0;
    for (const auto& [par, n] : g_respuestas)
    {
        std::cout << "   " << red.nombreCluster[par.first] << "1  ->  " << red.nombreCluster[par.second]
                  << "1        " << std::setw(2) << n << "/" << PAQUETES_PRUEBA << "\n";
        total += n;
        esperado += PAQUETES_PRUEBA;
    }
    double pct = 100.0 * total / esperado;
    std::cout << " Total: " << total << "/" << esperado << " (" << std::fixed << std::setprecision(1)
              << pct << " %)\n";
    // Los primeros paquetes pueden perderse mientras AODV descubre la ruta.
    bool ok = pct >= 90.0;
    std::cout << (ok ? " RESULTADO: la red jerárquica está conectada.\n"
                     : " RESULTADO: hay pares sin conectividad; revisar geometría/alcances.\n");
    (void)p;
    return ok;
}

// =============================================================================
//  Parte 1 — Resumen de la topología en consola
// =============================================================================
static void
ImprimirTopologia(const Parametros& p, const RedJerarquica& red)
{
    std::cout << "=== MANET jerárquica: " << red.todos.GetN() << " nodos, " << p.numClusters
              << " clústeres, " << red.superclusters.size() << " superclústeres ===\n";
    std::cout << "Escenario " << p.scenario << " | clusterSpeed=" << p.clusterSpeed
              << " m/s | nodeSpeed=" << p.nodeSpeed << " m/s | superSpeed=" << p.superSpeed
              << " m/s | simTime=" << p.simTime << " s | RngRun=" << RngSeedManager::GetRun()
              << "\n";
    std::cout << "Alcance clúster=" << p.rangoCluster << " m | backbone=" << p.rangoBackbone
              << " m | WiFi 802.11b " << p.tasaWifi << "\n\n";

    for (uint32_t k = 0; k < p.numClusters; ++k)
    {
        uint32_t s = red.SuperDe(k, p.clustersPorSuper);
        std::cout << "Clúster " << red.nombreCluster[k] << " (superclúster " << red.nombreSuper[s]
                  << ")\n";
        for (uint32_t i = 0; i < red.clusters[k].GetN(); ++i)
        {
            Ptr<Node> n = red.clusters[k].Get(i);
            Vector v = n->GetObject<MobilityModel>()->GetPosition();
            std::cout << "  nodo " << std::setw(2) << n->GetId() << (i == 0 ? " [CH]" : "     ")
                      << "  " << red.ifCluster[k].GetAddress(i);
            if (i == 0)
            {
                std::cout << "  backbone " << red.ifBackbone.GetAddress(k);
            }
            std::cout << "  pos=(" << std::fixed << std::setprecision(1) << v.x << ", " << v.y
                      << ")\n";
        }
    }
    std::cout << std::endl;
}

// =============================================================================
//  main
// =============================================================================
int
main(int argc, char* argv[])
{
    Parametros p;

    CommandLine cmd(__FILE__);
    cmd.AddValue("scenario", "Etiqueta del escenario (E0, E1, E2, E3)", p.scenario);
    cmd.AddValue("clusterSpeed", "Velocidad de movilidad de clúster (m/s)", p.clusterSpeed);
    cmd.AddValue("nodeSpeed", "Velocidad de movilidad de nodo (m/s)", p.nodeSpeed);
    cmd.AddValue("superSpeed", "Velocidad de los superclústeres (m/s)", p.superSpeed);
    cmd.AddValue("simTime", "Duración de la simulación (s)", p.simTime);
    cmd.AddValue("outDir", "Carpeta de salida de resultados", p.outDir);
    cmd.AddValue("netanim", "Generar traza de NetAnim", p.netanim);
    cmd.AddValue("verificar", "Ejecutar la prueba de conectividad de la Parte 1", p.verificar);
    cmd.AddValue("rangoCluster", "Alcance efectivo dentro del clúster (m)", p.rangoCluster);
    cmd.AddValue("rangoBackbone", "Alcance efectivo del backbone entre CH (m)", p.rangoBackbone);
    cmd.Parse(argc, argv);

    RedJerarquica red;
    CrearNodos(p, red);          // Parte 1
    ConfigurarRed(p, red);       // Parte 1
    ConfigurarMovilidad(p, red); // Parte 2
    ConfigurarTrafico(p, red);   // Parte 3
    ConfigurarMetricas(p, red);  // Parte 3

    ImprimirTopologia(p, red);

    if (p.verificar)
    {
        InstalarPruebaConectividad(p, red);
    }

    // NetAnim (Parte 3): AnimationInterface debe crearse antes de Run().

    Simulator::Stop(Seconds(p.simTime));
    Simulator::Run();

    GuardarMetricas(p); // Parte 3

    int codigo = 0;
    if (p.verificar)
    {
        codigo = ReportarPruebaConectividad(p, red) ? 0 : 1;
    }

    Simulator::Destroy();
    return codigo;
}
