# User Stories - Selector de Fondos de Pantalla (Wayland Native / C)

**Contexto del Proyecto:**
* **Lenguaje:** C, Wayland Protocols (Nativo), Hyprland en Arch Linux.
* **MVP:** Un carrusel con previsualización de fondos de pantalla que se seleccionan al hacer clic.

---

### US-001: Mostrar una imagen inicial al abrir la aplicación
**Como** usuario,  
**Quiero** ver una imagen en pantalla al lanzar el programa,  
**Para** verificar que la previsualización se está desplegando correctamente.

#### Criterios de Aceptación:
- [X] Criterio principal:
    - **Dado que** ejecuto el programa desde mi entorno,
    - **Cuando** la aplicación se inicia correctamente,
    - **Entonces** debe desplegarse una ventana en pantalla mostrando la imagen cargada por defecto sin errores visuales ni cierres inesperados.

---

### US-002: Desplegar la ventana mediante un atajo de teclado en Hyprland
**Como** usuario,  
**Quiero** abrir el selector de fondos al presionar una combinación de teclas,  
**Para** acceder a la herramienta rápidamente en cualquier momento sin usar la terminal.

#### Criterios de Aceptación:
- [X] Activación por atajo:
    - **Dado que** estoy usando el sistema y presiono la combinación de teclas configurada,
    - **Cuando** se detecta el atajo de teclado,
    - **Entonces** el selector se debe superponer sobre mis ventanas actuales de forma fluida.
- [X] Enfoque de la ventana:
    - **Dado que** el selector se ha desplegado en pantalla,
    - **Cuando** la interfaz está lista,
    - **Entonces** el cursor/foco debe quedar listo para interactuar con la aplicación.

---

### US-003: Cerrar la ventana al presionar Enter o hacer clic
**Como** usuario,  
**Quiero** cerrar la ventana inmediatamente presionando `Enter` o haciendo clic,  
**Para** salir de la previsualización cuando ya no la necesite.

#### Criterios de Aceptación:
- [X] Cierre interactivo:
    - **Dado que** la ventana de previsualización está visible en pantalla,
    - **Cuando** hago clic sobre la imagen o presiono la tecla `Enter`,
    - **Entonces** la aplicación debe cerrarse de inmediato y liberar la pantalla.
> [!NOTE]
> Se está leyendo solo el `Enter`, no el clic.

---

### US-004: Cargar automáticamente las imágenes de una carpeta
**Como** usuario,  
**Quiero** que el programa detecte todas las imágenes dentro de una carpeta y seleccione la primera,  
**Para** no tener que especificar archivo por archivo manualmente.

#### Criterios de Aceptación:
- [X] Carga inicial de directorio:
    - **Dado que** existe una carpeta con varios archivos de imagen,
    - **Cuando** abro el selector apuntando a esa carpeta,
    - **Entonces** la aplicación debe cargar la primera imagen encontrada en la lista automáticamente.
- [X] Filtrado de archivos:
    - **Dado que** la carpeta contiene archivos mixtos,
    - **Cuando** el programa escanea el directorio,
    - **Entonces** debe ignorar los archivos que no sean imágenes sin fallar.
> [!NOTE]
> Se están usando solo .ppm por ahora

---

### US-005: Especificar la ruta de la carpeta desde Hyprland / Línea de comandos
**Como** usuario,  
**Quiero** pasar la ruta de mi carpeta de fondos de pantalla como parámetro al comando,  
**Para** cambiar de colección de imágenes según mi configuración en Hyprland.

#### Criterios de Aceptación:
- [ ] Parámetro de ruta válido:
    - **Dado que** ejecuto el comando indicando una ruta de directorio válida (ej. `./selector /ruta/a/fondos`),
    - **Cuando** la aplicación inicia,
    - **Entonces** debe cargar e iterar únicamente sobre las imágenes pertenecientes a esa carpeta.
- [ ] Manejo de errores de ruta:
    - **Dado que** indico una ruta inexistente o vacía,
    - **Cuando** la aplicación intenta iniciar,
    - **Entonces** debe mostrar un mensaje comprensible y cerrarse de forma segura.

---

### US-006: Navegar entre imágenes usando el Scroll
**Como** usuario,  
**Quiero** cambiar de imagen desplazando la rueda del ratón (*scroll*),  
**Para** explorar y comparar rápidamente los diferentes fondos disponibles.

#### Criterios de Aceptación:
- [ ] Navegación por desplazamiento:
    - **Dado que** la previsualización de una imagen está en pantalla,
    - **Cuando** hago *scroll* hacia abajo o hacia arriba con el ratón,
    - **Entonces** la pantalla debe actualizarse mostrando la siguiente o anterior imagen de la carpeta.
- [ ] Comportamiento en los extremos:
    - **Dado que** alcanzo el final o el inicio de la lista de imágenes,
    - **Cuando** continúo haciendo scroll,
    - **Entonces** la navegación debe ser cíclica o detenerse de forma fluida.

---

### US-007: Previsualización con nivel de transparencia / opacidad
**Como** usuario,  
**Quiero** ver la imagen con una transparencia sutil respecto a mi escritorio,  
**Para** apreciar cómo se integrará el fondo con mi entorno actual antes de aplicarlo.

#### Criterios de Aceptación:
- [ ] Efecto visual de opacidad:
    - **Dado que** el selector está superpuesto en la pantalla,
    - **Cuando** estoy observando la previsualización,
    - **Entonces** la imagen debe mostrar un nivel de transparencia/opacidad equilibrado que me permita ver el fondo y la distribución general del escritorio.

---

### US-008: Aplicar el fondo seleccionado al hacer clic
**Como** usuario,  
**Quiero** que al hacer clic sobre una imagen esta se establezca como mi nuevo fondo de pantalla y la ventana se cierre,  
**Para** confirmar mi elección en un solo paso.

#### Criterios de Aceptación:
- [ ] Aplicación y cierre:
    - **Dado que** estoy navegando por las imágenes y encuentro una que me gusta,
    - **Cuando** hago clic directamente sobre la imagen elegida,
    - **Entonces** el sistema debe actualizar el fondo de pantalla del escritorio con dicha imagen,
    - **Y** la ventana del carrusel/selector debe cerrarse automáticamente tras aplicar el cambio.