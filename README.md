# Taller 01 — Simulación de MANETs jerárquicas en ns-3

**Curso:** Modelos estocásticos y simulación en computación y comunicaciones (2026-2S)
**Profesor:** Jorge Eduardo Ortiz Triviño
**Entrega:** miércoles 4 de noviembre de 2026, **11:59 pm**, respondiendo el correo del enunciado (solo al profesor, desde el correo institucional).

> Este README es el plan de trabajo del grupo. Cada integrante tiene una parte con tareas y archivos a su cargo. Si algo cambia (diseño, parámetros, reparto), **se actualiza aquí** para que todos trabajen sobre lo mismo.

---

## 1. Qué hay que entregar

Según el enunciado y la plantilla de "tarea automatizada" del curso, los entregables mínimos son:

1. **Pregunta de investigación** sobre modelos de movilidad, resuelta con el método científico (hipótesis → experimento → resultados → conclusión).
2. **Sistema MANET jerárquico** con al menos **3 clústeres en el primer nivel y 2 en el segundo**, con **movimiento a nivel de clúster y a nivel de nodo**.
3. **Implementación en ns-3** (ns3-ai es opcional; no lo usaremos).
4. **Informe** (`Entregable.pdf`) con: marco teórico, descripción y justificación del problema, diseño, código fuente, experimentación y análisis de **al menos 3 escenarios**, conclusiones y recomendaciones.
5. **Manual técnico y manual de usuario** (`Manuales.pdf`).
6. **Código fuente completo** con las versiones de software usadas.
7. **Correo de entrega** con los nombres completos de los 7 integrantes y el rol de cada uno.

---

## 2. Diseño propuesto (contrato común)

Estas decisiones permiten que el código, los scripts y el informe encajen entre sí. La **Parte 1** las revisa y las fija al comienzo; una vez fijadas, no se cambian sin avisar al grupo.

### 2.1 Pregunta de investigación (borrador)

> *En una MANET jerárquica de dos niveles con enrutamiento AODV, ¿qué degrada más el desempeño de la red —la movilidad a nivel de clúster o la movilidad a nivel de nodo— y cómo cambia ese efecto al aumentar la velocidad?*

- **Hipótesis (borrador):** la movilidad de clúster degrada más la tasa de entrega (PDR) y el retardo que la movilidad de nodo a la misma velocidad, porque rompe los enlaces entre *cluster heads* (la columna vertebral de la red), mientras que la movilidad de nodo solo altera rutas locales dentro del clúster.
- **Variables independientes:** tipo de movilidad (nodo / clúster / ambas) y velocidad.
- **Variables dependientes:** PDR, retardo extremo a extremo promedio, throughput y *jitter* (todas desde FlowMonitor).
- **Variables controladas:** número de nodos, área, protocolo (AODV), tráfico, potencia de transmisión, tiempo de simulación.

### 2.2 Topología jerárquica

- **Nivel 1 — 4 clústeres** (A, B, C, D) de **5 nodos** cada uno: 1 *cluster head* (CH) + 4 miembros → **20 nodos**.
- **Nivel 2 — 2 superclústeres**: **X = {A, B}** y **Y = {C, D}**.
- Cada clúster del nivel 1 tiene su propio canal WiFi ad hoc (un `YansWifiChannel` distinto por clúster).
- Los 4 CH tienen **una segunda interfaz** en un canal *backbone* compartido, que es donde se comunican entre clústeres.
- **Enrutamiento:** AODV en todos los nodos (soporta múltiples interfaces).
- **Direccionamiento:** una subred por clúster (`10.1.A.0/24`, …) y una para el backbone (`10.2.0.0/24`).
- **Tráfico:** flujos UDP `OnOff` entre miembros de **clústeres distintos**, incluidos pares que cruzan entre superclústeres X↔Y, para obligar a usar la jerarquía.

> Se usan 4 clústeres en el nivel 1 (el enunciado pide *al menos* 3) para que los 2 superclústeres queden simétricos.

### 2.3 Movilidad en tres capas (`HierarchicalMobilityModel`)

ns-3 permite anidar `HierarchicalMobilityModel` (el "padre" de un modelo puede ser otro modelo jerárquico):

```
Superclúster (X, Y)   → punto de referencia que se mueve (p. ej. RandomWaypoint en el área total)
   └── Clúster (A..D) → se mueve RELATIVO a su superclúster (movilidad de clúster)
         └── Nodo     → se mueve RELATIVO a su clúster (movilidad de nodo, p. ej. RandomWalk2d acotado)
```

- `clusterSpeed = 0` desactiva el movimiento de clúster; `nodeSpeed = 0` desactiva el de nodo.

### 2.4 Escenarios (mínimo 3, se piden al menos 3)

| Escenario | Movilidad de clúster | Movilidad de nodo | Propósito |
|---|---|---|---|
| E0 | No | No | Control (línea base) |
| E1 | No | Sí | Efecto aislado de la movilidad de nodo |
| E2 | Sí | No | Efecto aislado de la movilidad de clúster |
| E3 | Sí | Sí | Efecto combinado |

- Cada escenario con movimiento se corre para velocidades **v ∈ {1, 5, 10, 20} m/s**.
- **10 réplicas por configuración** (semillas distintas con `--RngRun`) → se reportan medias con **intervalos de confianza del 95 %** (t de Student).

### 2.5 Interfaz del programa (para que el código y los scripts encajen)

Archivo: `simulacion/manet-jerarquica.cc` (se copia a `scratch/` de ns-3).

```bash
./ns3 run "scratch/manet-jerarquica --scenario=E3 --clusterSpeed=5 --nodeSpeed=5 \
           --simTime=200 --RngRun=1 --outDir=resultados/raw"
```

| Parámetro | Tipo | Default | Descripción |
|---|---|---|---|
| `--scenario` | string | `E3` | Etiqueta del escenario (E0–E3) |
| `--clusterSpeed` | double | `5` | Velocidad de movilidad de clúster (m/s) |
| `--nodeSpeed` | double | `5` | Velocidad de movilidad de nodo (m/s) |
| `--simTime` | double | `200` | Duración de la simulación (s) |
| `--RngRun` | int | `1` | Réplica (semilla) |
| `--outDir` | string | `resultados/raw` | Carpeta de salida |
| `--netanim` | bool | `false` | Genera traza de NetAnim |

**Salida:** `resultados/raw/<scenario>_cs<clusterSpeed>_ns<nodeSpeed>_run<RngRun>.xml` (FlowMonitor).

### 2.6 Versiones

- **ns-3:** se acuerda **una sola versión** para todo el grupo (3.43-ai, la del video del profesor, o 3.46). Quien compile y corra los experimentos la deja escrita aquí: `ns-3 versión: ______`.
- **Sistema:** Ubuntu 22.04/24.04 o WSL2.
- **Python:** 3.10+ con `pandas`, `matplotlib`, `scipy`.

---

## 3. Estructura del repositorio

```
tallerme01/
├── README.md                    # este plan
├── Entregable.tex               # informe principal (incluye las secciones con \input)
├── secciones/                   # una sección por archivo → menos conflictos en git
│   ├── 01-problema.tex          # Parte 1
│   ├── 02-pregunta-hipotesis.tex# Parte 1
│   ├── 03-marco-teorico.tex     # Parte 6
│   ├── 04-diseno.tex            # Parte 5
│   ├── 05-codigo-fuente.tex     # Parte 3
│   ├── 06-experimentacion.tex   # Parte 7
│   └── 07-conclusiones.tex      # Parte 7
├── referencias.bib              # Parte 6
├── Manuales.tex                 # manual técnico (Parte 5) + manual de usuario (Parte 6)
├── simulacion/
│   └── manet-jerarquica.cc      # Partes 1, 2 y 3
├── scripts/
│   ├── correr_escenarios.sh     # Parte 4
│   ├── parse_flowmon.py         # Parte 4
│   └── graficar.py              # Parte 4
├── resultados/
│   ├── raw/                     # XML de FlowMonitor
│   ├── resumen.csv              # métricas por corrida
│   └── figuras/                 # gráficas para el informe
└── Materiales/                  # logos, diagramas
```

**Regla de oro:** cada quien edita **solo sus archivos**. Si necesitas cambiar algo de otra parte, avisa en el grupo.

---

## 4. Reparto del trabajo

El trabajo está organizado como una **cadena**: cada parte solo depende de las partes con número menor. Así nadie tiene que esperar a alguien que trabaja después que él, y cada quien sabe de quién recibe y a quién le entrega.

```
1 Diseño + red base ─► 2 Movilidad ─► 3 Tráfico y métricas ─► 4 Experimentos ─► 5 Diseño y manual técnico ─► 6 Marco teórico y manual de usuario ─► 7 Análisis y entrega
```

| # | Integrante | Parte | Recibe de | Entrega a |
|---|---|---|---|---|
| 1 | **David Fernando Benjumea Mora** | Formulación científica y red base en ns-3 | — | 2 |
| 2 | **José Esteban Alvarado Monroy** | Movilidad jerárquica | 1 | 3 |
| 3 | **Stephan Alain Roland García Martiquet** | Tráfico, métricas y validación del programa | 2 | 4, 5 |
| 4 | **Daniel Alonso Gracia Pinto** | Automatización de experimentos y gráficas | 3 | 5, 6, 7 |
| 5 | **Óscar Iván Ulises Gutiérrez Palacios** | LaTeX, diseño de la solución y manual técnico | 3, 4 | 6, 7 |
| 6 | **John Jairo Páez Albino** | Marco teórico, referencias y manual de usuario | 4, 5 | 7 |
| 7 | **Daniel Santiago Rodríguez Rodríguez** | Análisis, conclusiones, revisión final y entrega | 1, 4, 5, 6 | Profesor |

> Las partes 5 y 6 tienen trabajo que pueden adelantar desde el principio (estructura de LaTeX, marco teórico), y lo que depende de otros lo terminan cuando les llegue. Si alguien prefiere intercambiar su parte, se puede, siempre que se actualice esta tabla.

---

### Parte 1 — Formulación científica y red base en ns-3
**Responsable:** David Fernando Benjumea Mora
**Archivos:** `secciones/01-problema.tex`, `secciones/02-pregunta-hipotesis.tex`, `simulacion/manet-jerarquica.cc` (esqueleto y red), este `README.md`

No depende de nadie; es el punto de partida de toda la cadena.

- [ ] Fijar la pregunta de investigación, la hipótesis, las variables, los escenarios, las velocidades, el número de réplicas y los parámetros constantes (sección 2).
- [ ] Fijar la versión de ns-3 del grupo y escribirla en la sección 2.6.
- [ ] Crear `simulacion/manet-jerarquica.cc` con:
  - Todos los parámetros de la sección 2.5 (`CommandLine`).
  - Los 20 nodos agrupados en 4 clústeres y 2 superclústeres (`NodeContainer` por grupo, CH identificados).
  - WiFi ad hoc: un canal por clúster + un canal backbone para los 4 CH (802.11, `AdhocWifiMac`, `ConstantRateWifiManager`).
  - Pila de internet con **AODV** y direccionamiento por subredes (sección 2.2).
  - Una función vacía `ConfigurarMovilidad(...)` donde la Parte 2 conecta la movilidad (mientras tanto, posiciones fijas).
- [ ] Verificar que compila y corre en la versión acordada.
- [ ] Redactar **Descripción y justificación del problema** (por qué importa la movilidad en MANETs jerárquicas, conexión con el artículo de Ospina y Ortiz).
- [ ] Redactar **Pregunta de investigación e hipótesis**, explicando el método científico aplicado (observación → pregunta → hipótesis → experimento → análisis → conclusión).
- [ ] **Entrega a la Parte 2:** el `.cc` con la red funcionando y una nota de cómo está organizado.

### Parte 2 — Movilidad jerárquica
**Responsable:** José Esteban Alvarado Monroy
**Archivos:** `simulacion/manet-jerarquica.cc` (función `ConfigurarMovilidad`)
**Recibe de la Parte 1:** el `.cc` con nodos, clústeres y red.

- [ ] Implementar la movilidad en tres capas con `HierarchicalMobilityModel` anidado (sección 2.3).
- [ ] Movilidad de superclúster y de clúster con velocidad `clusterSpeed`; movilidad de nodo con `nodeSpeed`, acotada alrededor del centro de su clúster.
- [ ] Que `clusterSpeed=0` y `nodeSpeed=0` dejen quieta la capa correspondiente.
- [ ] Ajustar distancias (área, radio de clúster, separación entre clústeres) para que sin movimiento la red esté conectada.
- [ ] Prueba de validación: imprimir posiciones en el tiempo para mostrar que se mueven clústeres y nodos.
- [ ] Escribir una nota corta (en el código o en un `.md`) que explique la movilidad: la usarán las Partes 3 y 5.
- [ ] **Entrega a la Parte 3:** el `.cc` con red + movilidad funcionando.

### Parte 3 — Tráfico, métricas y validación del programa
**Responsable:** Stephan Alain Roland García Martiquet
**Archivos:** `simulacion/manet-jerarquica.cc` (tráfico, FlowMonitor, NetAnim), `secciones/05-codigo-fuente.tex`
**Recibe de la Parte 2:** el `.cc` con red y movilidad.

- [ ] Tráfico UDP `OnOff` entre miembros de clústeres distintos (incluir pares X↔Y), arrancando unos segundos después para que AODV se estabilice.
- [ ] Instalar **FlowMonitor** y guardar el XML con el nombre de la sección 2.5.
- [ ] Opcional: traza de **NetAnim** (`--netanim=true`) y una captura para el informe.
- [ ] Validar el programa completo con una corrida de cada escenario (E0–E3): que no falle, que genere el XML y que en E0 la PDR sea alta.
- [ ] Redactar la sección **Código fuente**: estructura del programa, funciones principales, fragmentos clave y versiones usadas.
- [ ] **Entrega a la Parte 4:** el programa final, listo para correr experimentos.

### Parte 4 — Automatización de experimentos y gráficas
**Responsable:** Daniel Alonso Gracia Pinto
**Archivos:** `scripts/correr_escenarios.sh`, `scripts/parse_flowmon.py`, `scripts/graficar.py`, `resultados/`
**Recibe de la Parte 3:** el programa final.

- [ ] `correr_escenarios.sh`: recorre escenarios × velocidades × réplicas (sección 2.4) y llama al programa.
- [ ] `parse_flowmon.py`: lee los XML de FlowMonitor y calcula por corrida PDR, retardo promedio, throughput y jitter → `resultados/resumen.csv`.
- [ ] `graficar.py`: medias con IC 95 % (t de Student) por escenario y velocidad; gráficas de PDR vs. velocidad y retardo vs. velocidad (una curva por escenario).
- [ ] Correr todos los experimentos y subir `resumen.csv`, una tabla resumen y las figuras a `resultados/`.
- [ ] **Entrega a las Partes 5, 6 y 7:** scripts, resultados y figuras en el repo, con una nota de cómo se corren.

### Parte 5 — LaTeX, diseño de la solución y manual técnico
**Responsable:** Óscar Iván Ulises Gutiérrez Palacios
**Archivos:** `Entregable.tex`, `Manuales.tex` (estructura + manual técnico), `secciones/04-diseno.tex`, `.github/workflows/`
**Recibe de las Partes 3 y 4:** el programa final, los scripts y su forma de uso.

- [ ] **Se puede adelantar desde el principio:** reorganizar `Entregable.tex` para que incluya `secciones/*.tex` con `\input`, con bibliografía (`referencias.bib`) y paquete para código (`listings` o `minted`). Quitar el texto de instrucciones de la plantilla, poner el título y los 7 autores.
- [ ] **Se puede adelantar:** crear `Manuales.tex` (portada + Parte I manual técnico + Parte II manual de usuario) y agregarlo al workflow de CI.
- [ ] Sección **Diseño de la solución**:
  - Diagrama de bloques de la arquitectura jerárquica (superclústeres, clústeres, CH, backbone).
  - Diagrama de flujo de la simulación (parámetros → nodos → red → movilidad → tráfico → ejecución → métricas).
  - Diagrama del flujo experimental (escenarios → réplicas → XML → CSV → gráficas).
- [ ] **Manual técnico**: requisitos, instalación de ns-3 (versión acordada), cómo compilar, estructura del código, descripción de cada parámetro, formato de salida, scripts, cómo extender (más clústeres, otro modelo de movilidad).
- [ ] **Entrega a las Partes 6 y 7:** LaTeX organizado y compilando, diseño y manual técnico listos.

### Parte 6 — Marco teórico, referencias y manual de usuario
**Responsable:** John Jairo Páez Albino
**Archivos:** `secciones/03-marco-teorico.tex`, `referencias.bib`, `Manuales.tex` (manual de usuario)
**Recibe de las Partes 4 y 5:** los scripts y su forma de uso, y la estructura de `Manuales.tex`.

- [ ] **Se puede adelantar desde el principio — Marco teórico:**
  - MANETs: definición, características y desafíos.
  - Arquitecturas jerárquicas y *clustering*: CH, miembros, gateways, escalabilidad.
  - Modelos de movilidad: entidad (Random Waypoint, Random Walk, Gauss-Markov) vs. grupo (RPGM); por qué la movilidad jerárquica representa movimiento de clúster y de nodo.
  - Protocolo AODV (RREQ, RREP, RERR).
  - Métricas: PDR, retardo, throughput, jitter.
  - ns-3 y los módulos usados (`wifi`, `aodv`, `mobility`, `flow-monitor`).
  - Resumen del artículo de Ospina y Ortiz como ejemplo de indagación en redes ad hoc jerárquicas.
- [ ] Bibliografía en `referencias.bib`: el libro *Wireless Network Simulation* (Zárate et al.), Sarkar et al., *Redes Ad Hoc*, el artículo de Ospina y Ortiz, Bunge (método científico) y la documentación de ns-3. Ver `Catalogo_referencias_usables.xlsx`.
- [ ] **Manual de usuario**: paso a paso para correr una simulación, correr todos los escenarios, generar las gráficas e interpretar la salida, con ejemplos de comandos y capturas.
- [ ] **Entrega a la Parte 7:** marco teórico, referencias y manual de usuario listos.

### Parte 7 — Análisis, conclusiones, revisión final y entrega
**Responsable:** Daniel Santiago Rodríguez Rodríguez
**Archivos:** `secciones/06-experimentacion.tex`, `secciones/07-conclusiones.tex`
**Recibe de:** la Parte 1 (pregunta e hipótesis), la Parte 4 (resultados y figuras) y las Partes 5 y 6 (informe y manuales).

Es la **última parte** de la cadena: recoge todo y termina con el envío.

- [ ] Sección **Experimentación y análisis de resultados**: descripción de cada escenario, tabla de parámetros, tablas con medias e IC 95 %, figuras de la Parte 4 e interpretación de E0–E3.
- [ ] Comparar E1 vs. E2 (la pregunta central) y discutir el efecto de la velocidad.
- [ ] **Conclusiones**: ¿se acepta o se rechaza la hipótesis de la Parte 1? ¿con qué evidencia?
- [ ] **Recomendaciones**: limitaciones del estudio y trabajo futuro (más nodos, otros protocolos como OLSR/DSDV, RPGM, ns3-ai).
- [ ] Revisión final del PDF completo (coherencia entre secciones, ortografía, figuras referenciadas) y verificar que `Entregable.pdf` y `Manuales.pdf` compilan.
- [ ] Redactar y enviar el **correo de entrega** (sección 6).

---

## 5. Flujo de trabajo con git

```bash
git clone https://github.com/10scar/tallerme01.git
cd tallerme01
git pull                       # SIEMPRE antes de empezar a editar
# ... editar solo tus archivos ...
git add <tus archivos>
git commit -m "Parte N: descripción corta"
git push
```

- Haz `git pull` antes de cada sesión de trabajo y commits pequeños y frecuentes.
- El PDF se compila solo en cada push a `main` (GitHub Actions) y queda en **Releases**. Si el workflow falla, revisa el log en la pestaña **Actions**.
- Las Partes 1, 2 y 3 editan el mismo `.cc` **en orden**: cada una hace `git pull` al recibirlo y avisa en el grupo cuando hace push de su versión terminada.

---

## 6. Correo de entrega (lo envía la Parte 7)

Responder **únicamente al profesor** el correo *"ME01: Enunciado y entrega del Taller No 01 Simulación de MANET"* desde el correo institucional, con:

- Nombres completos de los 7 integrantes y su rol (tabla de la sección 4).
- Adjuntos: `Entregable.pdf`, `Manuales.pdf`, código fuente (`simulacion/` y `scripts/` en un `.zip`) y el enlace al repositorio.
