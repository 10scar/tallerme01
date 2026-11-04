#!/usr/bin/env python3
"""Parte 4: gráficos e IC95% para E0/E1/E2/E3; contraste unilateral Welch E2 vs E1.

Requiere: pandas, matplotlib, scipy, numpy.
Solo utiliza los flujos 5000–5007 ya filtrados por parse_flowmon.py.
"""
from __future__ import annotations

import argparse
import math
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from scipy import stats

REPO = Path(__file__).resolve().parent.parent
VELOCIDADES = [1, 5, 10, 20]
ESCENARIOS = {'E0': 'E0 · Sin movilidad', 'E1': 'E1 · Solo nodos',
              'E2': 'E2 · Solo clústeres', 'E3': 'E3 · Ambas capas'}
METRICAS = [
    ('pdr_pct', 'PDR (%)', '01_pdr_vs_velocidad.png'),
    ('retardo_ms', 'Retardo extremo a extremo (ms)', '02_retardo_vs_velocidad.png'),
    ('throughput_kbps', 'Throughput IP (kbit/s)', '03_throughput_vs_velocidad.png'),
    ('jitter_ms', 'Jitter medio (ms)', '04_jitter_vs_velocidad.png'),
]


def resumen_ic(serie: pd.Series) -> tuple[int, float, float, float, float]:
    v = pd.to_numeric(serie, errors='coerce').dropna().astype(float)
    n = len(v)
    if n == 0:
        return 0, math.nan, math.nan, math.nan, math.nan
    media = float(v.mean())
    if n == 1:
        return 1, media, math.nan, math.nan, math.nan
    std = float(v.std(ddof=1))
    margen = float(stats.t.ppf(0.975, df=n - 1) * std / math.sqrt(n))
    return n, media, std, media - margen, media + margen


def estadisticas_agrupadas(datos: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for (e, v), gr in datos.groupby(['escenario', 'velocidad'], sort=True):
        fila = {'escenario': e, 'velocidad': int(v), 'repeticiones': len(gr),
                'corridas_sin_rx': int((gr.rx_packets == 0).sum())}
        for key, _, _ in METRICAS:
            n, m, sd, lo, hi = resumen_ic(gr[key])
            fila.update({f'{key}_n': n, f'{key}_media': m,
                         f'{key}_sd': sd, f'{key}_ic95_inf': lo,
                         f'{key}_ic95_sup': hi})
        rows.append(fila)
    return pd.DataFrame(rows).sort_values(['escenario', 'velocidad'])


def dibujar_metrica(datos: pd.DataFrame, key: str, etiqueta: str, ruta: Path):
    fig, ax = plt.subplots(figsize=(9, 5.5))
    fig.subplots_adjust(left=0.10, right=0.97, top=0.90, bottom=0.22)
    # Una única línea base E0: no repetirla artificialmente por cuatro velocidades.
    e0 = datos.loc[datos.escenario == 'E0', key]
    n0, m0, _, lo0, hi0 = resumen_ic(e0)
    if n0:
        ax.axhline(m0, linestyle='--', linewidth=1.7, label='E0 · Control estático')
        if n0 >= 2 and np.isfinite([lo0, hi0]).all():
            ax.axhspan(lo0, hi0, alpha=0.12)
    for e in ('E1', 'E2', 'E3'):
        sub = datos.loc[datos.escenario == e]
        x, y, lower, upper = [], [], [], []
        for v in VELOCIDADES:
            n, media, _, lo, hi = resumen_ic(sub.loc[sub.velocidad == v, key])
            x.append(v)
            y.append(media)
            lower.append(max(0.0, media - lo) if n >= 2 else 0.0)
            upper.append(max(0.0, hi - media) if n >= 2 else 0.0)
        ax.errorbar(x, y, yerr=[lower, upper], marker='o', linewidth=1.8,
                    capsize=3, label=ESCENARIOS[e])
    ax.set_title(f'{etiqueta} según velocidad y tipo de movilidad')
    ax.set_xlabel('Velocidad configurada (m/s)')
    ax.set_ylabel(etiqueta)
    ax.set_xticks(VELOCIDADES)
    ax.grid(alpha=0.22)
    if key == 'pdr_pct':
        ax.set_ylim(0, 105)
    ax.legend(loc='best', fontsize=9)
    fig.text(0.50, 0.065, 'Barras: IC 95 % t de Student; retardo/jitter solo en corridas con valores definidos.',
             ha='center', fontsize=8)
    fig.savefig(ruta, dpi=180, bbox_inches='tight')
    plt.close(fig)


def graficar_sin_recepcion(datos: pd.DataFrame, ruta: Path):
    fig, ax = plt.subplots(figsize=(9, 5.5), constrained_layout=True)
    for e in ('E1', 'E2', 'E3'):
        sub = datos.loc[datos.escenario == e]
        ys = []
        for v in VELOCIDADES:
            gr = sub.loc[sub.velocidad == v]
            ys.append(float((gr.rx_packets == 0).mean() * 100) if len(gr) else math.nan)
        ax.plot(VELOCIDADES, ys, marker='o', label=ESCENARIOS[e])
    ax.set_title('Corridas sin paquetes entregados (8 flujos agregados)')
    ax.set_xlabel('Velocidad configurada (m/s)')
    ax.set_ylabel('Corridas sin recepción (%)')
    ax.set_xticks(VELOCIDADES)
    ax.set_ylim(0, 105)
    ax.grid(alpha=0.22)
    ax.legend()
    fig.savefig(ruta, dpi=180, bbox_inches='tight')
    plt.close(fig)


def welch_unilateral(datos: pd.DataFrame) -> pd.DataFrame:
    out = []
    for v in VELOCIDADES:
        a = datos.loc[(datos.escenario == 'E1') & (datos.velocidad == v)]
        b = datos.loc[(datos.escenario == 'E2') & (datos.velocidad == v)]
        for key, sentido, etiqueta in (
            ('pdr_pct', 'less', 'PDR: E2 < E1'),
            ('retardo_ms', 'greater', 'Retardo: E2 > E1'),
        ):
            x = pd.to_numeric(a[key], errors='coerce').dropna().to_numpy(dtype=float)
            y = pd.to_numeric(b[key], errors='coerce').dropna().to_numpy(dtype=float)
            p = math.nan
            t = math.nan
            if len(x) >= 2 and len(y) >= 2:
                with np.errstate(all='ignore'):
                    prueba = stats.ttest_ind(y, x, equal_var=False, alternative=sentido)
                    p, t = float(prueba.pvalue), float(prueba.statistic)
            out.append({'velocidad': v, 'hipotesis_unilateral': etiqueta,
                        'n_E1': len(x), 'n_E2': len(y),
                        'media_E1': np.mean(x) if len(x) else math.nan,
                        'media_E2': np.mean(y) if len(y) else math.nan,
                        'diferencia_E2_menos_E1': np.mean(y) - np.mean(x) if len(x) and len(y) else math.nan,
                        'estadistico_t': t, 'p_unilateral': p,
                        'rechaza_H0_alpha_0_05': bool(p < 0.05) if math.isfinite(p) else '',
                        'observacion': 'Retardo condicionado a recepción' if key == 'retardo_ms' else ''})
    return pd.DataFrame(out)


def tabla_markdown(df: pd.DataFrame, cols: list[str]) -> str:
    cab = '| ' + ' | '.join(cols) + ' |'
    separador = '| ' + ' | '.join(['---'] * len(cols)) + ' |'
    lines = [cab, separador]
    for _, f in df.iterrows():
        values = []
        for c in cols:
            z = f[c]
            values.append('—' if pd.isna(z) else f'{z:.2f}' if isinstance(z, (float, np.floating)) else str(z))
        lines.append('| ' + ' | '.join(values) + ' |')
    return '\n'.join(lines)


def generar_resumen_md(tabla: pd.DataFrame, resultado: Path):
    t = tabla[['escenario', 'velocidad', 'pdr_pct_media', 'pdr_pct_ic95_inf',
               'pdr_pct_ic95_sup', 'retardo_ms_media', 'retardo_ms_n', 'corridas_sin_rx']]
    texto = '''# Parte 4 — Resultados de simulación MANET

> **Generado automáticamente desde `resultados/resumen.csv`.** No es una conclusión estadística definitiva.

Se analizaron exclusivamente los **8 flujos UDP de datos** con puertos de destino **5000–5007**. Los flujos de control AODV (puerto **654**) se excluyeron de PDR, retardo, throughput y jitter. Por esa razón los valores **no deben coincidir** con el resumen global que imprime el programa ns-3 en consola.

## Figuras

![PDR frente a velocidad](figuras/01_pdr_vs_velocidad.png)

![Retardo frente a velocidad](figuras/02_retardo_vs_velocidad.png)

![Throughput](figuras/03_throughput_vs_velocidad.png)

![Jitter](figuras/04_jitter_vs_velocidad.png)

![Corridas sin paquetes recibidos](figuras/05_corridas_sin_recepcion.png)

## Tabla de resultados agregados

Cada fila corresponde a un escenario y velocidad. Los intervalos de confianza usan t de Student para las réplicas disponibles.

'''
    texto += tabla_markdown(t, list(t.columns))
    texto += '''

## Contrastes estadísticos

Consultar `comparacion_E2_vs_E1.csv`: prueba t de Welch **unilateral** por velocidad; E2 tiene menor PDR o mayor retardo que E1. α = 0.05, sin ajuste por comparaciones múltiples. Los valores de retardo se calculan únicamente en corridas con paquetes recibidos.

## Limitaciones a documentar

- Los clústeres utilizan **Random Waypoint con pausa fija de 2 segundos** al llegar a un destino. Según la validación del grupo, las pausas representan aproximadamente **4 % a 1 m/s y 21 % a 20 m/s**; por tanto, la velocidad configurada no equivale a la velocidad media efectiva.
- Las métricas son agregadas para ocho pares de comunicación. No deben compararse con conteos globales que incluyan tráfico AODV.
- Si ningún paquete llega, el **retardo y jitter no son calculables** (celdas vacías), no cero.
- El throughput de los 8 flujos usa bytes IP (`rxBytes`) y un intervalo común de **185 s** (desde 10 hasta 195 s para simulaciones de 200 s).
- Los resultados dependen de la geometría, la propagación, los flujos y el protocolo AODV elegidos. No generalizar a todas las MANET.
'''
    resultado.write_text(texto, encoding='utf-8')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--csv', type=Path, default=REPO / 'resultados/resumen.csv')
    ap.add_argument('--figuras', type=Path, default=REPO / 'resultados/figuras')
    ap.add_argument('--resultados', type=Path, default=REPO / 'resultados')
    args = ap.parse_args()
    if not args.csv.exists():
        ap.error(f'No existe el CSV {args.csv}; ejecuta primero parse_flowmon.py')
    datos = pd.read_csv(args.csv)
    requeridas = {'escenario', 'velocidad', 'run', 'flujos_datos', 'tx_packets', 'rx_packets'} | {m[0] for m in METRICAS}
    if faltan := (requeridas - set(datos.columns)):
        ap.error(f'Faltan columnas del CSV: {sorted(faltan)}')
    if datos.empty or (datos.flujos_datos != 8).any() or (datos.tx_packets <= 0).any():
        ap.error('CSV vacío, sin datos o con número de flujos/txPackets inválido')
    if datos.duplicated(['escenario', 'velocidad', 'run']).any():
        ap.error('Hay combinaciones escenario/velocidad/run duplicadas')
    args.figuras.mkdir(parents=True, exist_ok=True)
    args.resultados.mkdir(parents=True, exist_ok=True)

    tabla = estadisticas_agrupadas(datos)
    tabla.to_csv(args.resultados / 'tabla_estadistica.csv', index=False, float_format='%.8f')
    for key, label, nombre in METRICAS:
        dibujar_metrica(datos, key, label, args.figuras / nombre)
    graficar_sin_recepcion(datos, args.figuras / '05_corridas_sin_recepcion.png')
    welch = welch_unilateral(datos)
    welch.to_csv(args.resultados / 'comparacion_E2_vs_E1.csv', index=False, float_format='%.8f')
    generar_resumen_md(tabla, args.resultados / 'RESULTADOS.md')
    print(f'Figuras: {args.figuras} (5 PNG)')
    print(f'Tabla estadística: {args.resultados / "tabla_estadistica.csv"}')
    print(f'Comparación Welch: {args.resultados / "comparacion_E2_vs_E1.csv"}')
    print(f'Resumen Markdown: {args.resultados / "RESULTADOS.md"}')


if __name__ == '__main__':
    main()
