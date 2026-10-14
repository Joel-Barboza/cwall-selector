
Se intentó seguir el tutorial Writting Wayland Clients[^tuto], pero usaba wl_shell y no funcionaba

Y viendo la documentación en The Wayland Protocol [^protocol], se notó que [`wl_shell`](https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_shell) 
está obsoleto y sustituido por `xdg_shell`

En Wayland Explorer, se encontró información sobre [`xdg_shell`](https://wayland.app/protocols/xdg-shell)

Del video de YouTube tomado como referencia [^main_video] usamos:
```
wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.c
wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h
gcc main.c xdg-shell-protocol.c -l wayland-client -o main
```

Al final, se notó que en video se estaban usando muchas instrucciones que se salían un poco
de lo primordial, por lo cual en el Wayland Book [^book], revisamos la sección de [XDG shell basics](https://wayland-book.com/xdg-shell-basics.html)
y se tomó el ejemplo de código de la sección [Extended example code](https://wayland-book.com/xdg-shell-basics/example-code.html)
como base. Este crea una ventana con un patrón de tablero.

Importante notar que luego, en Extended example code se dieron los comandos usados en el video, pero un poco diferentes
```
wayland-scanner private-code \
  < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  > xdg-shell-protocol.c
wayland-scanner client-header \
  < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  > xdg-shell-client-protocol.h
cc -o client client.c xdg-shell-protocol.c -lwayland-client -lrt
```
> [!NOTE]
> Se decidió usar los comandos del video


Revisar 

[fstat](https://linux.die.net/man/2/fstat)

[ejemplo hello wayland](https://gitlab.com/hdante/hello_wayland)

[the hello world wayland blog](https://hdante.wordpress.com/2014/07/08/the-hello-wayland-tutorial/)

Buscando como leer los colores de las imágenes para pasarlas al shm, se encontró en
StackOverflow una discusión iniciada en 2021 que resultó útil, [How to read images using C](https://stackoverflow.com/questions/69581117/how-to-read-images-using-c).

> [!NOTE]
> Se decidió usar en las versiones iniciales del proyecto el formato de imágenes .ppm

Esto ayudo a leer los valores del .ppm [Calculate average RGB value of a PPM file](https://stackoverflow.com/questions/55347596/calculate-average-rgb-value-of-a-ppm-file)

Con esto se llegó al punto de tener una ventana con una imagen. La imagen mantiene sus dimensiones en 
píxeles(3520x1984), mientras que la ventana puede cambiar de tamaño
([Imagen de ejemplo](images/showcase_primera_imagen.png)). 


# Referencias consultadas

[^book]: [Wayland book](https://wayland.freedesktop.org/docs/book/)

[^protocol]: [The Wayland Protocol](https://wayland.freedesktop.org/docs/html/index.html)

[xdg_shell](https://wayland.app/protocols/xdg-shell)

[^tuto]: [Writing Wayland Clients](https://bugaevc.gitbooks.io/writing-wayland-clients/content/)

[^main_video]: [zero to window - wayland client (YouTube)](https://youtu.be/iIVIu7YRdY0?si=T4jjiITBwp_5KDXr)

[Criterios de aceptación: ejemplos para elaborarlos](https://www.scrummanager.com/blog/2023/03/criterios-de-aceptacion-definicion-y-ejemplos/)

[Sintaxis básica de redacción y formato](https://docs.github.com/es/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax)


