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
 *    Parte 2 — Daniel Alonso Gracia Pinto ........... ConfigurarMovilidad().
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

#include <cmath>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
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

    // --- Movilidad jerárquica (Parte 2, README sección 2.3) ---
    double regionSuper = 100.0;    // Semiancho de la región de cada superclúster (m)
    double regionCluster = 150.0;  // Semiancho de la región del clúster respecto a su superclúster (m)
    double regionNodo = 40.0;      // Semiancho de la región del nodo respecto a su clúster (m)
    double pausaCluster = 2.0;     // Pausa del RandomWaypoint de clúster y superclúster (s)
    double pasoNodo = 10.0;        // Distancia recorrida por el nodo antes de cambiar de rumbo (m)
    double trazaPosiciones = 0.0;  // Cada cuántos segundos imprimir posiciones (0 = no)
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
//  Tres capas anidadas con HierarchicalMobilityModel (README, sección 2.3).
//  Cada capa se mueve RELATIVA a la de arriba: si el clúster se desplaza, sus
//  5 nodos se desplazan con él, y además cada miembro se mueve dentro del
//  clúster.
//
//    Superclúster (X, Y)  → RandomWaypoint alrededor de su posición inicial,
//                           región ±regionSuper, velocidad superSpeed
//                           (fijo si superSpeed = 0, como en los experimentos)
//      └─ Clúster (A..D)  → RandomWaypoint relativo a su superclúster,
//                           región ±regionCluster, velocidad clusterSpeed,
//                           pausa pausaCluster
//            ├─ CH        → fijo en el centro del clúster (README 2.7)
//            └─ Miembro   → RandomWalk2d relativo a su clúster,
//                           región ±regionNodo, velocidad nodeSpeed, cambia de
//                           rumbo cada pasoNodo metros
//
//  - Velocidad 0 en una capa → esa capa usa ConstantPositionMobilityModel
//    (queda quieta). Así E0, E1 y E2 aíslan cada tipo de movimiento.
//  - Las velocidades son CONSTANTES (no un rango): en E1 y E2 los nodos o los
//    clústeres se mueven exactamente a v, y los escenarios son comparables.
//  - Con regionNodo = 40 m un miembro está como máximo a 40·√2 ≈ 56.6 m de su
//    CH, dentro del alcance de 60 m: la movilidad de nodo nunca saca a un
//    miembro del alcance directo de su CH (solo cambia la calidad de las rutas
//    locales). La movilidad de clúster sí puede alejar CH más de 250 m.
//  - RandomWalk2d cambia de rumbo por DISTANCIA (pasoNodo), no por tiempo:
//    la forma de la trayectoria es la misma a cualquier velocidad, solo cambia
//    lo rápido que se recorre. Eso hace comparable el barrido de velocidades.
//  - Las posiciones iniciales son las del diseño (README 2.7) en todos los
//    escenarios: superclústeres en (350, 300) y (650, 300), clústeres a
//    ±separacionClusters/2 de su superclúster, CH en el centro y miembros en
//    un anillo de radioInicialNodos.
// =============================================================================

// Une un modelo "hijo" (coordenadas relativas) a su "padre".
static Ptr<HierarchicalMobilityModel>
Anidar(Ptr<MobilityModel> padre, Ptr<MobilityModel> hijo)
{
    Ptr<HierarchicalMobilityModel> h = CreateObject<HierarchicalMobilityModel>();
    h->SetParent(padre);
    h->SetChild(hijo);
    return h;
}

// Los modelos aleatorios de ns-3 NO se posicionan con SetPosition() antes de
// la simulación (reprograman su primer movimiento). En su lugar, cada uno
// arranca en su origen (0, 0) y se coloca sobre un "ancla" fija en la posición
// inicial; su región se desplaza en -inicio para que quede centrada en el
// centro de la capa padre. El resultado es una posición relativa a la capa
// padre que empieza en `inicio` y se mueve en [-semiancho, semiancho]².
static Ptr<MobilityModel>
Anclar(Vector inicio, Ptr<MobilityModel> movimiento)
{
    Ptr<ConstantPositionMobilityModel> ancla = CreateObject<ConstantPositionMobilityModel>();
    ancla->SetPosition(inicio);
    if (!movimiento)
    {
        return ancla; // capa quieta
    }
    return Anidar(ancla, movimiento);
}

// RandomWaypoint de velocidad constante, con destinos uniformes en
// [-semiancho, semiancho]² (coordenadas de la capa padre).
static Ptr<MobilityModel>
CrearWaypoint(double velocidad, double semiancho, double pausa, Vector inicio)
{
    if (velocidad <= 0.0)
    {
        return Anclar(inicio, nullptr);
    }

    auto uniforme = [semiancho](double desplazamiento) {
        std::ostringstream s;
        s << "ns3::UniformRandomVariable[Min=" << -semiancho - desplazamiento
          << "|Max=" << semiancho - desplazamiento << "]";
        return s.str();
    };
    Ptr<RandomRectanglePositionAllocator> destinos =
        CreateObject<RandomRectanglePositionAllocator>();
    destinos->SetAttribute("X", StringValue(uniforme(inicio.x)));
    destinos->SetAttribute("Y", StringValue(uniforme(inicio.y)));

    std::ostringstream vel;
    vel << "ns3::ConstantRandomVariable[Constant=" << velocidad << "]";
    std::ostringstream pau;
    pau << "ns3::ConstantRandomVariable[Constant=" << pausa << "]";

    Ptr<RandomWaypointMobilityModel> rwp = CreateObject<RandomWaypointMobilityModel>();
    rwp->SetAttribute("Speed", StringValue(vel.str()));
    rwp->SetAttribute("Pause", StringValue(pau.str()));
    rwp->SetAttribute("PositionAllocator", PointerValue(destinos));
    return Anclar(inicio, rwp);
}

// RandomWalk2d de velocidad constante, acotado a [-semiancho, semiancho]²
// (coordenadas del clúster). Cambia de rumbo cada `paso` metros.
static Ptr<MobilityModel>
CrearCaminata(double velocidad, double semiancho, double paso, Vector inicio)
{
    if (velocidad <= 0.0)
    {
        return Anclar(inicio, nullptr);
    }

    std::ostringstream vel;
    vel << "ns3::ConstantRandomVariable[Constant=" << velocidad << "]";

    Ptr<RandomWalk2dMobilityModel> rw = CreateObject<RandomWalk2dMobilityModel>();
    rw->SetAttribute("Bounds",
                     RectangleValue(Rectangle(-semiancho - inicio.x,
                                              semiancho - inicio.x,
                                              -semiancho - inicio.y,
                                              semiancho - inicio.y)));
    rw->SetAttribute("Mode", StringValue("Distance"));
    rw->SetAttribute("Distance", DoubleValue(paso));
    rw->SetAttribute("Speed", StringValue(vel.str()));
    return Anclar(inicio, rw);
}

static void
ConfigurarMovilidad(const Parametros& p, RedJerarquica& red)
{
    uint32_t numSuper = red.superclusters.size();
    uint32_t miembrosPorCluster = p.nodosPorCluster - 1;

    // Capa 1 — superclústeres, en coordenadas absolutas. El centro de cada
    // superclúster es el punto medio de sus clústeres en la geometría inicial.
    std::vector<Ptr<MobilityModel>> movSuper(numSuper);
    std::vector<Vector> centroSuper(numSuper);
    for (uint32_t s = 0; s < numSuper; ++s)
    {
        double primero = p.origenX + s * p.clustersPorSuper * p.separacionClusters;
        double cx = primero + (p.clustersPorSuper - 1) * p.separacionClusters / 2.0;
        centroSuper[s] = Vector(cx, p.origenY, 0.0);

        // Región ±regionSuper alrededor de su centro inicial.
        Ptr<ConstantPositionMobilityModel> centro = CreateObject<ConstantPositionMobilityModel>();
        centro->SetPosition(centroSuper[s]);
        movSuper[s] = Anidar(
            centro,
            CrearWaypoint(p.superSpeed, p.regionSuper, p.pausaCluster, Vector(0, 0, 0)));
    }

    // Capa 2 — clústeres, relativos a su superclúster.
    for (uint32_t k = 0; k < p.numClusters; ++k)
    {
        uint32_t s = red.SuperDe(k, p.clustersPorSuper);
        double cxAbs = p.origenX + k * p.separacionClusters;
        Vector inicioRel(cxAbs - centroSuper[s].x, p.origenY - centroSuper[s].y, 0.0);

        Ptr<MobilityModel> movCluster = Anidar(
            movSuper[s],
            CrearWaypoint(p.clusterSpeed, p.regionCluster, p.pausaCluster, inicioRel));

        // Capa 3 — nodos, relativos a su clúster. El CH (índice 0) va fijo en
        // el centro; los miembros arrancan en un anillo y caminan al azar.
        for (uint32_t i = 0; i < red.clusters[k].GetN(); ++i)
        {
            Ptr<MobilityModel> movNodo;
            if (i == 0)
            {
                Ptr<ConstantPositionMobilityModel> centro =
                    CreateObject<ConstantPositionMobilityModel>();
                centro->SetPosition(Vector(0, 0, 0));
                movNodo = centro;
            }
            else
            {
                double ang = 2.0 * M_PI * (i - 1) / miembrosPorCluster;
                Vector inicio(p.radioInicialNodos * std::cos(ang),
                              p.radioInicialNodos * std::sin(ang),
                              0.0);
                movNodo = CrearCaminata(p.nodeSpeed, p.regionNodo, p.pasoNodo, inicio);
            }
            red.clusters[k].Get(i)->AggregateObject(Anidar(movCluster, movNodo));
        }
    }
}

// Validación de la Parte 2 (--trazaPosiciones=T): cada T segundos guarda la
// posición de los 20 nodos en
//   <outDir>/posiciones_<scenario>_cs<clusterSpeed>_ns<nodeSpeed>_run<RngRun>.csv
// (columnas t,nodo,cluster,super,indice,x,y; indice 0 = CH) y, cada 10 s,
// imprime en consola la posición de cada CH y la distancia del miembro 1 a su
// CH: si el CH cambia de lugar se mueve el clúster; si cambia la distancia, se
// mueve el nodo. scripts/validar_movilidad.py revisa el CSV automáticamente.
static std::ofstream g_posiciones;

static void
TrazarPosiciones(const Parametros& p, const RedJerarquica& red, uint32_t paso)
{
    double t = Simulator::Now().GetSeconds();
    for (uint32_t k = 0; k < p.numClusters; ++k)
    {
        for (uint32_t i = 0; i < red.clusters[k].GetN(); ++i)
        {
            Ptr<Node> n = red.clusters[k].Get(i);
            Vector v = n->GetObject<MobilityModel>()->GetPosition();
            g_posiciones << t << "," << n->GetId() << "," << red.nombreCluster[k] << ","
                         << red.nombreSuper[red.SuperDe(k, p.clustersPorSuper)] << "," << i << ","
                         << v.x << "," << v.y << "\n";
        }
    }

    uint32_t cadaConsola = std::max<uint32_t>(1, std::lround(10.0 / p.trazaPosiciones));
    if (paso % cadaConsola == 0)
    {
        std::cout << "t=" << std::fixed << std::setprecision(0) << std::setw(4) << t << " s";
        for (uint32_t k = 0; k < p.numClusters; ++k)
        {
            Vector ch = red.clusters[k].Get(0)->GetObject<MobilityModel>()->GetPosition();
            Vector m1 = red.clusters[k].Get(1)->GetObject<MobilityModel>()->GetPosition();
            double d = std::hypot(m1.x - ch.x, m1.y - ch.y);
            std::cout << " | " << red.nombreCluster[k] << ": CH(" << std::setw(4) << ch.x << ","
                      << std::setw(4) << ch.y << ") m1 a " << std::setw(2) << d << " m";
        }
        std::cout << "\n";
    }

    if (t + p.trazaPosiciones <= p.simTime + 1e-9)
    {
        Simulator::Schedule(Seconds(p.trazaPosiciones), &TrazarPosiciones, std::cref(p),
                            std::cref(red), paso + 1);
    }
}

static void
IniciarTrazaPosiciones(const Parametros& p, const RedJerarquica& red)
{
    std::filesystem::create_directories(p.outDir);
    std::ostringstream nombre;
    nombre << p.outDir << "/posiciones_" << p.scenario << "_cs" << p.clusterSpeed << "_ns"
           << p.nodeSpeed << "_run" << RngSeedManager::GetRun() << ".csv";
    g_posiciones.open(nombre.str());
    g_posiciones << std::setprecision(10) << "t,nodo,cluster,super,indice,x,y\n";
    std::cout << "Traza de posiciones: " << nombre.str() << "\n";
    Simulator::Schedule(Seconds(0.0), &TrazarPosiciones, std::cref(p), std::cref(red), 0u);
}

// =============================================================================
//  Parte 3 — Tráfico   >>> RESPONSABLE: Parte 3 <<<
//
//  8 flujos UDP OnOff CBR (README, sección 2.7): 512 B a 4 paquetes/s, es
//  decir 16 384 bit/s por flujo, siempre de miembro a miembro:
//    A1→B1, B1→A1, C1→D1, D1→C1  (dentro de cada superclúster, miembro 1)
//    A2→C2, B2→D2, C2→A2, D2→B2  (entre superclústeres, miembro 2)
//
//  Los CH no generan ni reciben tráfico: solo reenvían por el backbone.
//  Cada flujo arranca en un instante uniforme en [10, 11] s (AODV ya se
//  estabilizó y los flujos no salen todos a la vez) y se detiene a los 195 s,
//  para que los últimos paquetes alcancen a llegar antes del fin.
//  Con --verificar=true no se instala: solo corre la prueba de la Parte 1.
// =============================================================================
static const double INICIO_TRAFICO_MIN = 10.0;
static const double INICIO_TRAFICO_MAX = 11.0;
static const double FIN_TRAFICO = 195.0;

struct Flujo
{
    uint32_t clusterOrigen;
    uint32_t miembroOrigen;
    uint32_t clusterDestino;
    uint32_t miembroDestino;
};

static void
ConfigurarTrafico(const Parametros& p, RedJerarquica& red)
{
    if (p.verificar)
    {
        return;
    }

    const std::vector<Flujo> flujos = {
        {0, 1, 1, 1}, // A1 -> B1
        {1, 1, 0, 1}, // B1 -> A1
        {2, 1, 3, 1}, // C1 -> D1
        {3, 1, 2, 1}, // D1 -> C1
        {0, 2, 2, 2}, // A2 -> C2
        {1, 2, 3, 2}, // B2 -> D2
        {2, 2, 0, 2}, // C2 -> A2
        {3, 2, 1, 2}, // D2 -> B2
    };

    // Si simTime es corto (pruebas rápidas), el tráfico termina 5 s antes del fin.
    double fin = std::min(FIN_TRAFICO, p.simTime - 5.0);

    Ptr<UniformRandomVariable> inicio = CreateObject<UniformRandomVariable>();
    inicio->SetAttribute("Min", DoubleValue(INICIO_TRAFICO_MIN));
    inicio->SetAttribute("Max", DoubleValue(INICIO_TRAFICO_MAX));

    const uint16_t puertoBase = 5000;
    for (uint32_t f = 0; f < flujos.size(); ++f)
    {
        const Flujo& fl = flujos[f];
        uint16_t puerto = puertoBase + f;
        Ipv4Address ipDestino = red.ifCluster[fl.clusterDestino].GetAddress(fl.miembroDestino);

        // Receptor en el miembro destino.
        PacketSinkHelper sumidero("ns3::UdpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), puerto));
        ApplicationContainer appRx =
            sumidero.Install(red.clusters[fl.clusterDestino].Get(fl.miembroDestino));
        appRx.Start(Seconds(0.0));
        appRx.Stop(Seconds(p.simTime));

        // Emisor CBR: siempre encendido (OffTime = 0).
        OnOffHelper emisor("ns3::UdpSocketFactory", InetSocketAddress(ipDestino, puerto));
        emisor.SetAttribute("PacketSize", UintegerValue(512));
        emisor.SetAttribute("DataRate", DataRateValue(DataRate("16384bps")));
        emisor.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        emisor.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer appTx =
            emisor.Install(red.clusters[fl.clusterOrigen].Get(fl.miembroOrigen));
        appTx.Start(Seconds(inicio->GetValue()));
        appTx.Stop(Seconds(fin));
    }
}

// =============================================================================
//  Parte 3 — Métricas   >>> RESPONSABLE: Parte 3 <<<
//
//  FlowMonitor se instala antes de Simulator::Run() y, al final, se guarda el
//  XML en <outDir>/<scenario>_cs<clusterSpeed>_ns<nodeSpeed>_run<RngRun>.xml
//  Además se imprime un resumen (PDR, retardo, throughput, jitter) calculado
//  con las mismas fórmulas del README, para comparar con parse_flowmon.py.
// =============================================================================
static FlowMonitorHelper g_flowHelper;
static Ptr<FlowMonitor> g_monitor;

static void
ConfigurarMetricas(const Parametros& p, RedJerarquica& /*red*/)
{
    if (p.verificar)
    {
        return;
    }
    g_monitor = g_flowHelper.InstallAll();
}

static std::string
NombreSalida(const Parametros& p, const std::string& prefijo)
{
    std::ostringstream nombre;
    nombre << p.outDir << "/" << prefijo << p.scenario << "_cs" << p.clusterSpeed << "_ns"
           << p.nodeSpeed << "_run" << RngSeedManager::GetRun() << ".xml";
    return nombre.str();
}

static void
GuardarMetricas(const Parametros& p)
{
    if (p.verificar || !g_monitor)
    {
        return;
    }

    std::filesystem::create_directories(p.outDir);
    std::string archivo = NombreSalida(p, "");

    g_monitor->CheckForLostPackets();
    g_monitor->SerializeToXmlFile(archivo, true, true);

    uint64_t tx = 0;
    uint64_t rx = 0;
    uint64_t rxBytes = 0;
    uint64_t rxParaJitter = 0;
    double retardo = 0.0;
    double jitter = 0.0;
    for (const auto& [id, st] : g_monitor->GetFlowStats())
    {
        tx += st.txPackets;
        rx += st.rxPackets;
        rxBytes += st.rxBytes;
        retardo += st.delaySum.GetSeconds();
        jitter += st.jitterSum.GetSeconds();
        if (st.rxPackets > 1)
        {
            rxParaJitter += st.rxPackets - 1;
        }
    }

    double duracion = std::min(FIN_TRAFICO, p.simTime - 5.0) - INICIO_TRAFICO_MIN;
    double pdr = tx > 0 ? 100.0 * rx / tx : 0.0;
    double retardoMedio = rx > 0 ? retardo / rx : 0.0;
    double throughput = duracion > 0 ? 8.0 * rxBytes / duracion / 1000.0 : 0.0;
    double jitterMedio = rxParaJitter > 0 ? jitter / rxParaJitter : 0.0;

    std::cout << "\n=== Métricas (FlowMonitor) ===\n";
    std::cout << " Archivo:    " << archivo << "\n";
    std::cout << " Paquetes:   " << rx << "/" << tx << " recibidos\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << " PDR:        " << pdr << " %\n";
    std::cout << " Throughput: " << throughput << " kbit/s\n";
    std::cout << std::setprecision(4);
    std::cout << " Retardo:    " << retardoMedio << " s\n";
    std::cout << " Jitter:     " << jitterMedio << " s\n";
}

// =============================================================================
//  Parte 3 — NetAnim (--netanim=true)   >>> RESPONSABLE: Parte 3 <<<
//
//  Traza en <outDir>/netanim_<...>.xml. Los CH se dibujan en rojo y más
//  grandes; los miembros en azul. AnimationInterface debe existir antes de
//  Simulator::Run(), por eso se guarda en una variable global.
// =============================================================================
static std::unique_ptr<AnimationInterface> g_anim;

static void
ConfigurarNetAnim(const Parametros& p, const RedJerarquica& red)
{
    if (!p.netanim)
    {
        return;
    }

    std::filesystem::create_directories(p.outDir);
    std::string archivo = NombreSalida(p, "netanim_");
    g_anim = std::make_unique<AnimationInterface>(archivo);

    for (uint32_t k = 0; k < p.numClusters; ++k)
    {
        for (uint32_t i = 0; i < red.clusters[k].GetN(); ++i)
        {
            Ptr<Node> n = red.clusters[k].Get(i);
            bool esCH = (i == 0);
            g_anim->UpdateNodeDescription(n, red.nombreCluster[k] + std::to_string(i) +
                                                 (esCH ? " [CH]" : ""));
            if (esCH)
            {
                g_anim->UpdateNodeColor(n, 220, 20, 60);
                g_anim->UpdateNodeSize(n->GetId(), 6.0, 6.0);
            }
            else
            {
                g_anim->UpdateNodeColor(n, 30, 144, 255);
                g_anim->UpdateNodeSize(n->GetId(), 3.5, 3.5);
            }
        }
    }
    std::cout << "Traza NetAnim: " << archivo << "\n";
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
    cmd.AddValue("regionCluster", "Semiancho de la región de movimiento del clúster (m)", p.regionCluster);
    cmd.AddValue("regionNodo", "Semiancho de la región de movimiento del nodo (m)", p.regionNodo);
    cmd.AddValue("trazaPosiciones", "Imprimir posiciones cada T segundos (0 = no)", p.trazaPosiciones);
    cmd.Parse(argc, argv);

    RedJerarquica red;
    CrearNodos(p, red);          // Parte 1
    ConfigurarRed(p, red);       // Parte 1
    ConfigurarMovilidad(p, red); // Parte 2
    ConfigurarTrafico(p, red);   // Parte 3
    ConfigurarMetricas(p, red);  // Parte 3

    ImprimirTopologia(p, red);

    if (p.trazaPosiciones > 0.0)
    {
        IniciarTrazaPosiciones(p, red); // Parte 2
    }

    if (p.verificar)
    {
        InstalarPruebaConectividad(p, red);
    }

    // NetAnim (Parte 3): AnimationInterface debe crearse antes de Run().
    ConfigurarNetAnim(p, red);   // Parte 3

    Simulator::Stop(Seconds(p.simTime));
    Simulator::Run();

    GuardarMetricas(p); // Parte 3

    int codigo = 0;
    if (p.verificar)
    {
        codigo = ReportarPruebaConectividad(p, red) ? 0 : 1;
    }

    g_anim.reset(); // Parte 3
    Simulator::Destroy();
    return codigo;
}
