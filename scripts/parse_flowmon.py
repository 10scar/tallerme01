#!/usr/bin/env python3
"""Parte 4: FlowMonitor XML → resultados/resumen.csv, FILTRANDO UDP 5000–5007.

Advertencia: el XML también contiene decenas de flujos AODV (UDP 654).
Nunca agregar FlowStats sin cruzar su flowId con Ipv4FlowClassifier.
"""
from __future__ import annotations

import argparse
import csv
import math
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
NOMBRE = re.compile(r'^(E[0-3])_cs(\d+)_ns(\d+)_run(\d+)\.xml$')
TIEMPO = re.compile(r'^\+?(-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)(ps|ns|us|µs|ms|s)$')
UNIDADES = {'ps': 1e-12, 'ns': 1e-9, 'us': 1e-6, 'µs': 1e-6, 'ms': 1e-3, 's': 1.0}
PUERTOS = set(range(5000, 5008))
COLUMNAS = [
    'escenario', 'cluster_speed', 'node_speed', 'velocidad', 'run', 'sim_time_s',
    'duracion_trafico_s', 'flujos_datos', 'flujos_aodv', 'flujos_otros',
    'tx_packets', 'rx_packets', 'lost_packets_flowmon', 'rx_bytes',
    'pdr_pct', 'retardo_ms', 'throughput_kbps', 'jitter_ms', 'pares_jitter', 'archivo_xml'
]


def segundos(valor: str) -> float:
    m = TIEMPO.fullmatch(valor.strip())
    if not m:
        raise ValueError(f'Formato de tiempo de FlowMonitor desconocido: {valor!r}')
    return float(m.group(1)) * UNIDADES[m.group(2)]


def leer_xml(ruta: Path, sim_time: float) -> dict:
    m = NOMBRE.fullmatch(ruta.name)
    if not m:
        raise ValueError(f'Nombre de XML inesperado: {ruta.name}')
    escenario, cs, ns, run = m.groups()
    cs, ns, run = int(cs), int(ns), int(run)
    if (escenario == 'E0' and (cs, ns) != (0, 0)) or \
       (escenario == 'E1' and (cs != 0 or ns not in (1, 5, 10, 20))) or \
       (escenario == 'E2' and (ns != 0 or cs not in (1, 5, 10, 20))) or \
       (escenario == 'E3' and (cs != ns or cs not in (1, 5, 10, 20))):
        raise ValueError(f'Configuración inconsistente con el diseño: {ruta.name}')

    root = ET.parse(ruta).getroot()
    stats_parent = root.find('FlowStats')
    classifier = root.find('Ipv4FlowClassifier')
    if stats_parent is None or classifier is None:
        raise ValueError(f'Faltan FlowStats o Ipv4FlowClassifier en {ruta.name}')

    # La correspondencia IP/puerto se obtiene EXCLUSIVAMENTE por flowId.
    por_id = {}
    for f in classifier.findall('Flow'):
        fid = f.get('flowId')
        if not fid or fid in por_id:
            raise ValueError(f'flowId inválido o duplicado en clasificador: {fid}')
        por_id[fid] = f

    puertos_vistos = []
    datos = []
    aodv = otros = 0
    for f in stats_parent.findall('Flow'):
        fid = f.get('flowId')
        clas = por_id.get(fid)
        if clas is None:
            raise ValueError(f'FlowStats flowId={fid} sin clasificador en {ruta.name}')
        protocolo = int(clas.get('protocol', '-1'))
        puerto = int(clas.get('destinationPort', '-1'))
        if protocolo == 17 and puerto == 654:
            aodv += 1
            continue
        if protocolo == 17 and puerto in PUERTOS:
            puertos_vistos.append(puerto)
            datos.append(f)
        else:
            otros += 1

    if sorted(puertos_vistos) != list(range(5000, 5008)):
        raise ValueError(
            f'{ruta.name}: se requieren EXACTAMENTE 8 flujos UDP, '
            f'uno por puerto 5000–5007; encontrados {sorted(puertos_vistos)}'
        )

    tx = rx = rx_bytes = lost = pares = 0
    delay = jitter = 0.0
    for f in datos:
        t = int(f.attrib['txPackets'])
        r = int(f.attrib['rxPackets'])
        b = int(f.attrib['rxBytes'])
        if t < 0 or r < 0 or r > t or b < 0:
            raise ValueError(f'{ruta.name}: contadores incoherentes para flowId={f.get("flowId")}')
        tx += t
        rx += r
        rx_bytes += b
        lost += int(f.get('lostPackets', '0'))
        delay += segundos(f.attrib['delaySum'])
        jitter += segundos(f.attrib['jitterSum'])
        pares += max(r - 1, 0)

    # Es el mismo intervalo que utiliza GuardarMetricas() del C++ (10–195 s).
    duracion = min(195.0, sim_time - 5.0) - 10.0
    if duracion <= 0:
        raise ValueError('La duración de tráfico es <= 0; revisa --sim-time')
    if tx == 0:
        raise ValueError(f'{ruta.name}: ningún paquete de datos transmitido')

    return {
        'escenario': escenario, 'cluster_speed': cs, 'node_speed': ns,
        'velocidad': 0 if escenario == 'E0' else ns if escenario == 'E1' else cs,
        'run': run, 'sim_time_s': sim_time, 'duracion_trafico_s': duracion,
        'flujos_datos': len(datos), 'flujos_aodv': aodv, 'flujos_otros': otros,
        'tx_packets': tx, 'rx_packets': rx, 'lost_packets_flowmon': lost,
        'rx_bytes': rx_bytes, 'pdr_pct': 100.0 * rx / tx,
        'retardo_ms': 1000.0 * delay / rx if rx else '',
        'throughput_kbps': 8.0 * rx_bytes / duracion / 1000.0,
        'jitter_ms': 1000.0 * jitter / pares if pares else '',
        'pares_jitter': pares, 'archivo_xml': ruta.name,
    }


def verificar_diseno(registros: list[dict], reps: int) -> None:
    esperados = {('E0', 0, 0, r) for r in range(1, reps + 1)}
    for e in ('E1', 'E2', 'E3'):
        for v in (1, 5, 10, 20):
            cs, ns = (0, v) if e == 'E1' else (v, 0) if e == 'E2' else (v, v)
            for r in range(1, reps + 1):
                esperados.add((e, cs, ns, r))
    presentes = {(f['escenario'], f['cluster_speed'], f['node_speed'], f['run']) for f in registros}
    faltantes, sobrantes = esperados - presentes, presentes - esperados
    if faltantes or sobrantes or len(presentes) != len(registros):
        raise ValueError(f'Experimentación incompleta o duplicada. Faltantes={sorted(faltantes)[:8]} '
                         f'(total {len(faltantes)}); sobrantes={sorted(sobrantes)[:8]} '
                         f'(total {len(sobrantes)}); duplicados={len(registros)-len(presentes)}')


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--raw', type=Path, default=REPO / 'resultados/raw', help='Carpeta de XML')
    ap.add_argument('--salida', type=Path, default=REPO / 'resultados/resumen.csv', help='CSV consolidado')
    ap.add_argument('--sim-time', type=float, default=200.0, help='Duración de cada simulación (s)')
    ap.add_argument('--completo', action='store_true', help='Exigir diseño íntegro de 13 configuraciones × N réplicas')
    ap.add_argument('--repeticiones', type=int, default=10)
    args = ap.parse_args()
    archivos = sorted(args.raw.glob('*.xml'))
    # NetAnim no es FlowMonitor; otros XML no se deben analizar silenciosamente.
    archivos = [p for p in archivos if not p.name.startswith('netanim_')]
    if not archivos:
        ap.error(f'No hay XML de FlowMonitor en {args.raw}')
    if args.repeticiones <= 0:
        ap.error('--repeticiones debe ser positivo')
    registros = []
    try:
        for ruta in archivos:
            registros.append(leer_xml(ruta, args.sim_time))
        if args.completo:
            verificar_diseno(registros, args.repeticiones)
        registros.sort(key=lambda f: (f['escenario'], f['velocidad'], f['run']))
        args.salida.parent.mkdir(parents=True, exist_ok=True)
        with args.salida.open('w', newline='', encoding='utf-8') as archivo:
            writer = csv.DictWriter(archivo, fieldnames=COLUMNAS)
            writer.writeheader()
            writer.writerows(registros)
    except (ValueError, ET.ParseError, OSError, KeyError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        return 1
    print(f'XML analizados: {len(registros)}. CSV: {args.salida}')
    print(f'Solo 8 puertos de datos 5000–5007 por corrida; AODV 654 excluido.')
    print(f'PDR agregado de todos los registros: {sum(x["rx_packets"] for x in registros)/sum(x["tx_packets"] for x in registros)*100:.2f}%')
    print(f'Flujos AODV excluidos (total): {sum(x["flujos_aodv"] for x in registros)}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
