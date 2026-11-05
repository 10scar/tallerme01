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
7. **Correo de entrega** con los nombres completos de los 6 integrantes y el rol de cada uno.

---

## 2. Diseño propuesto (contrato común)

Estas decisiones permiten que el código, los scripts y el informe encajen entre sí. **Estado: fijado por la Parte 1.** No se cambia sin avisar al grupo. El detalle formal está en `secciones/02-pregunta-hipotesis.tex`.

### 2.1 Pregunta de investigación e hipótesis

> *En una MANET jerárquica de dos niveles con enrutamiento AODV, ¿qué degrada más el desempeño de la red —la movilidad a nivel de clúster o la movilidad a nivel de nodo— y cómo cambia ese efecto al aumentar la velocidad?*

- **Hipótesis principal:** la movilidad de clúster degrada más la tasa de entrega (PDR) y el retardo que la movilidad de nodo a la misma velocidad, porque rompe los enlaces entre *cluster heads* (la columna vertebral de la red), mientras que la movilidad de nodo solo altera rutas locales dentro del clúster.
- **Formalmente**, para cada velocidad v: H₀: PDR(E2) = PDR(E1) contra H₁: PDR(E2) < PDR(E1); y H₀: retardo(E2) = retardo(E1) contra H₁: retardo(E2) > retardo(E1).
- **Hipótesis secundarias:** el desempeño empeora con la velocidad (HS1); E3 es igual o peor que el peor de E1 y E2 (HS2); la diferencia entre E1 y E2 crece con la velocidad (HS3).
- **Variables independientes:** tipo de movilidad (ninguna / nodo / clúster / ambas) y velocidad.
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
Superclúster (X, Y)   → punto de referencia FIJO en los experimentos (se puede mover con --superSpeed)
   └── Clúster (A..D) → RandomWaypoint relativo a su superclúster, región ±150 m, velocidad clusterSpeed, pausa 2 s
         └── Nodo     → RandomWalk2d relativo a su clúster, región ±40 m, velocidad nodeSpeed
```

- `clusterSpeed = 0` desactiva el movimiento de clúster; `nodeSpeed = 0` desactiva el de nodo.
- **Por qué los superclústeres quedan fijos:** para que E1 y E2 sean comparables, en cada uno se mueve **una sola capa** a la misma velocidad v. El programa igual debe permitir mover los superclústeres (`--superSpeed`, por defecto 0).

### 2.4 Escenarios (mínimo 3, se piden al menos 3)

| Escenario | Movilidad de clúster | Movilidad de nodo | Propósito |
|---|---|---|---|
| E0 | No | No | Control (línea base) |
| E1 | No | Sí | Efecto aislado de la movilidad de nodo |
| E2 | Sí | No | Efecto aislado de la movilidad de clúster |
| E3 | Sí | Sí | Efecto combinado |

- Cada escenario con movimiento se corre para velocidades **v ∈ {1, 5, 10, 20} m/s**.
- E0 no tiene movimiento, así que se corre una sola vez (sin barrido de velocidades).
- **10 réplicas por configuración** (semillas distintas con `--RngRun`): (3 × 4 + 1) × 10 = **130 corridas**.
- Análisis: medias con **intervalos de confianza del 95 %** (t de Student, t₀.₉₇₅,₉ ≈ 2.262) y **prueba t de Welch unilateral** E2 vs. E1 por velocidad con α = 0.05.
- **Regla de decisión:** la hipótesis principal se respalda si se rechaza H₀ en al menos 3 de las 4 velocidades.

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
| `--superSpeed` | double | `0` | Velocidad de los superclústeres (m/s); 0 en todos los experimentos |
| `--simTime` | double | `200` | Duración de la simulación (s) |
| `--RngRun` | int | `1` | Réplica (semilla) |
| `--outDir` | string | `resultados/raw` | Carpeta de salida |
| `--netanim` | bool | `false` | Genera traza de NetAnim |
| `--verificar` | bool | `false` | Prueba de conectividad de la Parte 1 (UDP Echo entre clústeres) |
| `--rangoCluster` | double | `60` | Alcance efectivo dentro del clúster (m) |
| `--rangoBackbone` | double | `250` | Alcance efectivo del backbone entre CH (m) |

**Salida:** `resultados/raw/<scenario>_cs<clusterSpeed>_ns<nodeSpeed>_run<RngRun>.xml` (FlowMonitor).

### 2.6 Versiones

- **ns-3: versión 3.43** (`ns-3 versión: 3.43`). Es la versión de los videos de instalación del profesor, así que si alguien tiene problemas puede seguir esa guía. **No** se instala ns3-ai: el correo lo recomienda, pero es opcional y el código no lo usa. Todo el grupo usa esta misma versión.

  Instalación probada (Windows + WSL). En PowerShell como administrador: `wsl --install -d Ubuntu-22.04`. Luego, en Ubuntu:
  ```bash
  sudo apt update && sudo apt install -y g++ python3 python3-pip cmake ninja-build git wget bzip2
  cd ~ && wget https://www.nsnam.org/releases/ns-allinone-3.43.tar.bz2 && tar xjf ns-allinone-3.43.tar.bz2
  cd ns-allinone-3.43/ns-3.43
  ./ns3 configure --build-profile=optimized --enable-modules="wifi;aodv;mobility;internet;applications;flow-monitor;netanim"
  ./ns3 build
  ```
  Trabajar siempre en `~` (disco de Linux), no en `/mnt/c/...`: compilar sobre el disco de Windows es mucho más lento.
- **Sistema:** Ubuntu 22.04 (nativo o WSL2 en Windows).
- **Compilador:** el que trae Ubuntu 22.04 (g++ 11), con CMake y el script `./ns3` de ns-3.
- **Python:** 3.10+ con `pandas`, `matplotlib`, `scipy`.

### 2.7 Parámetros constantes

| Parámetro | Valor |
|---|---|
| Nodos | 20: 4 clústeres × 5 (1 CH fijo + 4 miembros) |
| WiFi | 802.11b ad hoc, tasa fija DSSS 1 Mbps (`ConstantRateWifiManager`), potencia por defecto |
| Propagación | Modelo de disco (`RangePropagationLossModel`) + retardo de velocidad constante. Dentro del alcance se recibe todo; fuera, nada |
| Alcance efectivo | **60 m** dentro de cada clúster (`--rangoCluster`) y **250 m** en el backbone (`--rangoBackbone`) |
| Canales | 1 `YansWifiChannel` por clúster + 1 backbone para los 4 CH |
| Enrutamiento | AODV con parámetros por defecto |
| Tráfico | 8 flujos UDP `OnOff` CBR, 512 B, 4 paquetes/s (16 kbit/s cada uno) |
| Pares (miembro → miembro) | A→B, B→A, C→D, D→C (dentro de superclúster); A→C, B→D, C→A, D→B (entre superclústeres) |
| Tiempos | Simulación 200 s; tráfico desde un instante uniforme en [10, 11] s hasta 195 s |
| Posiciones iniciales | Superclústeres en (350, 300) y (650, 300) m; clústeres en x = 275, 425, 575, 725 m sobre y = 300 m; CH en el centro del clúster y los 4 miembros en un anillo de 25 m de radio |

> Los valores geométricos son **de partida**. Si en E0 la PDR da menos del 95 %, la Parte 2 los ajusta para que la red quede conectada y documenta el cambio aquí.

**Métricas** (sumando sobre todos los flujos de una corrida, con los campos del XML de FlowMonitor):
- PDR = Σ rxPackets / Σ txPackets
- Retardo medio = Σ delaySum / Σ rxPackets
- Throughput = 8 · Σ rxBytes / (fin − inicio del tráfico)
- Jitter medio = Σ jitterSum / Σ (rxPackets − 1)

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
| 2 | **Daniel Alonso Gracia Pinto** | Movilidad jerárquica | 1 | 3 |
| 3 | **Stephan Alain Roland García Martiquet** | Tráfico, métricas y validación del programa | 2 | 4, 5 |
| 4 | **José Esteban Alvarado Monroy** | Automatización de experimentos y gráficas | 3 | 5, 6, 7 |
| 5 | **Óscar Iván Ulises Gutiérrez Palacios** | LaTeX, diseño de la solución y manual técnico | 3, 4 | 6, 7 |
| 6 | **John Jairo Páez Albino** | Marco teórico, referencias y manual de usuario | 4, 5 | 7 |
| 7 | **Daniel Alonso Gracia Pinto** (análisis y conclusiones) · **Óscar Iván Ulises Gutiérrez Palacios** (revisión final y entrega) | Análisis, conclusiones, revisión final y entrega | 1, 4, 5, 6 | Profesor |

> **Cambio de integrantes (4 nov 2026):** Daniel Santiago Rodríguez Rodríguez, inicialmente a cargo de la Parte 7, salió del grupo por no reportarse. El grupo queda con **6 integrantes**: el análisis y las conclusiones los asumió Daniel Gracia, y la revisión final y el correo de entrega, Óscar Gutiérrez.

> Las partes 5 y 6 tienen trabajo que pueden adelantar desde el principio (estructura de LaTeX, marco teórico), y lo que depende de otros lo terminan cuando les llegue. Si alguien prefiere intercambiar su parte, se puede, siempre que se actualice esta tabla.

---

### Parte 1 — Formulación científica y red base en ns-3
**Responsable:** David Fernando Benjumea Mora
**Archivos:** `secciones/01-problema.tex`, `secciones/02-pregunta-hipotesis.tex`, `simulacion/manet-jerarquica.cc` (esqueleto y red), este `README.md`

> `referencias.bib` ya tiene las entradas que citan las secciones de la Parte 1 (es de la Parte 6, que lo completa sin cambiar las claves).

No depende de nadie; es el punto de partida de toda la cadena.

- [x] Fijar la pregunta de investigación, la hipótesis, las variables, los escenarios, las velocidades, el número de réplicas y los parámetros constantes (sección 2).
- [x] Fijar la versión de ns-3 del grupo y escribirla en la sección 2.6.
- [x] Crear `simulacion/manet-jerarquica.cc` con:
  - Todos los parámetros de la sección 2.5 (`CommandLine`).
  - Los 20 nodos agrupados en 4 clústeres y 2 superclústeres (`NodeContainer` por grupo, CH identificados).
  - WiFi ad hoc: un canal por clúster + un canal backbone para los 4 CH (802.11, `AdhocWifiMac`, `ConstantRateWifiManager`).
  - Pila de internet con **AODV** y direccionamiento por subredes (sección 2.2).
  - Una función vacía `ConfigurarMovilidad(...)` donde la Parte 2 conecta la movilidad (mientras tanto, posiciones fijas).
- [x] Verificar que compila y corre en la versión acordada (prueba de conectividad: 120/120 paquetes).
- [x] Redactar **Descripción y justificación del problema** (por qué importa la movilidad en MANETs jerárquicas, conexión con el artículo de Ospina y Ortiz).
- [x] Redactar **Pregunta de investigación e hipótesis**, explicando el método científico aplicado (observación → pregunta → hipótesis → experimento → análisis → conclusión).
- [x] **Entrega a la Parte 2:** el `.cc` con la red funcionando y una nota de cómo está organizado (abajo).

#### Nota de entrega de la Parte 1 → Parte 2

**Cómo correrlo** (desde `~/ns-allinone-3.43/ns-3.43` en Ubuntu/WSL):
```bash
cp "/mnt/c/Users/<tu-usuario>/<ruta-al-repo>/simulacion/manet-jerarquica.cc" scratch/
./ns3 run "scratch/manet-jerarquica --verificar=true --simTime=30"
```
Debe terminar con `Total: 120/120 (100.0 %)` y `RESULTADO: la red jerárquica está conectada.`

> Si ns-3 se configuró sin ejemplos, `./ns3 run hello-simulator` no existe; no es un error de instalación.

**Organización del código:**
- `struct Parametros`: todos los parámetros (los de línea de comandos y los geométricos).
- `struct RedJerarquica`: lo que reciben las Partes 2 y 3.
  - `red.clusters[k]`: nodos del clúster k (A=0 … D=3); el **índice 0 es el CH** y 1–4 son los miembros.
  - `red.superclusters[s]`: nodos del superclúster s (X=0, Y=1).
  - `red.clusterHeads`, `red.miembros`, `red.todos`.
  - `red.ifCluster[k].GetAddress(i)`: IP del nodo i del clúster k; `red.ifBackbone.GetAddress(k)`: IP de backbone del CH k.
- `CrearNodos()` y `ConfigurarRed()` (Parte 1): **no hay que tocarlas**.
- `ConfigurarMovilidad()` (**Parte 2**): hoy pone posiciones fijas (CH en el centro del clúster, miembros en un anillo de 25 m). Hay que reemplazarla por la movilidad jerárquica de la sección 2.3, usando `p.clusterSpeed`, `p.nodeSpeed` y `p.superSpeed`.
- `ConfigurarTrafico()`, `ConfigurarMetricas()`, `GuardarMetricas()` (**Parte 3**): vacías, con la especificación en el comentario.
- `--verificar=true`: prueba de conectividad (UDP Echo entre el miembro 1 de cada clúster). Sirve para comprobar que la movilidad no rompe la red en E0.

**Cosas a tener en cuenta:**
- La propagación es un **modelo de disco**: alcance exacto de 60 m dentro del clúster y 250 m en el backbone. Se cambia con `--rangoCluster` y `--rangoBackbone`.
- Los canales de los clústeres son **independientes**: un miembro solo puede hablar con nodos de su clúster, y lo que sale del clúster pasa por su CH y el backbone. Se comprobó: con `--rangoBackbone=100` (menos que los 150 m entre clústeres) la conectividad cae a 0/120.
- Con movilidad de nodo, un miembro puede quedar a más de 60 m de su CH. AODV puede rodear ese corte usando otros miembros del mismo clúster como intermediarios.

### Parte 2 — Movilidad jerárquica
**Responsable:** Daniel Alonso Gracia Pinto
**Archivos:** `simulacion/manet-jerarquica.cc` (función `ConfigurarMovilidad`)
**Recibe de la Parte 1:** el `.cc` con nodos, clústeres y red.

- [x] Implementar la movilidad en tres capas con `HierarchicalMobilityModel` anidado (sección 2.3).
- [x] Movilidad de superclúster y de clúster con velocidad `clusterSpeed`; movilidad de nodo con `nodeSpeed`, acotada alrededor del centro de su clúster.
- [x] Que `clusterSpeed=0` y `nodeSpeed=0` dejen quieta la capa correspondiente.
- [x] Ajustar distancias (área, radio de clúster, separación entre clústeres) para que sin movimiento la red esté conectada. *No hizo falta cambiarlas: con la geometría de la sección 2.7, E0 entrega 120/120 en `--verificar` y 5901/5901 paquetes de datos.*
- [x] Prueba de validación: imprimir posiciones en el tiempo para mostrar que se mueven clústeres y nodos. *`--trazaPosiciones=T` las imprime y las guarda en CSV; `scripts/validar_movilidad.py` las revisa automáticamente (46/46 pruebas).*
- [x] Escribir una nota corta (en el código o en un `.md`) que explique la movilidad: la usarán las Partes 3 y 5. *Comentario sobre `ConfigurarMovilidad()` en el `.cc`.*
- [x] **Entrega a la Parte 3:** el `.cc` con red + movilidad funcionando. *Rama `parte2-movilidad`, construida sobre la rama `P3`.*

### Parte 3 — Tráfico, métricas y validación del programa
**Responsable:** Stephan Alain Roland García Martiquet
**Archivos:** `simulacion/manet-jerarquica.cc` (tráfico, FlowMonitor, NetAnim), `secciones/05-codigo-fuente.tex`
**Recibe de la Parte 2:** el `.cc` con red y movilidad.

- [x] Tráfico UDP `OnOff` entre miembros de clústeres distintos (incluir pares X↔Y), arrancando unos segundos después para que AODV se estabilice.
- [x] Instalar **FlowMonitor** y guardar el XML con el nombre de la sección 2.5.
- [x] Opcional: traza de **NetAnim** (`--netanim=true`) y una captura para el informe.
- [x] Validar el programa completo con una corrida de cada escenario (E0–E3): que no falle, que genere el XML y que en E0 la PDR sea alta.
- [x] Redactar la sección **Código fuente**: estructura del programa, funciones principales, fragmentos clave y versiones usadas.
- [x] **Entrega a la Parte 4:** el programa final, listo para correr experimentos.

#### Nota de entrega de la Parte 3 → Parte 4 (José)

**El programa ya está listo para tus scripts de simulación y gráficas.**

1. **Cómo correr una simulación individual:**
   ```bash
   cp "simulacion/manet-jerarquica.cc" scratch/
   ./ns3 run "scratch/manet-jerarquica --scenario=E3 --clusterSpeed=5 --nodeSpeed=5 --simTime=200 --RngRun=1 --outDir=resultados/raw"
   ```
2. **Archivos generados:**
   - FlowMonitor exporta automáticamente a:
     `resultados/raw/<scenario>_cs<clusterSpeed>_ns<nodeSpeed>_run<RngRun>.xml`
   - La carpeta `resultados/raw/` se crea automáticamente si no existe.
   - Si activas `--netanim=true`, genera además `resultados/raw/netanim_<scenario>_cs<clusterSpeed>_ns<nodeSpeed>_run<RngRun>.xml` con los CH resaltados en rojo y miembros en azul.
3. **Resumen en consola:**
   Al terminar cada ejecución, el programa imprime en terminal un bloque con el conteo de flujos, paquetes TX/RX, PDR (\%), retardo medio (s), throughput (kbps) y jitter (s). Te sirve para validar rápidamente que tu script `parse_flowmon.py` esté extrayendo exactamente los mismos valores numéricos desde los XML.
4. **Tráfico configurado:**
   - 8 flujos UDP OnOff CBR de 512 bytes @ 4 paq/s (16 kbps).
   - Pares intra-superclúster (miembros 1): A1↔B1, C1↔D1.
   - Pares inter-superclúster (miembros 2): A2→C2, B2→D2, C2→A2, D2→B2.
   - Los flujos inician con desfase uniforme entre 10.0 y 11.0 s y finalizan a los 195 s.
5. **Sección de informe lista:**
   Se redactó `secciones/05-codigo-fuente.tex` con todo el detalle de software, arquitectura del código, ecuaciones de cálculo, fragmentos clave y la validación en E0. Óscar (Parte 5) solo debe incluirla en `Entregable.tex` con `\input{secciones/05-codigo-fuente.tex}`.

### Parte 4 — Automatización de experimentos y gráficas

**Responsable:** José Esteban Alvarado Monroy  
**Estado:** Completada  
**Archivos:** `scripts/correr_escenarios.sh`, `scripts/parse_flowmon.py`, `scripts/graficar.py`, `resultados/`  
**Recibe de la Parte 3:** el programa final.

- [x] `correr_escenarios.sh`: automatización de los cuatro escenarios (E0, E1, E2 y E3), cuatro velocidades (1, 5, 10 y 20 m/s) y diez réplicas por configuración.
- [x] `parse_flowmon.py`: procesamiento de los archivos XML de FlowMonitor y cálculo de PDR, retardo promedio, throughput y jitter. Se filtran exclusivamente los ocho flujos de datos (puertos 5000–5007), excluyendo el tráfico de control AODV (puerto 654).
- [x] `graficar.py`: generación de tablas estadísticas, medias e intervalos de confianza del 95 %, gráficas comparativas y pruebas t de Welch unilaterales entre E2 y E1.
- [x] Ejecución de las 130 simulaciones correspondientes a las 13 configuraciones experimentales con diez réplicas cada una.
- [x] Generación y validación de `resultados/resumen.csv`, `resultados/tabla_estadistica.csv`, `resultados/comparacion_E2_vs_E1.csv`, `resultados/RESULTADOS.md` y cinco gráficas PNG.
- [x] **Entrega a las Partes 5, 6 y 7:** scripts, resultados y figuras disponibles en la rama `P4`, junto con instrucciones de ejecución.

**Resultados principales:**

- 130 simulaciones completadas y procesadas correctamente.
- 1.040 flujos de datos analizados, excluyendo 4.097 flujos de control AODV.
- PDR agregado de 83,57 %.
- Comparaciones estadísticas E2 vs. E1 calculadas para las cuatro velocidades.
- Cinco figuras generadas para su incorporación al informe.

**Limitación metodológica:** el modelo Random Waypoint introduce pausas fijas de 2 segundos al alcanzar cada destino. Por ello, la proporción de tiempo en pausa puede aumentar con la velocidad configurada.

**Nota:** las interpretaciones científicas definitivas y las conclusiones corresponden a la Parte 7.

### Parte 5 — LaTeX, diseño de la solución y manual técnico
**Responsable:** Óscar Iván Ulises Gutiérrez Palacios
**Archivos:** `Entregable.tex`, `Manuales.tex` (estructura + manual técnico), `secciones/04-diseno.tex`, `.github/workflows/`
**Recibe de las Partes 3 y 4:** el programa final, los scripts y su forma de uso.

- [ ] **Se puede adelantar desde el principio:** reorganizar `Entregable.tex` para que incluya `secciones/*.tex` con `\input`, con bibliografía (`referencias.bib`) y paquete para código (`listings` o `minted`). Quitar el texto de instrucciones de la plantilla, poner el título y los autores.
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
**Responsables:** Daniel Alonso Gracia Pinto (análisis, conclusiones y recomendaciones) · Óscar Iván Ulises Gutiérrez Palacios (revisión final y correo de entrega)
**Archivos:** `secciones/06-experimentacion.tex`, `secciones/07-conclusiones.tex`
**Recibe de:** la Parte 1 (pregunta e hipótesis), la Parte 4 (resultados y figuras) y las Partes 5 y 6 (informe y manuales).

Es la **última parte** de la cadena: recoge todo y termina con el envío.

- [x] Sección **Experimentación y análisis de resultados**: descripción de cada escenario, tabla de parámetros, tablas con medias e IC 95 %, figuras de la Parte 4 e interpretación de E0–E3. *`secciones/06-experimentacion.tex`; la tabla se verificó número a número contra `resultados/tabla_estadistica.csv`.*
- [x] Comparar E1 vs. E2 (la pregunta central) y discutir el efecto de la velocidad. *Incluye HS1–HS3, con pruebas de Welch E3 vs. E2 y 1 vs. 20 m/s calculadas sobre `resultados/resumen.csv`.*
- [x] **Conclusiones**: ¿se acepta o se rechaza la hipótesis de la Parte 1? ¿con qué evidencia? *Se acepta: PDR 4/4 y retardo 3/4 velocidades (`secciones/07-conclusiones.tex`).*
- [x] **Recomendaciones**: limitaciones del estudio y trabajo futuro (más nodos, otros protocolos como OLSR/DSDV, RPGM, ns3-ai).
- [ ] *(Óscar)* Revisión final del PDF completo (coherencia entre secciones, ortografía, figuras referenciadas) y verificar que `Entregable.pdf` y `Manuales.pdf` compilan.
- [ ] *(Óscar)* Redactar y enviar el **correo de entrega** (sección 6).

> Para que las secciones 06 y 07 aparezcan en el PDF, `Entregable.tex` (Parte 5) debe incluirlas con `\input{secciones/06-experimentacion.tex}` y `\input{secciones/07-conclusiones.tex}`.

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

- Nombres completos de los 6 integrantes y su rol (tabla de la sección 4).
- Adjuntos: `Entregable.pdf`, `Manuales.pdf`, código fuente (`simulacion/` y `scripts/` en un `.zip`) y el enlace al repositorio.
