#!/usr/bin/env python3
"""Batería de pruebas automáticas del programa manet-jerarquica (Partes 2 y 3).

Corre simulaciones cortas y comprueba, con reglas medibles, que el programa
hace lo que dice el diseño (README, secciones 2.3 y 2.7):

  1. E0: ningún nodo se mueve.
  2. E1: los CH no se mueven; los miembros sí.
  3. E2: cada miembro conserva su posición relativa a su CH (el clúster viaja
     junto) y los CH sí se mueven.
  4. Los clústeres no salen de ±regionCluster alrededor de su superclúster y
     los miembros no salen de ±regionNodo alrededor de su CH.
  5. Lo que se mueve va a la velocidad pedida (v ∈ {1, 5, 10, 20} m/s).
  6. Misma semilla → resultado idéntico; semilla distinta → resultado distinto.
  7. Hay 8 flujos de datos con la cantidad esperada de paquetes y, en E0,
     se entregan todos.

Uso (desde la raíz del repo, dentro del Ubuntu donde está ns-3):
    python3 scripts/validar_movilidad.py --ns3 ~/ns-allinone-3.43/ns-3.43

El script copia simulacion/manet-jerarquica.cc a scratch/ de ns-3 antes de
correr, así siempre prueba la versión actual del repo. Solo usa la librería
estándar de Python. Termina con código 0 si todas las pruebas pasan.
"""
import argparse
import csv
import filecmp
import math
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROGRAMA = REPO / "simulacion" / "manet-jerarquica.cc"

# Valores del diseño (README 2.3 y 2.7). Si cambian en el .cc, cambiar aquí.
CENTRO_SUPER = {"X": (350.0, 300.0), "Y": (650.0, 300.0)}
REGION_CLUSTER = 150.0
REGION_NODO = 40.0
VELOCIDADES = (1, 5, 10, 20)
DT = 0.1            # muestreo de posiciones (s)
SIM_TIME = 60.0     # duración de cada simulación de prueba (s)
EPS = 1e-6          # tolerancia numérica (m)
PUERTOS_DATOS = range(5000, 5008)

resultados = []  # (nombre, pasó, detalle)


def comprobar(nombre, condicion, detalle=""):
    resultados.append((nombre, bool(condicion), detalle))
    marca = "PASA " if condicion else "FALLA"
    print(f"  [{marca}] {nombre}" + (f" — {detalle}" if detalle else ""))


def correr(ns3, salida, escenario, cs, ns, run=1, traza=True, sim_time=SIM_TIME):
    args = (f"scratch/manet-jerarquica --scenario={escenario} --clusterSpeed={cs} "
            f"--nodeSpeed={ns} --simTime={sim_time} --RngRun={run} --outDir={salida}")
    if traza:
        args += f" --trazaPosiciones={DT}"
    p = subprocess.run(["./ns3", "run", args], cwd=ns3, capture_output=True, text=True)
    if p.returncode != 0:
        print(p.stdout[-2000:], p.stderr[-2000:], file=sys.stderr)
        raise SystemExit(f"La simulación falló: {args}")
    base = f"{escenario}_cs{cs}_ns{ns}_run{run}"
    return Path(salida) / f"{base}.xml", Path(salida) / f"posiciones_{base}.csv"


def leer_posiciones(ruta):
    """Devuelve {nodo: [(t, x, y), ...]} y {nodo: (cluster, super, indice)}."""
    tray = defaultdict(list)
    info = {}
    with open(ruta, newline="") as f:
        for fila in csv.DictReader(f):
            n = int(fila["nodo"])
            tray[n].append((float(fila["t"]), float(fila["x"]), float(fila["y"])))
            info[n] = (fila["cluster"], fila["super"], int(fila["indice"]))
    return tray, info


def ch_de(info):
    """{cluster: nodo del CH}"""
    return {c: n for n, (c, _, i) in info.items() if i == 0}


def desplazamiento_max(puntos):
    x0, y0 = puntos[0][1], puntos[0][2]
    return max(math.hypot(x - x0, y - y0) for _, x, y in puntos)


def velocidades(puntos):
    return [math.hypot(b[1] - a[1], b[2] - a[2]) / (b[0] - a[0])
            for a, b in zip(puntos, puntos[1:])]


def relativo(tray, n, ch):
    """Posición del nodo n relativa a su CH, en cada instante."""
    return [(t, x - xc, y - yc) for (t, x, y), (_, xc, yc) in zip(tray[n], tray[ch])]


def leer_flujos_datos(ruta_xml):
    raiz = ET.parse(ruta_xml).getroot()
    clase = {f.get("flowId"): f for f in raiz.iter("Flow") if f.get("sourceAddress")}
    flujos = []
    for f in raiz.find("FlowStats"):
        c = clase[f.get("flowId")]
        if int(c.get("destinationPort")) in PUERTOS_DATOS:
            flujos.append((int(f.get("txPackets")), int(f.get("rxPackets"))))
    return flujos


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ns3", type=Path, required=True, help="Carpeta ns-3.43 (donde está ./ns3)")
    a = ap.parse_args()
    ns3 = a.ns3.expanduser().resolve()
    if not (ns3 / "ns3").exists():
        ap.error(f"No encuentro ./ns3 en {ns3}")

    shutil.copy(PROGRAMA, ns3 / "scratch" / PROGRAMA.name)
    print("Compilando...")
    if subprocess.run(["./ns3", "build", "scratch/manet-jerarquica"], cwd=ns3,
                      capture_output=True).returncode != 0:
        raise SystemExit("No compiló el programa")

    salida = Path(tempfile.mkdtemp(prefix="validacion_"))
    print(f"Archivos de prueba en {salida}\n")

    # --- 1. E0: nada se mueve -------------------------------------------------
    print("1) E0 — sin movimiento")
    xml_e0, pos = correr(ns3, salida, "E0", 0, 0)
    tray, info = leer_posiciones(pos)
    mov = max(desplazamiento_max(p) for p in tray.values())
    comprobar("Ningún nodo se mueve", mov < EPS, f"desplazamiento máximo {mov:.2e} m")

    # --- 7. Tráfico (con la corrida E0 de 200 s) --------------------------------
    print("7) Tráfico de la Parte 3 (E0, 200 s)")
    xml_e0_200, _ = correr(ns3, salida, "E0", 0, 0, traza=False, sim_time=200)
    flujos = leer_flujos_datos(xml_e0_200)
    comprobar("Hay exactamente 8 flujos de datos", len(flujos) == 8, f"{len(flujos)} flujos")
    # 4 paq/s desde U[10,11] s hasta 195 s → entre 4·184 = 736 y 4·185 = 740.
    tx_ok = all(736 <= tx <= 741 for tx, _ in flujos)
    comprobar("Cada flujo envía ~4 paquetes/s entre ~10.5 s y 195 s", tx_ok,
              f"tx por flujo: {sorted(tx for tx, _ in flujos)}")
    comprobar("En E0 se entregan todos los paquetes", all(tx == rx for tx, rx in flujos),
              f"{sum(r for _, r in flujos)}/{sum(t for t, _ in flujos)}")

    for v in VELOCIDADES:
        print(f"\n--- Velocidad v = {v} m/s ---")

        # --- 2. E1: solo nodos ------------------------------------------------
        print("2) E1 — solo movilidad de nodo")
        _, pos = correr(ns3, salida, "E1", 0, v)
        tray, info = leer_posiciones(pos)
        chs = ch_de(info)
        mov_ch = max(desplazamiento_max(tray[n]) for n in chs.values())
        mov_m = min(desplazamiento_max(tray[n]) for n, (_, _, i) in info.items() if i > 0)
        comprobar("Los CH no se mueven", mov_ch < EPS, f"máx {mov_ch:.2e} m")
        comprobar("Todos los miembros se mueven", mov_m > 1.0, f"el que menos: {mov_m:.1f} m")
        # 4b. Región de nodo y 5. velocidad de nodo
        fuera = max(max(abs(dx), abs(dy)) for n, (c, _, i) in info.items() if i > 0
                    for _, dx, dy in relativo(tray, n, chs[c]))
        comprobar(f"Miembros dentro de ±{REGION_NODO:.0f} m de su CH", fuera <= REGION_NODO + EPS,
                  f"máx |dx|,|dy| = {fuera:.2f} m")
        vel = [s for n, (_, _, i) in info.items() if i > 0 for s in velocidades(tray[n])]
        exactas = sum(abs(s - v) < 0.01 * v for s in vel) / len(vel)
        comprobar(f"Miembros a {v} m/s", max(vel) <= v * 1.001 and exactas > 0.9,
                  f"{100 * exactas:.1f} % de muestras a v exacta, máx {max(vel):.3f} m/s "
                  "(el resto: rebotes en el borde de la región)")

        # --- 3. E2: solo clústeres --------------------------------------------
        print("3) E2 — solo movilidad de clúster")
        _, pos = correr(ns3, salida, "E2", v, 0)
        tray, info = leer_posiciones(pos)
        chs = ch_de(info)
        mov_ch = min(desplazamiento_max(tray[n]) for n in chs.values())
        comprobar("Todos los CH se mueven", mov_ch > 1.0, f"el que menos: {mov_ch:.1f} m")
        deriva = max(desplazamiento_max(relativo(tray, n, chs[c]))
                     for n, (c, _, i) in info.items() if i > 0)
        comprobar("Los miembros viajan con su CH (posición relativa fija)", deriva < 1e-4,
                  f"deriva máxima {deriva:.2e} m")
        # 4a. Región de clúster y 5. velocidad de clúster
        fuera = max(max(abs(x - CENTRO_SUPER[s][0]), abs(y - CENTRO_SUPER[s][1]))
                    for n, (c, s, i) in info.items() if i == 0 for _, x, y in tray[n])
        comprobar(f"Clústeres dentro de ±{REGION_CLUSTER:.0f} m de su superclúster",
                  fuera <= REGION_CLUSTER + EPS, f"máx |dx|,|dy| = {fuera:.2f} m")
        vel = [s for n in chs.values() for s in velocidades(tray[n])]
        quietas = sum(s < 1e-6 for s in vel) / len(vel)
        exactas = sum(abs(s - v) < 0.01 * v for s in vel) / len(vel)
        comprobar(f"CH a {v} m/s (o quietos en la pausa de 2 s)",
                  max(vel) <= v * 1.001 and quietas + exactas > 0.9,
                  f"{100 * exactas:.1f} % a v exacta, {100 * quietas:.1f} % en pausa, "
                  f"máx {max(vel):.3f} m/s")

        # --- E3: ambas capas a la vez -----------------------------------------
        print("   E3 — ambas movilidades")
        _, pos = correr(ns3, salida, "E3", v, v)
        tray, info = leer_posiciones(pos)
        chs = ch_de(info)
        vel = [s for n, (_, _, i) in info.items() if i > 0 for s in velocidades(tray[n])]
        mov_rel = min(desplazamiento_max(relativo(tray, n, chs[c]))
                      for n, (c, _, i) in info.items() if i > 0)
        comprobar("Los miembros se mueven respecto a su CH y el CH se mueve",
                  mov_rel > 1.0 and min(desplazamiento_max(tray[n]) for n in chs.values()) > 1.0)
        comprobar(f"Velocidad de miembro ≤ v_clúster + v_nodo = {2 * v} m/s",
                  max(vel) <= 2 * v * 1.001, f"máx {max(vel):.3f} m/s")

    # --- 6. Reproducibilidad --------------------------------------------------
    print("\n6) Reproducibilidad (E3, v = 5)")
    xml1, pos1 = correr(ns3, salida / "a", "E3", 5, 5, run=1)
    xml1b, pos1b = correr(ns3, salida / "b", "E3", 5, 5, run=1)
    xml2, pos2 = correr(ns3, salida / "a", "E3", 5, 5, run=2)
    comprobar("Misma semilla → mismas posiciones y mismo XML",
              filecmp.cmp(pos1, pos1b, shallow=False) and filecmp.cmp(xml1, xml1b, shallow=False))
    comprobar("Semilla distinta → posiciones distintas", not filecmp.cmp(pos1, pos2, shallow=False))

    # --- Resumen --------------------------------------------------------------
    malas = [r for r in resultados if not r[1]]
    print(f"\n=== {len(resultados) - len(malas)}/{len(resultados)} pruebas pasaron ===")
    for nombre, _, detalle in malas:
        print(f"  FALLÓ: {nombre} — {detalle}")
    sys.exit(1 if malas else 0)


if __name__ == "__main__":
    main()
