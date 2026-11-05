# Correo de entrega — Taller No. 01

> Lo envía **Óscar Iván Ulises Gutiérrez Palacios** desde su **correo institucional**, respondiendo el correo del enunciado **únicamente al profesor** (sin "responder a todos"), antes de las **11:59 pm del 4 de noviembre de 2026**.

**Para:** Jorge Eduardo Ortiz Triviño (solo el profesor)
**Asunto:** RE: ME01: Enunciado y entrega del Taller No 01 Simulación de MANET

---

Profesor Jorge Eduardo Ortiz Triviño, buenas noches.

En respuesta a este mensaje, el grupo entrega el Taller No. 01 "Simulación de MANETs". El trabajo responde la pregunta de investigación: *en una MANET jerárquica de dos niveles con enrutamiento AODV, ¿qué degrada más el desempeño de la red, la movilidad a nivel de clúster o la movilidad a nivel de nodo, y cómo cambia ese efecto al aumentar la velocidad?* Para ello se simuló en ns-3.43 una red de 20 nodos, con 4 clústeres en el primer nivel y 2 superclústeres en el segundo, y movimiento tanto a nivel de clúster como de nodo. Se analizaron 4 escenarios mediante 130 corridas.

**Integrantes y roles**

1. **David Fernando Benjumea Mora:** formulación científica (problema, pregunta, hipótesis y diseño experimental) y red base en ns-3.
2. **Daniel Alonso Gracia Pinto:** movilidad jerárquica en tres capas y su validación, y análisis de resultados, conclusiones y recomendaciones.
3. **Stephan Alain Roland García Martiquet:** generación de tráfico, métricas con FlowMonitor y sección de código fuente.
4. **José Esteban Alvarado Monroy:** automatización de los experimentos, procesamiento estadístico y gráficas.
5. **Óscar Iván Ulises Gutiérrez Palacios:** estructura del documento en LaTeX, diseño de la solución, manual técnico, revisión final y entrega.
6. **John Jairo Páez Albino:** marco teórico, referencias bibliográficas y manual de usuario.

**Retiro de un integrante**

Le informamos que Daniel Santiago Rodríguez Rodríguez, inscrito inicialmente en el grupo, fue retirado por no participar en el desarrollo del taller. No asumió la parte que se le asignó ni respondió a las comunicaciones del grupo. La decisión se tomó mediante una encuesta entre los integrantes, cuyo resultado adjuntamos como imagen. Sus tareas fueron asumidas por Daniel Gracia y Óscar Gutiérrez, y su nombre no figura como autor en los documentos entregados.

**Adjuntos**

- `Entregable.pdf`: informe con el marco teórico, la descripción y justificación del problema, el diseño de la solución, el código fuente, la experimentación y el análisis de resultados, las conclusiones y las recomendaciones.
- `Manuales.pdf`: manual técnico y manual de usuario.
- `codigo-fuente.zip`: programa de simulación (`simulacion/`) y scripts de experimentación, análisis y validación (`scripts/`).
- `encuesta-retiro.png`: resultado de la encuesta del grupo.

Repositorio con el código, los resultados y el historial de trabajo: https://github.com/10scar/tallerme01

**Software utilizado:** ns-3.43, compilado con g++ 11.4 y CMake 3.22 sobre Ubuntu 22.04. No se usó ns3-ai, que el enunciado señala como opcional; la justificación está en la sección de código fuente del informe. Para procesar los resultados se usó Python con las librerías pandas, matplotlib y scipy, solo para el análisis estadístico y las gráficas, no para resolver el problema planteado.

Quedamos atentos a sus comentarios y a la programación de la sustentación.

Cordialmente,

Óscar Iván Ulises Gutiérrez Palacios, en nombre del grupo

---

## Lista de verificación antes de enviar

- [ ] Adjuntar la imagen de la encuesta (`encuesta-retiro.png`). Si se quiere, agregar el resultado al texto (por ejemplo, "por unanimidad, 6 de 6"), solo si así fue.
- [ ] Generar el `.zip` del código desde la raíz del repositorio:
  ```bash
  zip -r codigo-fuente.zip simulacion/ scripts/
  ```
- [ ] Adjuntar `Entregable.pdf` y `Manuales.pdf` (están en la raíz del repositorio).
- [ ] Verificar el acceso al repositorio: si es privado, invitar al profesor como colaborador o quitar el enlace.
- [ ] Enviar desde el correo institucional, respondiendo **solo** al profesor.
- [ ] Enviar antes de las **11:59 pm**.
