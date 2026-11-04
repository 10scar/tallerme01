#!/usr/bin/env bash
# Parte 4 — Ejecutar el diseño experimental del README (130 simulaciones)
set -Eeuo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NS3_DIR="${NS3_DIR:-$HOME/simulaciones/ns-allinone-3.43/ns-3.43}"
REPS=10
SIM_TIME=200
PILOTO=0
DRY_RUN=0
FORZAR=0
COPIAR=1

ayuda() {
  cat <<'EOF'
Uso: bash scripts/correr_escenarios.sh [opciones]
  --ns3 RUTA          Directorio que contiene ./ns3
  --repeticiones N    Réplicas por configuración (predeterminado: 10)
  --piloto            Ejecuta 4 pruebas: E0 y E1/E2/E3 a 5 m/s, run 1
  --dry-run           Muestra comandos, no ejecuta ni modifica archivos
  --forzar            Repite incluso si existe un XML previo
  --sin-copiar        No copia simulacion/manet-jerarquica.cc a scratch/
  --ayuda             Muestra esta ayuda
Variables de entorno:
  NS3_DIR             Ruta ns-3.43 (alternativa a --ns3)
  NS3_PYTHON          Python para el lanzador ./ns3 (opcional; p.ej. ~/.local/bin/python3.12)
EOF
}

while (( $# )); do
  case "$1" in
    --ns3) [[ $# -ge 2 ]] || { echo 'Falta ruta tras --ns3' >&2; exit 2; }; NS3_DIR="$2"; shift 2 ;;
    --repeticiones) [[ $# -ge 2 ]] || { echo 'Falta N tras --repeticiones' >&2; exit 2; }; REPS="$2"; shift 2 ;;
    --piloto) PILOTO=1; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    --forzar) FORZAR=1; shift ;;
    --sin-copiar) COPIAR=0; shift ;;
    -h|--help|--ayuda) ayuda; exit 0 ;;
    *) echo "Opción desconocida: $1" >&2; ayuda >&2; exit 2 ;;
  esac
done

[[ "$REPS" =~ ^[1-9][0-9]*$ ]] || { echo 'Las repeticiones deben ser enteras positivas' >&2; exit 2; }
NS3_DIR="$(cd "$NS3_DIR" 2>/dev/null && pwd)" || { echo 'No existe el directorio de ns-3' >&2; exit 2; }
[[ -f "$NS3_DIR/ns3" ]] || { echo "No está ./ns3 en $NS3_DIR" >&2; exit 2; }
[[ -f "$REPO/simulacion/manet-jerarquica.cc" ]] || { echo 'Falta simulacion/manet-jerarquica.cc en el repo' >&2; exit 2; }

if [[ -n "${NS3_PYTHON:-}" ]]; then
  PYTHON_NS3="$NS3_PYTHON"
elif [[ -x "$HOME/.local/bin/python3.12" ]]; then
  PYTHON_NS3="$HOME/.local/bin/python3.12"
else
  PYTHON_NS3="python3"
fi

RAW="$REPO/resultados/raw"
LOGS="$REPO/resultados/logs"
if (( !DRY_RUN )); then
  mkdir -p "$RAW" "$LOGS" "$NS3_DIR/scratch"
  if (( COPIAR )); then
    cp "$REPO/simulacion/manet-jerarquica.cc" "$NS3_DIR/scratch/manet-jerarquica.cc"
    echo "Código sincronizado: simulacion/manet-jerarquica.cc → scratch/"
  fi
fi

TOTAL=$(( 13 * REPS ))
if (( PILOTO )); then TOTAL=4; fi
N=0; OK=0; OMITIDOS=0

correr() {
  local escenario="$1" cs="$2" ns="$3" rep="$4"
  local nombre="${escenario}_cs${cs}_ns${ns}_run${rep}"
  local xml="$RAW/${nombre}.xml"
  local parametros="scratch/manet-jerarquica --scenario=${escenario} --clusterSpeed=${cs} --nodeSpeed=${ns} --superSpeed=0 --simTime=${SIM_TIME} --RngRun=${rep} --outDir=${RAW}"
  N=$((N+1))
  if (( DRY_RUN )); then
    printf '[%03d/%03d] %s\n' "$N" "$TOTAL" "$parametros"
    return 0
  fi
  if (( !FORZAR )) && [[ -s "$xml" ]]; then
    printf '[%03d/%03d] OMITIDO (XML existente): %s\n' "$N" "$TOTAL" "$nombre"
    OMITIDOS=$((OMITIDOS+1))
    return 0
  fi
  printf '[%03d/%03d] Ejecutando %s ... ' "$N" "$TOTAL" "$nombre"
  if (cd "$NS3_DIR" && "$PYTHON_NS3" ./ns3 run "$parametros") >"$LOGS/${nombre}.log" 2>&1; then
    if [[ -s "$xml" ]]; then
      echo 'OK'
      OK=$((OK+1))
    else
      echo 'ERROR: no se generó XML' >&2
      tail -40 "$LOGS/${nombre}.log" >&2
      exit 1
    fi
  else
    echo 'ERROR de simulación' >&2
    tail -50 "$LOGS/${nombre}.log" >&2
    exit 1
  fi
}

if (( PILOTO )); then
  correr E0 0 0 1
  correr E1 0 5 1
  correr E2 5 0 1
  correr E3 5 5 1
else
  for (( r=1; r<=REPS; r++ )); do correr E0 0 0 "$r"; done
  for e in E1 E2 E3; do
    for v in 1 5 10 20; do
      for (( r=1; r<=REPS; r++ )); do
        case "$e" in
          E1) correr E1 0 "$v" "$r" ;;
          E2) correr E2 "$v" 0 "$r" ;;
          E3) correr E3 "$v" "$v" "$r" ;;
        esac
      done
    done
  done
fi
if (( DRY_RUN )); then
  echo "Plan validado: $N comandos (${TOTAL} esperados)."
else
  echo "Fin: $N configuraciones; ejecutadas $OK; existentes $OMITIDOS."
  echo "XML: $RAW"
  echo 'Siguiente: python3 scripts/parse_flowmon.py'
fi
